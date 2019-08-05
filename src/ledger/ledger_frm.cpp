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

#include <sstream>

#include <utils/utils.h>
#include <common/storage.h>
#include <common/pb2json.h>
#include <glue/glue_manager.h>
#include "ledger_manager.h"
#include "ledger_frm.h"
#include "ledgercontext_manager.h"
#include <contract/contract_manager.h>

namespace bumo {

#define COUNT_PER_PARTITION 1000000

	ProposeTxsResult::ProposeTxsResult() :
		block_timeout_(false),
		exec_result_(false),
		tx_execute_count_(0){}

	ProposeTxsResult::~ProposeTxsResult() {}

	void ProposeTxsResult::SetApply(ProposeTxsResult &result) {
		tx_execute_count_ = result.tx_execute_count_;
		exec_result_ = result.exec_result_;
		cons_validation_ = result.cons_validation_;
		need_dropped_tx_ = result.need_dropped_tx_;
	}

	LedgerFrm::LedgerFrm() {
		lpledger_context_ = NULL;
		enabled_ = false;
		apply_time_ = -1;
		total_fee_ = 0;
		is_test_mode_ = false;
	}

	LedgerFrm::~LedgerFrm() {
	}

	bool LedgerFrm::LoadFromDb(int64_t ledger_seq) {

		bumo::KeyValueDb *db = bumo::Storage::Instance().ledger_db();
		std::string ledger_header;
		int32_t ret = db->Get(ComposePrefix(General::LEDGER_PREFIX, ledger_seq), ledger_header);
		if (ret > 0) {
			ledger_.mutable_header()->ParseFromString(ledger_header);
			return true;
		}
		else if (ret < 0) {
			LOG_ERROR("Failed to get ledger, error desc(%s)", db->error_desc().c_str());
			return false;
		}

		return false;
	}

	bool LedgerFrm::AddToDb(WRITE_BATCH &batch) {
		KeyValueDb *db = Storage::Instance().ledger_db();

		batch.Put(bumo::General::KEY_LEDGER_SEQ, utils::String::ToString(ledger_.header().seq()));
		batch.Put(ComposePrefix(General::LEDGER_PREFIX, ledger_.header().seq()), ledger_.header().SerializeAsString());
		
		protocol::EntryList list;
		for (size_t i = 0; i < apply_tx_frms_.size(); i++) {
			const TransactionFrm::pointer ptr = apply_tx_frms_[i];

			protocol::TransactionEnvStore env_store;
			*env_store.mutable_transaction_env() = apply_tx_frms_[i]->GetTransactionEnv();
			env_store.set_ledger_seq(ledger_.header().seq());
			env_store.set_close_time(ledger_.header().close_time());
			env_store.set_error_code(ptr->GetResult().code());
			env_store.set_error_desc(ptr->GetResult().desc());
			if (ptr->GetResult().code() != 0){
				env_store.set_actual_fee(ptr->GetFeeLimit());
			}
			else{
				int64_t actual_fee=0;
				if (!utils::SafeIntMul(ptr->GetActualGas(), ptr->GetGasPrice(), actual_fee)){
					LOG_ERROR("Caculation of actual fee overflowed.");
				}

				env_store.set_actual_fee(actual_fee);
			}

			list.add_entry(ptr->GetContentHash());

			//If a transaction succeeds, the transactions triggered by it can be stored in db.
			if (ptr->GetResult().code() == protocol::ERRCODE_SUCCESS) {
				for (size_t j = 0; j < ptr->instructions_.size(); j++){
					protocol::TransactionEnvStore &env_sto = ptr->instructions_[j];
					env_sto.set_ledger_seq(ledger_.header().seq());
					env_sto.set_close_time(ledger_.header().close_time());
					std::string hash = HashWrapper::Crypto(env_sto.transaction_env().transaction().SerializeAsString());
					env_sto.set_hash(hash);

					//save contract txs
					batch.Put(ComposePrefix(General::TRANSACTION_PREFIX, hash), env_sto.SerializeAsString());
					list.add_entry(hash);
					env_store.add_contract_tx_hashes(hash);
				}
			}

			//save txs
			batch.Put(ComposePrefix(General::TRANSACTION_PREFIX, ptr->GetContentHash()), env_store.SerializeAsString());
		}

		//save tx hash list by ledger seq
		batch.Put(ComposePrefix(General::LEDGER_TRANSACTION_PREFIX, ledger_.header().seq()), list.SerializeAsString());

		//save the last tx hash
		if (list.entry_size() > 0) {
			protocol::EntryList new_last_hashs;
			if (list.entry_size() < General::LAST_TX_HASHS_LIMIT) {
				std::string str_last_hashs;
				int32_t ncount = db->Get(General::LAST_TX_HASHS, str_last_hashs);
				if (ncount < 0) {
					LOG_ERROR("Faild to load last transaction's hash, error desc(%s)", db->error_desc().c_str());
				}

				protocol::EntryList exist_hashs;
				if (ncount > 0 && !exist_hashs.ParseFromString(str_last_hashs)) {
					LOG_ERROR("Failed to parse last transaction hash from string.");
				}

				for (int32_t i = list.entry_size() - 1; i >= 0; i--) {
					*new_last_hashs.add_entry() = list.entry(i);
				}

				for (int32_t i = 0; 
					i < exist_hashs.entry_size() && new_last_hashs.entry_size() < General::LAST_TX_HASHS_LIMIT;
					i++) { 
					*new_last_hashs.add_entry() = exist_hashs.entry(i);
				}
			} else{
				for (int32_t i = list.entry_size() - 1; i >= list.entry_size() - General::LAST_TX_HASHS_LIMIT; i--) {
					*new_last_hashs.add_entry() = list.entry(i);
				}
			}

			batch.Put(General::LAST_TX_HASHS, new_last_hashs.SerializeAsString());
		}

		if (!db->WriteBatch(batch)){
			PROCESS_EXIT("Failed to write ledger and transaction to database(%s)", db->error_desc().c_str());
		}
		return true;
	}

