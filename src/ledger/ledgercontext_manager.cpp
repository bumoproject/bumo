/*
	bumo is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	bumo is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with bumo.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ledgercontext_manager.h"
#include "ledger_manager.h"
#include <contract/contract_manager.h>

namespace bumo {
	//For synchronizing blocks.
	LedgerContext::LedgerContext(const std::string &chash, const protocol::ConsensusValue &consvalue) :
		type_(AT_NORMAL),
		lpmanager_(NULL),
		hash_(chash),
		consensus_value_(consvalue),
		start_time_(-1),
		tx_timeout_(-1),
		timeout_tx_index_(-1),
		apply_mode_(LedgerFrm::APPLY_MODE_FOLLOW) {
		closing_ledger_ = std::make_shared<LedgerFrm>();
	}


	//For synchronizing the block before the current block.
	LedgerContext::LedgerContext(LedgerContextManager *lpmanager, const std::string &chash, const protocol::ConsensusValue &consvalue, bool propose) :
		type_(AT_NORMAL),
		lpmanager_(lpmanager),
		hash_(chash),
		consensus_value_(consvalue),
		start_time_(-1),
		timeout_tx_index_(-1){
		apply_mode_ = propose ? LedgerFrm::APPLY_MODE_PROPOSE : LedgerFrm::APPLY_MODE_CHECK;
		closing_ledger_ = std::make_shared<LedgerFrm>();
	}

	//for test
	LedgerContext::LedgerContext(
		int32_t type,
		const ContractTestParameter &parameter) :
		type_(type), 
		parameter_(parameter),
		lpmanager_(NULL) {
		apply_mode_ = LedgerFrm::APPLY_MODE_PROPOSE;
		closing_ledger_ = std::make_shared<LedgerFrm>();
	}
	LedgerContext::LedgerContext(
		int32_t type,
		const protocol::ConsensusValue &consensus_value,
		int64_t timeout) :
		type_(type),
		consensus_value_(consensus_value),
		lpmanager_(NULL),
		tx_timeout_(timeout) {
		apply_mode_ = LedgerFrm::APPLY_MODE_PROPOSE;
		closing_ledger_ = std::make_shared<LedgerFrm>();
	}
	LedgerContext::~LedgerContext() {}

	void LedgerContext::Run() {
		LOG_INFO("Preprocessing the consensus value, ledger(" FMT_I64 ")", consensus_value_.ledger_seq());
		start_time_ = utils::Timestamp::HighResolution();
		switch (type_)
		{
		case AT_NORMAL:
			Do();
			break;
		case AT_TEST_V8:
			TestV8();
			break;
		case AT_TEST_EVM:
			LOG_ERROR("Test evm not support");
			break;
		case AT_TEST_TRANSACTION:
			TestTransaction();
			break;
		default:
			LOG_ERROR("Action type unknown of LedgerContext.");
			break;
		}
	}

	void LedgerContext::Do() {
		protocol::Ledger& ledger = closing_ledger_->ProtoLedger();
		auto header = ledger.mutable_header();
		header->set_seq(consensus_value_.ledger_seq());
		header->set_close_time(consensus_value_.close_time());
		header->set_previous_hash(consensus_value_.previous_ledger_hash());
		header->set_consensus_value_hash(hash_);
		header->set_chain_id(General::GetSelfChainId());
		header->set_version(LedgerManager::Instance().GetLastClosedLedger().version());
		LedgerManager::Instance().tree_->time_ = 0;
		if (apply_mode_ == LedgerFrm::APPLY_MODE_PROPOSE) {
			propose_result_.exec_result_ = closing_ledger_->ApplyPropose(consensus_value_, this, propose_result_);
		}
		else if (apply_mode_ == LedgerFrm::APPLY_MODE_CHECK) {
			propose_result_.exec_result_ = closing_ledger_->ApplyCheck(consensus_value_, this);
		}
		else {
			propose_result_.exec_result_ = closing_ledger_->ApplyFollow(consensus_value_, this);
		}

		//async
		if (lpmanager_) {
			//Move the finished transactions to the complete list.
			if (propose_result_.exec_result_) {
				lpmanager_->MoveRunningToComplete(this);
			}
			else { //delete
				lpmanager_->MoveRunningToDelete(this);
			}
		}
	}

	bool LedgerContext::TestV8() {
		//If the source address for starting the contract does not exist, a temporary account will be created.
		std::shared_ptr<Environment> environment = std::make_shared<Environment>();
		if (parameter_.contract_address_.empty()) {
			//Create a temporary account
			PrivateKey priv_key(SIGNTYPE_ED25519);
			Json::Value account_json = Json::Value(Json::objectValue);
			protocol::Account account;
			account.set_balance(parameter_.contract_balance_);
			account.set_address(priv_key.GetEncAddress());
			account.mutable_contract()->set_payload(parameter_.code_);
			account.mutable_contract()->set_type((protocol::Contract_ContractType)type_);
			parameter_.contract_address_ = account.address();
			std::shared_ptr<AccountFrm> dest_account = std::make_shared<AccountFrm>(account);
			if (!environment->AddEntry(dest_account->GetAccountAddress(), dest_account)) {
				LOG_ERROR("Failed to add account(%s) entry.", account.address().c_str());
				propose_result_.exec_result_ = false;
				return false;
			}
		}

		AccountFrm::pointer null_acc;
		if (!Environment::AccountFromDB(parameter_.source_address_, null_acc)) {
			if (!PublicKey::IsAddressValid(parameter_.source_address_)) {
				PrivateKey priv_key(SIGNTYPE_ED25519);
				parameter_.source_address_ = priv_key.GetEncAddress();
			}
			//Create a temporary source address
			protocol::Account account;
			account.set_address(parameter_.source_address_);
			account.set_nonce(0);
			account.set_balance(100000000000000000);
			std::shared_ptr<AccountFrm> dest_account = std::make_shared<AccountFrm>(account);
			dest_account->SetProtoMasterWeight(1);
			dest_account->SetProtoTxThreshold(1);
			if (!environment->AddEntry(dest_account->GetAccountAddress(), dest_account)) {
				LOG_ERROR("Failed to add account(%s) entry.", account.address().c_str());
				propose_result_.exec_result_ = false;
				return false;
			}
		}

		protocol::LedgerHeader lcl = LedgerManager::Instance().GetLastClosedLedger();
		consensus_value_.set_ledger_seq(lcl.seq() + 1);
		consensus_value_.set_close_time(lcl.close_time() + 1);

		if (ContractTestParameter::INIT == parameter_.opt_type_){
			protocol::TransactionEnv env;
			protocol::Transaction *tx = env.mutable_transaction();
			tx->set_source_address(parameter_.source_address_);
			tx->set_fee_limit(parameter_.fee_limit_);
			tx->set_gas_price(parameter_.gas_price_);
			protocol::Operation *ope = tx->add_operations();
			ope->set_type(protocol::Operation_Type_CREATE_ACCOUNT);

			protocol::OperationCreateAccount* create_account = ope->mutable_create_account();
			create_account->set_init_input(parameter_.input_);
			create_account->set_init_balance(100000000000000);
			protocol::AccountPrivilege *priv = create_account->mutable_priv();
			priv->set_master_weight(0);
			priv->mutable_thresholds()->set_tx_threshold(1);
			create_account->mutable_contract()->set_payload(parameter_.code_);
			create_account->mutable_contract()->set_type((protocol::Contract_ContractType)type_);

			TransactionFrm::pointer tx_frm = std::make_shared<TransactionFrm>(env);
			tx_frm->environment_ = environment;
			int64_t time_now = utils::Timestamp::HighResolution();
			tx_frm->SetApplyStartTime(time_now);
			tx_frm->SetMaxEndTime(time_now + 5 * utils::MICRO_UNITS_PER_SEC);
			tx_frm->EnableChecked();

			transaction_stack_.push_back(tx_frm);
			closing_ledger_->apply_tx_frms_.push_back(tx_frm);

			closing_ledger_->value_ = std::make_shared<protocol::ConsensusValue>(consensus_value_);
			closing_ledger_->lpledger_context_ = this;

			bool ret = LedgerManager::Instance().DoTransaction(env, this).code() == 0;
			tx_frm->SetApplyEndTime(utils::Timestamp::HighResolution());
			return ret;
		}
		else if (ContractTestParameter::MAIN == parameter_.opt_type_) {
			//Construct trigger tx
			protocol::TransactionEnv env;
			protocol::Transaction *tx = env.mutable_transaction();
			tx->set_source_address(parameter_.source_address_);
			tx->set_fee_limit(parameter_.fee_limit_);
			tx->set_gas_price(parameter_.gas_price_);
			protocol::Operation *ope = tx->add_operations();
			ope->set_type(protocol::Operation_Type_PAY_ASSET);
			protocol::OperationPayAsset *payAsset = ope->mutable_pay_asset();
			payAsset->set_dest_address(parameter_.contract_address_);
			payAsset->set_input(parameter_.input_);

			TransactionFrm::pointer tx_frm = std::make_shared<TransactionFrm>(env);
			tx_frm->environment_ = environment;
			int64_t time_now = utils::Timestamp::HighResolution();
			tx_frm->SetApplyStartTime(time_now);
			tx_frm->SetMaxEndTime(time_now + 5 * utils::MICRO_UNITS_PER_SEC);
			tx_frm->EnableChecked();

			transaction_stack_.push_back(tx_frm);
			closing_ledger_->apply_tx_frms_.push_back(tx_frm);

			closing_ledger_->value_ = std::make_shared<protocol::ConsensusValue>(consensus_value_);
			closing_ledger_->lpledger_context_ = this;

			bool ret = LedgerManager::Instance().DoTransaction(env, this).code() == 0;
			tx_frm->SetApplyEndTime(utils::Timestamp::HighResolution());
			return ret;
		}
		else if(ContractTestParameter::QUERY == parameter_.opt_type_){
			if (parameter_.code_.empty() ){
				LOG_ERROR("Failed to check contract code.");
				return false;
			} 

			ContractParameter parameter;
			//contract
			parameter.code_ = parameter_.code_;
			parameter.init_ = false;
			parameter.input_ = parameter_.input_;
			parameter.ledger_context_ = this;
			parameter.this_address_ = parameter_.contract_address_;

			//blcok
			parameter.block_.number_ = consensus_value_.ledger_seq();
			parameter.block_.timestamp_ = consensus_value_.close_time();

			//tx
			parameter.tx_.Reset();
			parameter.tx_.initiator_ = parameter_.source_address_;
			parameter.tx_.sender_ = parameter_.source_address_;
			parameter.tx_.gas_price_ = parameter_.gas_price_;
			parameter.tx_.fee_limit_ = parameter_.fee_limit_;

			//for msg
			parameter.msg_.Reset();
			parameter.msg_.initiator_ = parameter_.source_address_;
			parameter.msg_.sender_ = parameter_.source_address_;

			//Query
			TransactionFrm::pointer tx_frm = std::make_shared<TransactionFrm>();
			tx_frm->environment_ = environment;
			transaction_stack_.push_back(tx_frm);
			int64_t time_now = utils::Timestamp::HighResolution();
			tx_frm->SetApplyStartTime(time_now);
			tx_frm->SetMaxEndTime(time_now + 5 * utils::MICRO_UNITS_PER_SEC);
			tx_frm->EnableChecked();

			Json::Value query_result;
			bool ret = ContractManager::Instance().Query(type_, parameter, query_result);
			tx_frm->SetApplyEndTime(utils::Timestamp::HighResolution());
			return ret;
		}
		else {
			return false;
		}
	}

	void LedgerContext::Cancel() {
		std::stack<int64_t> copy_stack;
		do {
			utils::MutexGuard guard(lock_);
			copy_stack = contract_ids_;
		} while (false);

		while (!copy_stack.empty()) {
			int64_t cid = copy_stack.top();
			ContractManager::Instance().Cancel(cid);
			copy_stack.pop();
		}

		JoinWithStop();
	}

	bool LedgerContext::CheckExpire(int64_t total_timeout) {
		return utils::Timestamp::HighResolution() - start_time_ >= total_timeout;
	}

	void LedgerContext::PushLog(const std::string &address, const utils::StringList &logs) {
		Json::Value &item = logs_[utils::String::Format(FMT_SIZE "-%s", logs_.size(), address.c_str())];
		for (utils::StringList::const_iterator iter = logs.begin(); iter != logs.end(); iter++) {
			item[item.size()] = *iter;
		}
	}

	std::shared_ptr<TransactionFrm> LedgerContext::GetBottomTx() {
		return transaction_stack_.front();
	}

	std::shared_ptr<TransactionFrm> LedgerContext::GetTopTx() {
		if (transaction_stack_.size() > 0) {
			return transaction_stack_.back();
		} else{
			return NULL;
		}
	}

	void LedgerContext::GetLogs(Json::Value &logs) {
		logs = logs_;
	}

	void LedgerContext::PushRet(const std::string &address, const Json::Value &ret) {
		rets_[rets_.size()] = ret;
	}

	void LedgerContext::GetRets(Json::Value &rets) {
		rets = rets_;
	}

	void LedgerContext::PushContractId(int64_t id) {
		utils::MutexGuard guard(lock_);
		contract_ids_.push(id);
	}

	void LedgerContext::PopContractId() {
		utils::MutexGuard guard(lock_);
		contract_ids_.pop();
	}

	int64_t LedgerContext::GetTopContractId() {
		utils::MutexGuard guard(lock_);
		if (!contract_ids_.empty()) {
			return contract_ids_.top();
		}

		return -1;
	}

	std::string LedgerContext::GetHash() {
		return hash_;
	}

	int32_t LedgerContext::GetTxTimeoutIndex() {
		return timeout_tx_index_;
	}

	bool LedgerContext::TestTransaction() {
		protocol::LedgerHeader lcl = LedgerManager::Instance().GetLastClosedLedger();
		consensus_value_.set_ledger_seq(lcl.seq() + 1);
		consensus_value_.set_close_time(lcl.close_time() + 1);

		closing_ledger_->SetTestMode(true);
		propose_result_.exec_result_ = closing_ledger_->ApplyPropose(consensus_value_, this, propose_result_);
		return propose_result_.exec_result_;
	}

	LedgerContextManager::LedgerContextManager() {
		check_interval_ = 10 * utils::MICRO_UNITS_PER_MILLI;
	}
	LedgerContextManager::~LedgerContextManager() {
	}

	void LedgerContextManager::Initialize() {
		TimerNotify::RegisterModule(this);
	}

	int32_t LedgerContextManager::CheckComplete(const std::string &chash) {
		do {
			utils::MutexGuard guard(ctxs_lock_);
			LedgerContextMap::iterator iter = completed_ctxs_.find(chash);
			if (iter != completed_ctxs_.end()) {
				return iter->second->propose_result_.exec_result_ ? 1 : 0;
			}
		} while (false);

		return -1;
	}

	LedgerFrm::pointer LedgerContextManager::SyncProcess(const protocol::ConsensusValue& consensus_value) {
		std::string con_str = consensus_value.SerializeAsString();
		std::string chash = HashWrapper::Crypto(con_str);
		do {
			utils::MutexGuard guard(ctxs_lock_);
			LedgerContextMap::iterator iter = completed_ctxs_.find(chash);
			if (iter != completed_ctxs_.end()) {
				return iter->second->closing_ledger_;
			}
		} while (false);

		LOG_TRACE("Sync processing the consensus value, ledger seq(" FMT_I64 ")", consensus_value.ledger_seq());
		LedgerContext ledger_context(chash, consensus_value);
		ledger_context.Do();
		if (ledger_context.propose_result_.exec_result_) {
			return ledger_context.closing_ledger_;
		}
		else {
			LOG_ERROR("Failed to process ledger synchronized.");
			return NULL;
		}
	}

	bool LedgerContextManager::SyncTestProcess(LedgerContext::ACTION_TYPE type,
		TestParameter *parameter, 
		int64_t total_timeout, 
		Result &result, 
		Json::Value &logs,
		Json::Value &txs,
		Json::Value &rets,
		Json::Value &stat,
		int32_t signature_number) {
		LedgerContext *ledger_context = nullptr;
		std::string thread_name = "test";
		if (type == LedgerContext::AT_TEST_V8){
			thread_name = "test-contract";
			ledger_context = new LedgerContext(type, *((ContractTestParameter*)parameter));

			do {
				//check syntax error
				std::string code = ((ContractTestParameter*)parameter)->code_;
				if (code.empty()) {
					break;
				}

				ContractTestParameter::OptType opt_type = ((ContractTestParameter*)parameter)->opt_type_;
				if (opt_type > ContractTestParameter::QUERY || opt_type < ContractTestParameter::INIT){
					result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
					result.set_desc(utils::String::Format("opt_type overload:%d", opt_type));
					delete ledger_context;
					return false;
				}

				result = ContractManager::Instance().SourceCodeCheck(Contract::TYPE_V8, code, ledger_context->transaction_stack_.size());
				if (result.code() == protocol::ERRCODE_SUCCESS) {
					break;
				}
				delete ledger_context;
				return false;
			} while (false);
		}
		else if (type == LedgerContext::AT_TEST_TRANSACTION){
			thread_name = "test-transaction";
			ledger_context = new LedgerContext(type, ((TransactionTestParameter*)parameter)->consensus_value_, total_timeout);
		}
		else {
			LOG_ERROR("Test type(%d) error",type);
			return false;
		}

		if (!ledger_context->Start(thread_name)) {
			LOG_ERROR_ERRNO("Failed to start test thread.",
				STD_ERR_CODE, STD_ERR_DESC);
			result.set_code(protocol::ERRCODE_INTERNAL_ERROR);
			result.set_desc("Failed to start thread.");
			delete ledger_context;
			return false;
		}

		int64_t time_start = utils::Timestamp::HighResolution();
		bool is_timeout = false;
		while (ledger_context->IsRunning()) {
			utils::Sleep(10);
			if (utils::Timestamp::HighResolution() - time_start > total_timeout) {
				is_timeout = true;
				break;
			}
		}

		if (is_timeout) { //cancel it
			ledger_context->Cancel();
			result.set_code(protocol::ERRCODE_TX_TIMEOUT);
			result.set_desc("Contract execution timeout");
			LOG_ERROR("Testing consensus value(" FMT_I64 "ms) timeout", total_timeout / utils::MICRO_UNITS_PER_MILLI);
			ledger_context->JoinWithStop();
			delete ledger_context;
			return false;
		}

		//add tx
		LedgerFrm::pointer ledger = ledger_context->closing_ledger_;
		std::vector<TransactionFrm::pointer> &apply_tx_frms = ledger->apply_tx_frms_;
		if (apply_tx_frms.size() == 0)
			apply_tx_frms = ledger->dropped_tx_frms_;

		for (size_t i = 0; i < apply_tx_frms.size(); i++) {
			const TransactionFrm::pointer ptr = apply_tx_frms[i];

			protocol::TransactionEnvStore env_store;
			*env_store.mutable_transaction_env() = apply_tx_frms[i]->GetTransactionEnv();
			env_store.set_ledger_seq(ledger->GetProtoHeader().seq());
			env_store.set_close_time(ledger->GetProtoHeader().close_time());
			env_store.set_error_code(ptr->GetResult().code());
			env_store.set_error_desc(ptr->GetResult().desc());
			if (ptr->GetResult().code() != 0)
				env_store.set_actual_fee(ptr->GetFeeLimit());
			else{
				if (type == LedgerContext::AT_TEST_V8)
					env_store.set_actual_fee(ptr->GetActualGas()*ptr->GetGasPrice());
				else if (LedgerContext::AT_TEST_TRANSACTION) {
					int64_t actual_gas = 0;
					//64 is length of signature data, 76 is length of public key, 5 and 20 for redundancy
					if (!utils::SafeIntAdd(ptr->GetActualGas(), (int64_t)(signature_number*(64 + 76 + 5) + 20), actual_gas)) {
						LOG_ERROR("Calculate actual gas overflow, actual gas (" FMT_I64 "), signature number(" FMT_I64 ")", ptr->GetActualGas(), signature_number);
					}

					int64_t actual_fee = 0;
					if (!utils::SafeIntMul(actual_gas, ptr->GetGasPrice(), actual_fee)) {
						LOG_ERROR("Calculate actual fee overflow, actual gas(" FMT_I64 "), gas price(" FMT_I64 ")", actual_gas, ptr->GetGasPrice());
					}
					env_store.set_actual_fee(actual_fee);

					result = ptr->GetResult();
					Json::Value jtx = Proto2Json(env_store);
					jtx["gas"] = actual_gas;
					txs[txs.size()] = jtx;
				}
			}
			if (LedgerContext::AT_TEST_TRANSACTION)
				result = ptr->GetResult();
			//batch.Put(ComposePrefix(General::TRANSACTION_PREFIX, ptr->GetContentHash()), env_store.SerializeAsString());

			for (size_t j = 0; j < ptr->instructions_.size(); j++) {
				protocol::TransactionEnvStore env_sto = ptr->instructions_.at(j);
				env_sto.set_ledger_seq(ledger->GetProtoHeader().seq());
				env_sto.set_close_time(ledger->GetProtoHeader().close_time());
				txs[txs.size()] = Proto2Json(env_sto);
				//std::string hash = HashWrapper::Crypto(env_sto.transaction_env().transaction().SerializeAsString());
				//batch.Put(ComposePrefix(General::TRANSACTION_PREFIX, hash), env_sto.SerializeAsString());
			}
		}

		//add stat
		if (ledger_context->transaction_stack_.size() > 0) {
			TransactionFrm::pointer ptr = ledger_context->transaction_stack_[0];
			stat["step"] = ptr->GetContractStep();
			stat["memory_usage"] = ptr->GetMemoryUsage();
			stat["stack_usage"] = ptr->GetStackUsage();
			stat["apply_time"] = ptr->GetApplyTime();
		}
		if (type == LedgerContext::AT_TEST_TRANSACTION){
			TransactionFrm::pointer ptr = apply_tx_frms[0];
			stat["step"] = ptr->GetContractStep();
			stat["memory_usage"] = ptr->GetMemoryUsage();
			stat["stack_usage"] = ptr->GetStackUsage();
			stat["apply_time"] = ptr->GetApplyTime();
		}

		ledger_context->GetLogs(logs);
		ledger_context->GetRets(rets);
		ledger_context->JoinWithStop();
		delete ledger_context;
		return true;
	}

	bool LedgerContextManager::SyncPreProcess(const protocol::ConsensusValue &consensus_value, bool propose, ProposeTxsResult &propose_result) {

		std::string con_str = consensus_value.SerializeAsString();
		std::string chash = HashWrapper::Crypto(con_str);

		int32_t check_complete = CheckComplete(chash);
		if (check_complete > 0 ){
			return check_complete == 1;
		} 

		LedgerContext *ledger_context = new LedgerContext(this, chash, consensus_value, propose);

		if (!ledger_context->Start("process-value")) {
			LOG_ERROR_ERRNO("Failed to start processing consensus value, consensus value hash(%s)", utils::String::BinToHexString(chash).c_str(), 
				STD_ERR_CODE, STD_ERR_DESC);

			propose_result.block_timeout_ = true;
			delete ledger_context;
			return false;
		}

		int64_t time_start = utils::Timestamp::HighResolution();
		while (ledger_context->IsRunning()) {
			utils::Sleep(10);
			if (utils::Timestamp::HighResolution() - time_start > General::BLOCK_EXECUTE_TIME_OUT) {
				propose_result.block_timeout_ = true;
				break;
			}
		}

		propose_result.tx_execute_count_ = ledger_context->closing_ledger_->GetTxCount();

		if (propose_result.block_timeout_) { //cancel it
			ledger_context->Cancel();
			LOG_ERROR("Pre-executing consensus value(" FMT_I64 "ms) timeout", (utils::Timestamp::HighResolution() - time_start) / utils::MICRO_UNITS_PER_MILLI);
			return false;
		}

		propose_result = ledger_context->propose_result_;
		return propose_result.exec_result_;
	}

	void LedgerContextManager::RemoveCompleted(int64_t ledger_seq) {
		utils::MutexGuard guard(ctxs_lock_);
		for (LedgerContextMap::iterator iter = completed_ctxs_.begin();
			iter != completed_ctxs_.end();
			) {
			if (iter->second->consensus_value_.ledger_seq() <= ledger_seq) {
				delete iter->second;
				completed_ctxs_.erase(iter++);
			}
			else {
				iter++;
			}
		}
	}

	void LedgerContextManager::GetModuleStatus(Json::Value &data) {
		utils::MutexGuard guard(ctxs_lock_);
		data["completed_size"] = (Json::UInt64)completed_ctxs_.size();
		data["running_size"] = (Json::UInt64)running_ctxs_.size();
	}

	void LedgerContextManager::OnTimer(int64_t current_time) {

		std::vector<LedgerContext *> delete_context;
		do {
			utils::MutexGuard guard(ctxs_lock_);
			for (LedgerContextMultiMap::iterator iter = running_ctxs_.begin(); 
				iter != running_ctxs_.end();) {
				if (iter->second->CheckExpire( 5 * utils::MICRO_UNITS_PER_SEC)){
					delete_context.push_back(iter->second);
					iter = running_ctxs_.erase(iter);
				}
				else {
					iter++;
				}
			}

			for (LedgerContextTimeMultiMap::iterator iter = delete_ctxs_.begin();
				iter != delete_ctxs_.end();
				) {
				if (current_time > iter->first) {
					delete_context.push_back(iter->second);
					iter = delete_ctxs_.erase(iter);
				} else{
					iter++;
				}
			}
			 
		} while (false);

		for (size_t i = 0; i < delete_context.size(); i++) {
			delete_context[i]->Cancel();
			delete delete_context[i];
		}
	}

	void LedgerContextManager::OnSlowTimer(int64_t current_time) {}
	
	void LedgerContextManager::MoveRunningToDelete(LedgerContext *ledger_context) {
		utils::MutexGuard guard(ctxs_lock_);
		for (LedgerContextMultiMap::iterator iter = running_ctxs_.begin();
			iter != running_ctxs_.end();
			) {
			if (iter->second == ledger_context) {
				running_ctxs_.erase(iter++);
			}
			else {
				iter++;
			}
		}

		delete_ctxs_.insert(std::make_pair(utils::Timestamp::HighResolution() + 5 * utils::MICRO_UNITS_PER_SEC, ledger_context));
	}

	void LedgerContextManager::MoveRunningToComplete(LedgerContext *ledger_context) {
		utils::MutexGuard guard(ctxs_lock_);
		for (LedgerContextMultiMap::iterator iter = running_ctxs_.begin();
			iter != running_ctxs_.end();
			) {
			if (iter->second == ledger_context) {
				running_ctxs_.erase(iter++);
			}
			else {
				iter++;
			}
		}

		completed_ctxs_.insert(std::make_pair(ledger_context->GetHash(), ledger_context));
	}
}