	bool LedgerFrm::Cancel() {
		enabled_ = false;
		return true;
	}

	bool LedgerFrm::CheckConsValueValidation(const protocol::ConsensusValue& request,
		std::set<int32_t> &expire_txs_status,
		std::set<int32_t> &error_txs_status) {
		
		if (!request.has_validation()) {
			return true;
		}

		std::set<int32_t> totol_error;
		int32_t tx_size = request.txset().txs_size();
		const protocol::ConsensusValueValidation &validation = request.validation();
		for (int32_t i = 0; i < validation.expire_tx_ids_size(); i++) {
			int32_t tid = validation.expire_tx_ids(i);
			if (tid >= tx_size || tid < 0) {
				LOG_ERROR("Id(%d) of transaction that exceeding limit in proposed value is not valid: transaction size(%d), consensus value(sequence:" FMT_I64 ")",
					tid, tx_size, request.ledger_seq());
				return false;
			}
			expire_txs_status.insert(tid);

			if (totol_error.find(tid) != totol_error.end()){
				LOG_ERROR("Id(%d) of transaction in proposed value duplicated: consensus value(sequence:" FMT_I64 ")", tid, request.ledger_seq());
				return false;
			}

			totol_error.insert(tid);
		}

		for (int32_t i = 0; i < validation.error_tx_ids_size(); i++) {
			int32_t tid = validation.error_tx_ids(i);
			if (tid >= tx_size || tid < 0) {
				LOG_ERROR("Error id(%d) of transaction in proposed value not valid: transaction size(%d), consensus value(sequence:" FMT_I64 ")",
					tid, tx_size, request.ledger_seq());
				return false;
			}
			error_txs_status.insert(validation.error_tx_ids(i));

			if (totol_error.find(tid) != totol_error.end()) {
				LOG_ERROR("Id(%d) of transaction in proposed value duplicated: consensus value sequence(" FMT_I64 ")", tid, request.ledger_seq());
				return false;
			}
			totol_error.insert(tid);
		}

		return true;
	}

	void LedgerFrm::SetValidationToProto(std::set<int32_t> expire_txs,
		std::set<int32_t> error_txs,
		protocol::ConsensusValueValidation &validation) {
		for (std::set<int32_t>::iterator iter = expire_txs.begin();
			iter != expire_txs.end();
			iter++) {
			validation.add_expire_tx_ids(*iter);
		}
		for (std::set<int32_t>::iterator iter = error_txs.begin();
			iter != error_txs.end();
			iter++) {
			validation.add_error_tx_ids(*iter);
		}
	}

	bool LedgerFrm::ApplyPropose(const protocol::ConsensusValue& request,
		LedgerContext *ledger_context,
		ProposeTxsResult &proposed_result) {

		int64_t start_time = utils::Timestamp::HighResolution();
		lpledger_context_ = ledger_context;
		enabled_ = true;
		value_ = std::make_shared<protocol::ConsensusValue>(request);
		uint32_t success_count = 0;
		total_fee_ = 0;
		environment_ = std::make_shared<Environment>();

		//init the txs map (transaction map).
		std::set<int32_t> expire_txs, error_txs;

		if (request.has_validation()) {
			LOG_ERROR("Propose value has no validation object: consensus value sequence: " FMT_I64 ".", request.ledger_seq());
			return false;
		}

		for (int i = 0; i < request.txset().txs_size() && enabled_; i++) {
			const protocol::TransactionEnv &txproto = request.txset().txs(i);

			TransactionFrm::pointer tx_frm = std::make_shared<TransactionFrm>(txproto);

			if (!tx_frm->ValidForApply(environment_, !IsTestMode())) {
				dropped_tx_frms_.push_back(tx_frm);
				proposed_result.need_dropped_tx_.insert(i); //for drop
				continue;
			}

			//pay fee
			if (!tx_frm->PayFee(environment_, total_fee_)) {
				dropped_tx_frms_.push_back(tx_frm);
				proposed_result.need_dropped_tx_.insert(i);//for drop
				continue;
			}

			ledger_context->transaction_stack_.push_back(tx_frm);
			tx_frm->NonceIncrease(this, environment_);
			environment_->Commit();

			tx_frm->EnableChecked();
			tx_frm->SetMaxEndTime(utils::Timestamp::HighResolution() + General::TX_EXECUTE_TIME_OUT);

			bool ret = tx_frm->Apply(this, environment_);
			//Calculate the required minimum fee by calculating the bytes of the transaction. Do not store the transaction when the user-specified fee is less than this fee. 
			std::string error_info;
			if (tx_frm->IsExpire(error_info)) {
				LOG_ERROR("Failed to apply transaction(%s): %s, %s",
					utils::String::BinToHexString(tx_frm->GetContentHash()).c_str(), tx_frm->GetResult().desc().c_str(),
					error_info.c_str());
				expire_txs.insert(i - proposed_result.need_dropped_tx_.size());//for check
			}
			else {
				if (!ret) {
					LOG_ERROR("Failed to apply transaction(%s): %s",
						utils::String::BinToHexString(tx_frm->GetContentHash()).c_str(), tx_frm->GetResult().desc().c_str());
					error_txs.insert(i - proposed_result.need_dropped_tx_.size());//for check
				}
				else {
					tx_frm->ReturnFee(total_fee_);
					tx_frm->environment_->Commit();
				}
			}

			environment_->ClearChangeBuf();
			apply_tx_frms_.push_back(tx_frm);
			ledger_.add_transaction_envs()->CopyFrom(txproto);
			ledger_context->transaction_stack_.pop_back();

			if ( utils::Timestamp::HighResolution() - start_time > General::BLOCK_EXECUTE_TIME_OUT) {
				LOG_ERROR("Applying block timeout(" FMT_I64 ") ", utils::Timestamp::HighResolution() - start_time);
				return false;
			}
		}

		AllocateReward();

		SetValidationToProto(expire_txs, error_txs, proposed_result.cons_validation_);

		apply_time_ = utils::Timestamp::HighResolution() - start_time;
		return true;
	}

	bool LedgerFrm::ApplyCheck(const protocol::ConsensusValue& request,
		LedgerContext *ledger_context) {

		int64_t start_time = utils::Timestamp::HighResolution();
		lpledger_context_ = ledger_context;
		enabled_ = true;
		value_ = std::make_shared<protocol::ConsensusValue>(request);
		uint32_t success_count = 0;
		total_fee_ = 0;
		environment_ = std::make_shared<Environment>();

		//init the txs map (transaction map).
		std::set<int32_t> expire_txs_check,  error_txs_check;
		std::set<int32_t> expire_txs,  error_txs;
		if (!CheckConsValueValidation(request, expire_txs_check,  error_txs_check)) {
			LOG_ERROR("Failed to check consensus value: consensus value sequence(" FMT_I64 ")", request.ledger_seq());
			return false;
		}

		for (int i = 0; i < request.txset().txs_size() && enabled_; i++) {
			auto txproto = request.txset().txs(i);

			TransactionFrm::pointer tx_frm = std::make_shared<TransactionFrm>(txproto);

			if (!tx_frm->ValidForApply(environment_, !IsTestMode())) {
				LOG_ERROR("Validition for application failed: consensus value sequence(" FMT_I64 ")", request.ledger_seq());
				return false;
			}

			//pay fee
			if (!tx_frm->PayFee(environment_, total_fee_)) {
				LOG_ERROR("Failed to pay fee, consensus value sequence(" FMT_I64 ")", request.ledger_seq());
				return false;
			}

			ledger_context->transaction_stack_.push_back(tx_frm);
			tx_frm->NonceIncrease(this, environment_);
			environment_->Commit();

			tx_frm->EnableChecked();
			tx_frm->SetMaxEndTime(utils::Timestamp::HighResolution() + General::TX_EXECUTE_TIME_OUT);

			bool ret = tx_frm->Apply(this, environment_);
			//Caculate the required mininum fee by calculting the bytes of the transaction. Do not store the transaction when the user-specified fee is less than this fee. 
			std::string error_info;
			if (tx_frm->IsExpire(error_info)) {
				LOG_ERROR("Failed to apply transaction(%s). %s, %s",
					utils::String::BinToHexString(tx_frm->GetContentHash()).c_str(), tx_frm->GetResult().desc().c_str(),
					error_info.c_str());
				expire_txs.insert(i);//for check
			}
			else {
				if (!ret) {
					LOG_ERROR("Failed to apply transaction(%s). %s",
						utils::String::BinToHexString(tx_frm->GetContentHash()).c_str(), tx_frm->GetResult().desc().c_str());
					error_txs.insert(i);//for check
				}
				else {
					tx_frm->ReturnFee(total_fee_);
					tx_frm->environment_->Commit();
				}
			}

			environment_->ClearChangeBuf();
			apply_tx_frms_.push_back(tx_frm);
			ledger_.add_transaction_envs()->CopyFrom(txproto);
			ledger_context->transaction_stack_.pop_back();

			if (utils::Timestamp::HighResolution() - start_time > General::BLOCK_EXECUTE_TIME_OUT) {
				LOG_ERROR("Applying block timeout(" FMT_I64 ") ", utils::Timestamp::HighResolution() - start_time);
				return false;
			}
		}
		AllocateReward();
		apply_time_ = utils::Timestamp::HighResolution() - start_time;

		bool ret = (expire_txs == expire_txs_check && error_txs == error_txs_check);
		if (!ret) {
			LOG_ERROR("Failed to check validation: this size(%d,%d), check size(%d,%d) ",
				expire_txs.size(), error_txs.size(),
				expire_txs_check.size(), error_txs_check.size());
		}
		return ret;
	}

	bool LedgerFrm::ApplyFollow(const protocol::ConsensusValue& request,
		LedgerContext *ledger_context) {

		int64_t start_time = utils::Timestamp::HighResolution();
		lpledger_context_ = ledger_context;
		enabled_ = true;
		value_ = std::make_shared<protocol::ConsensusValue>(request);
		uint32_t success_count = 0;
		total_fee_= 0;
		environment_ = std::make_shared<Environment>();

		//Init the txs map (transaction map).
		std::set<int32_t> expire_txs_check, error_txs_check;
		std::set<int32_t> error_txs;
		if (!CheckConsValueValidation(request, expire_txs_check, error_txs_check)) {
			LOG_ERROR("Failed to check consensus value validation, consensus value sequence(" FMT_I64 ")", request.ledger_seq());
			return false;
		}

		for (int i = 0; i < request.txset().txs_size() && enabled_; i++) {
			auto txproto = request.txset().txs(i);
			
			TransactionFrm::pointer tx_frm = std::make_shared<TransactionFrm>(txproto);

			//Pay fee
			if (!tx_frm->PayFee(environment_, total_fee_)) {
				LOG_WARN("Failed to pay fee.");
				continue;
			}

			ledger_context->transaction_stack_.push_back(tx_frm);
			tx_frm->NonceIncrease(this, environment_);
			environment_->Commit();


			if ( expire_txs_check.find(i) != expire_txs_check.end()) {
				//Follow the consensus value, and do not apply the transaction set.
				tx_frm->ApplyExpireResult();
			}
			else {
				bool ret = tx_frm->Apply(this, environment_);
				if (!ret) {
					LOG_ERROR("Failed to apply transaction(%s). %s",
						utils::String::BinToHexString(tx_frm->GetContentHash()).c_str(), tx_frm->GetResult().desc().c_str());
					error_txs.insert(i);//for check
				}
				else {
					tx_frm->ReturnFee(total_fee_);
					tx_frm->environment_->Commit();
				}
			}

			environment_->ClearChangeBuf();
			apply_tx_frms_.push_back(tx_frm);			
			ledger_.add_transaction_envs()->CopyFrom(txproto);
			ledger_context->transaction_stack_.pop_back();

		}
		AllocateReward();
		apply_time_ = utils::Timestamp::HighResolution() - start_time;

		if (error_txs != error_txs_check) {
			LOG_ERROR("Failed to check validation. This statement should not be executed, and the execution and checked result are (%d,%d)",
				error_txs.size(), error_txs_check.size());
		}
		return true;
	}

	Json::Value LedgerFrm::ToJson() {
		return bumo::Proto2Json(ledger_);
	}

	protocol::Ledger &LedgerFrm::ProtoLedger() {
		return ledger_;
	}

	bool LedgerFrm::Commit(KVTrie* trie, int64_t& new_count, int64_t& change_count) {
		auto batch = trie->batch_;
		auto entries = environment_->GetData();

		for (auto it = entries.begin(); it != entries.end(); it++){

			if (it->second.type_ == utils::DEL)
				continue; //There is no delete account function now.

			std::shared_ptr<AccountFrm> account = it->second.ptr_;
			account->UpdateHash(batch);
			std::string ss = account->Serializer();
			std::string index = DecodeAddress(it->first);
			bool is_new = trie->Set(index, ss);
			if (is_new){
				new_count++;
			}
			else{
				change_count++;
			}
		}
		return true;
	}

	bool LedgerFrm::AllocateReward() {
		int64_t block_reward = GetBlockReward(ledger_.header().seq());
		int64_t total_reward = total_fee_ + block_reward;
		if (total_reward == 0 || IsTestMode()) {
			return true;
		}
		LOG_INFO("total reward(" FMT_I64 ") = total fee(" FMT_I64 ") + block reward(" FMT_I64 ") in ledger(" FMT_I64 ")", total_reward, total_fee_, block_reward, ledger_.header().seq());

		// Allocate reward directly or through contract
		if (CHECK_VERSION_GT_1002)
		{
			std::shared_ptr<AccountFrm> account;
			if (environment_->GetEntry(GET_CONTRACT_VALIDATOR_ADDRESS, account)) {
				int64_t new_balance = 0;
				if (!utils::SafeIntAdd(account->GetAccountBalance(), total_reward, new_balance)){
					LOG_ERROR("Overflowed when rewarding account. Account balance:(" FMT_I64 "), total reward:(" FMT_I64 ")", account->GetAccountBalance(), total_reward);
					return false;
				}
				account->GetProtoAccount().set_balance(new_balance);
				LOG_TRACE("Allocate block reward, amount:(" FMT_I64 "), seq:(" FMT_I64 ")", total_reward, ledger_.header().seq());
			}
			else {
				LOG_TRACE("Failed to get dpos contract address(%s), allocate reward to validators directly", GET_CONTRACT_VALIDATOR_ADDRESS);
				if (!AllocateRewardDirectly(total_reward)) return false;
			}
		}
		else {
			if (!AllocateRewardDirectly(total_reward)) return false;
		}

		environment_->Commit();
		return true;
	}

	bool LedgerFrm::AllocateRewardDirectly(int64_t total_reward) {
		protocol::ValidatorSet set;
		if (!LedgerManager::Instance().GetValidators(ledger_.header().seq() - 1, set)) {
			LOG_ERROR("Failed to get validator of ledger(" FMT_I64 ")", ledger_.header().seq() - 1);
			return false;
		}
		if (set.validators_size() == 0) {
			LOG_ERROR("Validator set should not be empty.");
			return false;
		}

		int64_t left_reward = total_reward;
		std::shared_ptr<AccountFrm> random_account;
		int64_t random_index = ledger_.header().seq() % set.validators_size();
		int64_t average_fee = total_reward / set.validators_size();
		
		for (int32_t i = 0; i < set.validators_size(); i++) {
			std::shared_ptr<AccountFrm> account;
			if (!environment_->GetEntry(set.validators(i).address(), account)) {
				account = AccountFrm::CreatAccountFrm(set.validators(i).address(), 0);
				environment_->AddEntry(account->GetAccountAddress(), account);
			}
			if (random_index == i) {
				random_account = account;
			}

			left_reward -= average_fee;

			LOG_TRACE("Account(%s) allocated reward(" FMT_I64 "), left reward(" FMT_I64 ") in ledger(" FMT_I64 ")", account->GetAccountAddress().c_str(), average_fee, left_reward, ledger_.header().seq());
			protocol::Account &proto_account = account->GetProtoAccount();
			int64_t new_balance = 0;;
			if (!utils::SafeIntAdd(proto_account.balance(), average_fee, new_balance)){
				LOG_ERROR("Overflowed when rewarding account. Account balance:(" FMT_I64 "), average_fee:(" FMT_I64 ")", proto_account.balance(), average_fee);
				return false;
			}
			proto_account.set_balance(new_balance);
		}
		if (left_reward > 0) {
			protocol::Account &proto_account = random_account->GetProtoAccount();
			int64_t new_balance = 0;
			if (!utils::SafeIntAdd(proto_account.balance(), left_reward, new_balance)){
				LOG_ERROR("Overflowed when rewarding account. Account balance:(" FMT_I64 "), reward:(" FMT_I64 ")", proto_account.balance(), left_reward);
				return false;
			}
			proto_account.set_balance(new_balance);
			LOG_TRACE("Account(%s) aquired last reward(" FMT_I64 ") of allocation in ledger(" FMT_I64 ")", proto_account.address().c_str(), left_reward, ledger_.header().seq());
		}

		return true;
	}

	void LedgerFrm::SetTestMode(bool test_mode){
		is_test_mode_ = test_mode;
	}

	bool LedgerFrm::IsTestMode(){
		return is_test_mode_;
	}
}
