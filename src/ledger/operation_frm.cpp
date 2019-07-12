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

#include <utils/strings.h>
#include <ledger/ledger_manager.h>
#include "transaction_frm.h"
#include "operation_frm.h"
#include <contract/contract_manager.h>
#include "fee_calculate.h"


namespace bumo {
	OperationFrm::OperationFrm(const protocol::Operation &operation, TransactionFrm* tran, int32_t index) :
		operation_(operation), transaction_(tran), index_(index), ope_fee_(0){
	}

	OperationFrm::~OperationFrm() {}

	Result OperationFrm::GetResult() const {
		return result_;
	}

	int64_t OperationFrm::GetOpeFee() const {
		return ope_fee_;
	}

	bool OperationFrm::isReservedAddress(const std::string& addr){
		if (addr == General::CONTRACT_DPOS_ADDRESS ||
			addr == General::RESERVED_ADDRESS4 ||
			addr == General::RESERVED_ADDRESS5 ||
			addr == General::RESERVED_ADDRESS6 ||
			addr == General::RESERVED_ADDRESS7 ||
			addr == General::RESERVED_ADDRESS8){
			return true;
		}
		return false;
	}

	bool OperationFrm::ReservedAddrAvailable(const std::string& src, const std::string& dest){
		AccountFrm::pointer destAccount;
		if (isReservedAddress(dest) && src != General::RESERVED_CREATOR_ADDRESS && !Environment::AccountFromDB(dest, destAccount)){
			return false;
		}

		return true;
	}

	Result OperationFrm::CheckValid(const protocol::Operation& operation, const std::string &source_address, uint32_t ldcontext_stack_size) {
		Result result;
		result.set_code(protocol::ERRCODE_SUCCESS);
		auto type = operation.type();
		const protocol::OperationCreateAccount& create_account = operation.create_account();
		const protocol::OperationPayAsset& payAsset = operation.pay_asset();
		const protocol::OperationIssueAsset& issue_asset = operation.issue_asset();

		if (!bumo::PublicKey::IsAddressValid(source_address)) {
			result.set_code(protocol::ERRCODE_INVALID_ADDRESS);
			result.set_desc(utils::String::Format("Source address should be a valid account address."));
			return result;
		}

		const utils::StringList &forb_list = Configure::Instance().ledger_configure_.forbid_addrs_;
		for (utils::StringList::const_iterator iter = forb_list.begin();
			iter != forb_list.end();
			iter++) {
			if (source_address == *iter){
				result.set_code(protocol::ERRCODE_INVALID_ADDRESS);
				result.set_desc(utils::String::Format("Source address should be a valid accout address."));
				return result;
			}
		}

		//const auto &issue_property = issue_asset.
		switch (type) {
		case protocol::Operation_Type_CREATE_ACCOUNT:
		{
			if (CHECK_VERSION_GT_1000){
				if (!ReservedAddrAvailable(source_address, create_account.dest_address())){
					result.set_code(protocol::ERRCODE_INVALID_ADDRESS);
					result.set_desc(utils::String::Format("Dest account address(%s) invalid.", create_account.dest_address().c_str()));
					break;
				}

				result = CheckCreateAccountGt1000(create_account, ldcontext_stack_size);
				break;
			}

			if (!bumo::PublicKey::IsAddressValid(create_account.dest_address())) {
				result.set_code(protocol::ERRCODE_INVALID_ADDRESS);
				result.set_desc(utils::String::Format("Dest account address(%s) invalid.", create_account.dest_address().c_str()));
				break;
			}

			if (!ReservedAddrAvailable(source_address, create_account.dest_address())){
				result.set_code(protocol::ERRCODE_INVALID_ADDRESS);
				result.set_desc(utils::String::Format("Dest account address(%s) invalid.", create_account.dest_address().c_str()));
				break;
			}

			if (!create_account.has_priv()) {
				result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
				result.set_desc(utils::String::Format("Dest account address(%s) has no priv object", create_account.dest_address().c_str()));
				break;
			} 

			const protocol::AccountPrivilege &priv = create_account.priv();
			if (priv.master_weight() < 0 || priv.master_weight() > UINT32_MAX) {
				result.set_code(protocol::ERRCODE_WEIGHT_NOT_VALID);
				result.set_desc(utils::String::Format("Master weight(" FMT_I64 ") is larger than %u or less than 0", priv.master_weight(), UINT32_MAX));
				break;
			}

			//for signers
			std::set<std::string> duplicate_set;
			bool shouldBreak = false;
			for (int32_t i = 0; i < priv.signers_size(); i++) {
				const protocol::Signer &signer = priv.signers(i);
				if (signer.weight() < 0 || signer.weight() > UINT32_MAX) {
					result.set_code(protocol::ERRCODE_WEIGHT_NOT_VALID);
					result.set_desc(utils::String::Format("Signer weight(" FMT_I64 ") is larger than %u or less than 0", signer.weight(), UINT32_MAX));
					shouldBreak = true;
					break;
				}

				if (signer.address() == create_account.dest_address()) {
					result.set_code(protocol::ERRCODE_INVALID_ADDRESS);
					result.set_desc(utils::String::Format("Signer address(%s) can't be equal to the source address", signer.address().c_str()));
					shouldBreak = true;
					break;
				}

				if (!PublicKey::IsAddressValid(signer.address())) {
					result.set_code(protocol::ERRCODE_INVALID_ADDRESS);
					result.set_desc(utils::String::Format("Signer address(%s) is not valid", signer.address().c_str()));
					shouldBreak = true;
					break;
				}

				if (duplicate_set.find(signer.address()) != duplicate_set.end()) {
					result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
					result.set_desc(utils::String::Format("Signer address(%s) duplicated", signer.address().c_str()));
					shouldBreak = true;
					break;
				} 

				duplicate_set.insert(signer.address());
			}
			if (shouldBreak) break;

			//for threshold
			if (!priv.has_thresholds()) {
				result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
				result.set_desc(utils::String::Format("dest account address(%s) has no threshold object", create_account.dest_address().c_str()));
				break;
			}

			const protocol::AccountThreshold &threshold = priv.thresholds();
			if (threshold.tx_threshold() < 0) {
				result.set_code(protocol::ERRCODE_THRESHOLD_NOT_VALID);
				result.set_desc(utils::String::Format("Low threshold(" FMT_I64 ") is less than 0", threshold.tx_threshold()));
				break;
			}

			std::set<int32_t> duplicate_type;
			for (int32_t i = 0; i < threshold.type_thresholds_size(); i++) {
				const protocol::OperationTypeThreshold  &type_thresholds = threshold.type_thresholds(i);
				if (type_thresholds.type() > 100 || type_thresholds.type() <= 0) {
					result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
					result.set_desc(utils::String::Format("Operation type(%u) not supported", type_thresholds.type()));
					break;
				}

				if (type_thresholds.threshold() < 0) {
					result.set_code(protocol::ERRCODE_THRESHOLD_NOT_VALID);
					result.set_desc(utils::String::Format("Operation type(%d) (threshold: " FMT_I64 ") is less than 0", (int32_t)type_thresholds.type(), type_thresholds.threshold()));
					break;
				}

				if (duplicate_type.find(type_thresholds.type()) != duplicate_type.end()) {
					result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
					result.set_desc(utils::String::Format("Operation type(%u) duplicated", type_thresholds.type()));
					break;
				} 
				
				duplicate_type.insert(type_thresholds.type());
			}

			//If it's contract then {master_weight:0 , thresholds:{tx_threshold:1} }
			if (create_account.contract().payload() != ""){
 				if (create_account.contract().payload().size() > General::CONTRACT_CODE_LIMIT) {
					result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
					result.set_desc(utils::String::Format("Contract payload size(" FMT_SIZE ") > limit(%d)",
						create_account.contract().payload().size(), General::CONTRACT_CODE_LIMIT));
					break;
				}

				if (!(priv.master_weight() == 0 &&
					priv.signers_size() == 0 &&
					threshold.tx_threshold() == 1 &&
					threshold.type_thresholds_size() == 0 
					)) {
					result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
					result.set_desc(utils::String::Format("Contract account 'priv' configuraion must be({master_weight:0, thresholds:{tx_threshold:1}})"));
					break;
				}
				
				std::string src = create_account.contract().payload();
				result = ContractManager::Instance().SourceCodeCheck(Contract::TYPE_V8, src, ldcontext_stack_size);
			}

			for (int32_t i = 0; i < create_account.metadatas_size(); i++){
				const auto kp = create_account.metadatas(i);
				if (kp.key().size() == 0 || kp.key().size() > General::METADATA_KEY_MAXSIZE) {
					result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
					result.set_desc(utils::String::Format("Length of the key should be between [1, %d]. key=%s,key.length=%d",
						General::METADATA_KEY_MAXSIZE, kp.key().c_str(), kp.key().length()));
				}

				if (kp.value().size() > General::METADATA_MAX_VALUE_SIZE){
					result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
					result.set_desc(utils::String::Format("Length of the value should be between [1, %d].key=%s,value.length=%d",
						General::METADATA_MAX_VALUE_SIZE, kp.key().c_str(), kp.value().length()));
				}
			}
			break;
		}
		case protocol::Operation_Type_PAY_ASSET:
		{
			if (payAsset.has_asset()){
				if (payAsset.asset().key().type() != 0){
					result.set_code(protocol::ERRCODE_ASSET_INVALID);
					result.set_desc(utils::String::Format("payAsset asset type must be 0"));
					break;
				}

				if (payAsset.asset().amount() <= 0) {
					result.set_code(protocol::ERRCODE_ASSET_INVALID);
					result.set_desc(utils::String::Format("Amount should be greater than 0"));
					break;
				}

				std::string trim_code = payAsset.asset().key().code();
				//utils::String::Trim(trim_code);
				if (trim_code.size() == 0 || trim_code.size() > General::ASSET_CODE_MAX_SIZE) {
					result.set_code(protocol::ERRCODE_ASSET_INVALID);
					result.set_desc(utils::String::Format("asset code length should be between (0,64]"));
					break;
				}

				if (!bumo::PublicKey::IsAddressValid(payAsset.asset().key().issuer())) {
					result.set_code(protocol::ERRCODE_ASSET_INVALID);
					result.set_desc(utils::String::Format("asset issuer should be a valid account address"));
					break;
				}
			}

			if (source_address == payAsset.dest_address()) {
				result.set_code(protocol::ERRCODE_ACCOUNT_SOURCEDEST_EQUAL);
				result.set_desc(utils::String::Format("Source address(%s) is equal to dest address", source_address.c_str()));
				break;
			} 

			if (!bumo::PublicKey::IsAddressValid(payAsset.dest_address())) {
				result.set_code(protocol::ERRCODE_INVALID_ADDRESS);
				result.set_desc(utils::String::Format("Dest address should be a valid account address"));
				break;
			}
			break;
		}

		case protocol::Operation_Type_ISSUE_ASSET:
		{
			if (issue_asset.amount() <= 0) {
				result.set_code(protocol::ERRCODE_ASSET_INVALID);
				result.set_desc(utils::String::Format("Amount should be greater than 0"));
				break;
			}

			std::string trim_code = issue_asset.code();
			trim_code = utils::String::Trim(trim_code);
			if (trim_code.size() == 0 || trim_code.size() > General::ASSET_CODE_MAX_SIZE ||
				trim_code.size() != issue_asset.code().size()) {
				result.set_code(protocol::ERRCODE_ASSET_INVALID);
				result.set_desc(utils::String::Format("Asset code length should be between (0,64]"));
				break;
			}

			break;
		}
		case protocol::Operation_Type_SET_METADATA:
		{
			const protocol::OperationSetMetadata &set_metadata = operation.set_metadata();

			std::string trim = set_metadata.key();
			if (trim.size() == 0 || trim.size() > General::METADATA_KEY_MAXSIZE) {
				result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
				result.set_desc(utils::String::Format("Length of the key should be between [1, %d]. key=%s,key.length=%d",
					General::METADATA_KEY_MAXSIZE, trim.c_str(), trim.length()));
				break;
			}

			if (set_metadata.value().size() > General::METADATA_MAX_VALUE_SIZE) {
				result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
				result.set_desc(utils::String::Format("Length of the value should be between [0, %d]. key=%s,value.length=%d",
					General::METADATA_MAX_VALUE_SIZE, trim.c_str(), set_metadata.value().length()));
				break;
			}


			break;
		}
		case protocol::Operation_Type_SET_SIGNER_WEIGHT:
		{
			const protocol::OperationSetSignerWeight &operation_setoptions = operation.set_signer_weight();
			if (operation_setoptions.master_weight() < -1 || operation_setoptions.master_weight() > UINT32_MAX) {
				result.set_code(protocol::ERRCODE_WEIGHT_NOT_VALID);
				result.set_desc(utils::String::Format("Master weight(" FMT_I64 ") is larger than %u or less than -1", operation_setoptions.master_weight(), UINT32_MAX));
				break;
			}

			for (int32_t i = 0; i < operation_setoptions.signers_size(); i++) {
				const protocol::Signer &signer = operation_setoptions.signers(i);
				if (signer.weight() < 0 || signer.weight() > UINT32_MAX) {
					result.set_code(protocol::ERRCODE_WEIGHT_NOT_VALID);
					result.set_desc(utils::String::Format("Signer weight(" FMT_I64 ") is larger than %u or less than 0", signer.weight(), UINT32_MAX));
					break;
				}

				if (signer.address() == source_address) {
					result.set_code(protocol::ERRCODE_INVALID_ADDRESS);
					result.set_desc(utils::String::Format("Signer address(%s) can't be equal to the source address", signer.address().c_str()));
					break;
				}

				if (!PublicKey::IsAddressValid(signer.address())) {
					result.set_code(protocol::ERRCODE_INVALID_ADDRESS);
					result.set_desc(utils::String::Format("Signer address(%s) is not valid", signer.address().c_str()));
					break;
				}
			}

			break;
		}
		case protocol::Operation_Type_SET_THRESHOLD:
		{
			const protocol::OperationSetThreshold operation_setoptions = operation.set_threshold();

			if ( operation_setoptions.tx_threshold() < -1) {
				result.set_code(protocol::ERRCODE_THRESHOLD_NOT_VALID);
				result.set_desc(utils::String::Format("Low threshold(" FMT_I64 ") is less than -1", operation_setoptions.tx_threshold()));
				break;
			}

			for (int32_t i = 0; i < operation_setoptions.type_thresholds_size(); i++) {
				const protocol::OperationTypeThreshold  &type_thresholds = operation_setoptions.type_thresholds(i);
				if (type_thresholds.type() > 100 || type_thresholds.type() <= 0) {
					result.set_code(protocol::ERRCODE_THRESHOLD_NOT_VALID);
					result.set_desc(utils::String::Format("Operation type(%u) not supported", type_thresholds.type()));
					break;
				}

				if (type_thresholds.threshold()  < 0 ) {
					result.set_code(protocol::ERRCODE_THRESHOLD_NOT_VALID);
					result.set_desc(utils::String::Format("Operation type(%d) threshold(" FMT_I64 ") is less than 0", (int32_t)type_thresholds.type(), type_thresholds.threshold()));
					break;
				}
			}
			break;
		}
		case protocol::Operation_Type_PAY_COIN:
		{
			const protocol::OperationPayCoin &pay_coin = operation.pay_coin();
			if (pay_coin.amount() < 0){
				result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
				result.set_desc(utils::String::Format("Amount should be greater than or equal to 0"));
				break;
			}

			if (source_address == pay_coin.dest_address()) {
				result.set_code(protocol::ERRCODE_ACCOUNT_SOURCEDEST_EQUAL);
				result.set_desc(utils::String::Format("Source address(%s) is equal to dest address", source_address.c_str()));
				break;
			}

			if (!bumo::PublicKey::IsAddressValid(pay_coin.dest_address())) {
				result.set_code(protocol::ERRCODE_INVALID_ADDRESS);
				result.set_desc(utils::String::Format("Dest address should be a valid account address"));
				break;
			}

			if (!ReservedAddrAvailable(source_address, pay_coin.dest_address())){
				result.set_code(protocol::ERRCODE_INVALID_ADDRESS);
				result.set_desc(utils::String::Format("Dest account address(%s) invalid.", pay_coin.dest_address().c_str()));
				break;
			}

			break;
		}
		case protocol::Operation_Type_LOG:
		{
			const protocol::OperationLog &log = operation.log();
			if (log.topic().size() == 0 || log.topic().size() > General::TRANSACTION_LOG_TOPIC_MAXSIZE ){
				result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
				result.set_desc(utils::String::Format("Log's parameter topic size should be between (0,%d]", General::TRANSACTION_LOG_TOPIC_MAXSIZE));
				break;
			}
			for (int i = 0; i < log.datas_size();i++) {
				if (log.datas(i).size() == 0 || log.datas(i).size() > General::TRANSACTION_LOG_DATA_MAXSIZE){
					result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
					result.set_desc(utils::String::Format("Log's parameter data size should be between (0, %d]",General::TRANSACTION_LOG_DATA_MAXSIZE));
					break;
				}
			}
			break;
		}
		case protocol::Operation_Type_SET_PRIVILEGE:
		{
			result = CheckSetPrivilege(operation.set_privilege(), source_address);
			break;
		}

		case protocol::Operation_Type_Operation_Type_INT_MIN_SENTINEL_DO_NOT_USE_:
			break;
		case protocol::Operation_Type_Operation_Type_INT_MAX_SENTINEL_DO_NOT_USE_:
			break;
		default:{
			result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
			result.set_desc(utils::String::Format("Operation type(%d) invalid", type));
			break;
		}
		}

		return result;
	}

	bool OperationFrm::CheckSignature(std::shared_ptr<Environment> txenvironment) {
		std::string source_address_ = operation_.source_address();
		if (source_address_.size() == 0) {
			source_address_ = transaction_->GetSourceAddress();
		}

		if (!txenvironment->GetEntry(source_address_, source_account_)) {
			result_.set_code(protocol::ERRCODE_ACCOUNT_NOT_EXIST);
			result_.set_desc(utils::String::Format("Source account(%s) is not exist", source_address_.c_str()));
			return false;
		}

		utils::StringVector vec;
		vec.push_back(source_address_);
		if (!transaction_->SignerHashPriv(source_account_, operation_.type())) {
			LOG_ERROR("Failed to check operation's signature.");
			result_.set_code(protocol::ERRCODE_INVALID_SIGNATURE);
			result_.set_desc(utils::String::Format("Failed to check operation's signature."));
			return false;
		}

		return true;
	}


	Result OperationFrm::Apply(std::shared_ptr<Environment>  environment) {
		std::string source_address = operation_.source_address();
		if (source_address.size() == 0) {
			source_address = transaction_->GetSourceAddress();
		}
		if (!environment->GetEntry(source_address, source_account_)) {
			result_.set_code(protocol::ERRCODE_ACCOUNT_NOT_EXIST);
			result_.set_desc(utils::String::Format("Source address(%s) is not exist", source_address.c_str()));
			return result_;
		}
		auto type = operation_.type();
		switch (type) {
		case protocol::Operation_Type_UNKNOWN:
			break;
		case protocol::Operation_Type_CREATE_ACCOUNT:
			CreateAccount(environment);
			break;
		case protocol::Operation_Type_PAY_ASSET:
			PayAsset(environment);
			break;
		case protocol::Operation_Type_ISSUE_ASSET:
			IssueAsset(environment);
			break;
		case protocol::Operation_Type_SET_METADATA:
			SetMetaData(environment);
			break;
		case protocol::Operation_Type_SET_SIGNER_WEIGHT:
			SetSignerWeight(environment);
			break;
		case protocol::Operation_Type_SET_THRESHOLD:
			SetThreshold(environment);
			break;
		case protocol::Operation_Type_PAY_COIN:
			PayCoin(environment);
			break;
		case protocol::Operation_Type_LOG:
			Log(environment);
			break;
		case protocol::Operation_Type_SET_PRIVILEGE:
			SetPrivilege(environment);
			break;
		case protocol::Operation_Type_Operation_Type_INT_MIN_SENTINEL_DO_NOT_USE_:
			break;
		case protocol::Operation_Type_Operation_Type_INT_MAX_SENTINEL_DO_NOT_USE_:
			break;
		default:
			result_.set_code(protocol::ERRCODE_INVALID_PARAMETER);
			result_.set_desc(utils::String::Format("type(%d) not supported", type));
			break;
		}
		return result_;
	}

	void OperationFrm::CreateAccount(std::shared_ptr<Environment> environment) {
		const protocol::OperationCreateAccount& create_account = operation_.create_account();
		do {
			std::shared_ptr<AccountFrm> dest_account;
			std::string dest_address = create_account.dest_address();
			if (dest_address.empty()){
				//If the ledger version is greater than 1000, you must leave the address of the smart contract empty. An address will be created automatically.
				bumo::PublicKey pub_key;
				std::string raw_pkey = utils::String::Format("%s-" FMT_I64 "-%d", 
					transaction_->GetSourceAddress().c_str(), transaction_->GetNonce(), index_);
				pub_key.Init(raw_pkey);
				dest_address = pub_key.GetEncAddress();
			}

			if (environment->GetEntry(dest_address, dest_account)) {
				result_.set_code(protocol::ERRCODE_ACCOUNT_DEST_EXIST);
				result_.set_desc(utils::String::Format("Dest address(%s) already exist", dest_address.c_str()));
				break;
			}

			int64_t base_reserve = LedgerManager::Instance().GetCurFeeConfig().base_reserve();
			if (create_account.init_balance() < base_reserve) {
				result_.set_code(protocol::ERRCODE_ACCOUNT_INIT_LOW_RESERVE);
				std::string error_desc = utils::String::Format("Dest address init balance (" FMT_I64 ") is not enough for base_reserve (" FMT_I64 ")", create_account.init_balance(), base_reserve);
				result_.set_desc(error_desc);
				LOG_ERROR("%s", error_desc.c_str());
				break;
			}
			int64_t limit_balance=0;
			if (!utils::SafeIntSub(source_account_->GetAccountBalance(), base_reserve, limit_balance)){
				result_.set_code(protocol::ERRCODE_MATH_OVERFLOW);
				std::string error_desc = utils::String::Format("Dest address init balance (" FMT_I64 ") overflowed (" FMT_I64 ")", source_account_->GetAccountBalance(), base_reserve);
				result_.set_desc(error_desc);
				LOG_ERROR("%s", error_desc.c_str());
				break;
			}

			if (limit_balance < create_account.init_balance()) {
				result_.set_code(protocol::ERRCODE_ACCOUNT_LOW_RESERVE);
				std::string error_desc = utils::String::Format("Source account(%s) balance(" FMT_I64 ") - base_reserve(" FMT_I64 ") is not enough for init balance(" FMT_I64 ")", 
					source_account_->GetAccountAddress().c_str(), source_account_->GetAccountBalance(), base_reserve, create_account.init_balance());
				result_.set_desc(error_desc);
				LOG_ERROR("%s", error_desc.c_str());
				break;
			}
			if (!source_account_->AddBalance(-1 * create_account.init_balance())){
				result_.set_code(protocol::ERRCODE_MATH_OVERFLOW);
				std::string error_desc = utils::String::Format("Source account(%s) balance overflow (" FMT_I64 ") - base_reserve(" FMT_I64 ") is not enough for init balance(" FMT_I64 ")",
					source_account_->GetAccountAddress().c_str(), source_account_->GetAccountBalance(), base_reserve, create_account.init_balance());
				result_.set_desc(error_desc);
				LOG_ERROR("%s", error_desc.c_str());
				break;
			}

			protocol::Account account;
			account.set_balance(create_account.init_balance());
			account.mutable_priv()->CopyFrom(create_account.priv());
			account.set_address(dest_address);
			
			account.mutable_contract()->CopyFrom(create_account.contract());
			dest_account = std::make_shared<AccountFrm>(account);

			bool success = true;
			for (int i = 0; i < create_account.metadatas_size(); i++) {
				protocol::KeyPair kp;
				kp.CopyFrom(create_account.metadatas(i));
				if (kp.version() != 0 && kp.version() != 1){
					success = false;
					break;
				}
				kp.set_version(1);
				dest_account->SetMetaData(kp);
			}
			if (!success){
				result_.set_code(protocol::ERRCODE_INVALID_DATAVERSION);
				result_.set_desc(utils::String::Format(
					"The version to set metadata while create account(%s) should be 0 or 1 ",
					dest_account->GetAccountAddress().c_str()));
				
				break;
			}

			environment->AddEntry(dest_account->GetAccountAddress(), dest_account);

			if (!create_account.contract().payload().empty()) {
				TransactionFrm::pointer bottom_tx = transaction_->ledger_->lpledger_context_->GetBottomTx();

				ContractParameter parameter;
				parameter.code_ = dest_account->GetProtoAccount().contract().payload();
				parameter.input_ = create_account.init_input();
				parameter.this_address_ = dest_address;
				parameter.ledger_context_ = transaction_->ledger_->lpledger_context_;
				parameter.init_ = true;
				
				//for block
				parameter.block_.number_ = transaction_->ledger_->value_->ledger_seq();
				parameter.block_.timestamp_ = transaction_->ledger_->value_->close_time();

				//for tx
				parameter.tx_.sender_ = bottom_tx->GetOperatingSourceAddress();
				parameter.tx_.initiator_ = bottom_tx->GetSourceAddress();
				parameter.tx_.fee_limit_ = bottom_tx->GetFeeLimit();
				parameter.tx_.gas_price_ = bottom_tx->GetGasPrice();
				parameter.tx_.hash_ = utils::String::BinToHexString(bottom_tx->GetContentHash());

				//for msg
				parameter.msg_.coin_amount_ = 0;
				parameter.msg_.asset_.Clear();
				parameter.msg_.initiator_ = transaction_->GetSourceAddress();
				parameter.msg_.nonce_ = transaction_->GetNonce();
				parameter.msg_.operation_index_ = transaction_->GetProcessingOperation();
				parameter.msg_.sender_ = source_account_->GetAccountAddress();
				ChangeCreateContractAmount(create_account.init_balance(), parameter.msg_.coin_amount_);

				std::string err_msg;
				result_ = ContractManager::Instance().Execute(Contract::TYPE_V8, parameter);

				if (result_.code() == 0){
					Json::Value contract_result;
					contract_result["contract_address"] = dest_address;
					contract_result["operation_index"] = index_;
					result_.set_desc(contract_result.toFastString());

					Json::Value temp = Json::Value(Json::objectValue);
					temp["type"] = "address";
					temp["value"] = dest_address;
					result_.set_contract_result(temp);
				}
			}

		} while (false);
	}

	void OperationFrm::IssueAsset(std::shared_ptr<Environment> environment) {
		const protocol::OperationIssueAsset& ope = operation_.issue_asset();
		do {

			protocol::AssetStore asset_e;
			protocol::AssetKey key;
			key.set_issuer(source_account_->GetAccountAddress());
			key.set_code(ope.code());
			if (!source_account_->GetAsset(key, asset_e)) {
				protocol::AssetStore asset;
				asset.mutable_key()->CopyFrom(key);
				asset.set_amount(ope.amount());
				source_account_->SetAsset(asset);
			}
			else {
				int64_t amount = 0;
				if (!utils::SafeIntAdd(asset_e.amount(), ope.amount(), amount)){
					result_.set_code(protocol::ERRCODE_MATH_OVERFLOW);
					result_.set_desc(utils::String::Format("IssueAsset asset(%s:%s:%d) overflowed(" FMT_I64 " " FMT_I64 ")", key.issuer().c_str(), key.code().c_str(), key.type(), asset_e.amount(), ope.amount()));
					break;
				}
				asset_e.set_amount(amount);
				source_account_->SetAsset(asset_e);
			}

		} while (false);
	}

	void OperationFrm::PayAsset(std::shared_ptr<Environment> environment) {
		const protocol::OperationPayAsset& payAsset = operation_.pay_asset();
		do {
			std::shared_ptr<AccountFrm> dest_account = nullptr;

			if (!environment->GetEntry(payAsset.dest_address(), dest_account)) {
				result_.set_code(protocol::ERRCODE_ACCOUNT_NOT_EXIST);
				result_.set_desc(utils::String::Format("Dest account(%s) is not exist", payAsset.dest_address().c_str()));
				break;
			}

			if (payAsset.has_asset()){
				protocol::AssetStore asset_e;
				protocol::AssetKey key = payAsset.asset().key();
				if (!source_account_->GetAsset(key, asset_e)) {
					result_.set_code(protocol::ERRCODE_ACCOUNT_ASSET_LOW_RESERVE);
					result_.set_desc(utils::String::Format("Failed to get asset(%s:%s:%d)", key.issuer().c_str(), key.code().c_str(), key.type()));
					break;
				}

				if (payAsset.asset().key().type() == 0){
					int64_t sender_amount = 0;
					if (!utils::SafeIntSub(asset_e.amount(), payAsset.asset().amount(), sender_amount)) {
						result_.set_code(protocol::ERRCODE_MATH_OVERFLOW);
						result_.set_desc(utils::String::Format("PayAsset asset(%s:%s:%d) overflowed(" FMT_I64 " " FMT_I64 ")",
							key.issuer().c_str(), key.code().c_str(), key.type(), asset_e.amount(), payAsset.asset().amount()));
						break;
					}

					if (sender_amount < 0) {
						result_.set_code(protocol::ERRCODE_ACCOUNT_ASSET_LOW_RESERVE);
						result_.set_desc(utils::String::Format("asset(%s:%s:%d) is not enought for payment.", key.issuer().c_str(), key.code().c_str(), key.type()));
						break;
					}
					asset_e.set_amount(sender_amount);
					source_account_->SetAsset(asset_e);

					protocol::AssetStore dest_asset;
					if (!dest_account->GetAsset(key, dest_asset)) {
						dest_asset.mutable_key()->CopyFrom(key);
						dest_asset.set_amount(payAsset.asset().amount());
						dest_account->SetAsset(dest_asset);
					}
					else {
						int64_t receiver_amount =0;
						if (!utils::SafeIntAdd(dest_asset.amount(), payAsset.asset().amount(), receiver_amount))
						{
							result_.set_code(protocol::ERRCODE_MATH_OVERFLOW);
							result_.set_desc(utils::String::Format("PayAsset asset(%s:%s:%d) overflowed(" FMT_I64 " " FMT_I64 ")", 
								key.issuer().c_str(), key.code().c_str(), key.type(), dest_asset.amount(), payAsset.asset().amount()));
							break;
						}
						dest_asset.set_amount(receiver_amount);
						dest_account->SetAsset(dest_asset);
					}
				}
				else{
					result_.set_code(protocol::ERRCODE_ASSET_INVALID);
					result_.set_desc(utils::String::Format("payAsset type must be 0"));
					break;
				}
			}
			
			std::string javascript = dest_account->GetProtoAccount().contract().payload();
			if (!javascript.empty()){
				TransactionFrm::pointer bottom_tx = transaction_->ledger_->lpledger_context_->GetBottomTx();
				ContractParameter parameter;
				parameter.code_ = javascript;
				parameter.input_ = payAsset.input();
				parameter.this_address_ = payAsset.dest_address();
				parameter.init_ = false;
				parameter.ledger_context_ = transaction_->ledger_->lpledger_context_;

				//for block
				parameter.block_.number_ = transaction_->ledger_->value_->ledger_seq();
				parameter.block_.timestamp_ = transaction_->ledger_->value_->close_time();

				//for tx
				parameter.tx_.fee_limit_ = bottom_tx->GetFeeLimit();
				parameter.tx_.gas_price_ = bottom_tx->GetGasPrice();
				parameter.tx_.hash_ = utils::String::BinToHexString(bottom_tx->GetContentHash());
				parameter.tx_.initiator_ = bottom_tx->GetSourceAddress();
				parameter.tx_.sender_ = bottom_tx->GetOperatingSourceAddress();

				//for msg
				parameter.msg_.coin_amount_ = 0;
				parameter.msg_.asset_ = payAsset.asset();
				parameter.msg_.initiator_ = transaction_->GetSourceAddress();
				parameter.msg_.nonce_ = transaction_->GetNonce();
				parameter.msg_.operation_index_ = transaction_->GetProcessingOperation();
				parameter.msg_.sender_ = source_account_->GetAccountAddress();

				result_ = ContractManager::Instance().Execute(Contract::TYPE_V8, parameter);
			}
		} while (false);
	}

	void OperationFrm::SetMetaData(std::shared_ptr<Environment> environment) {

		do {
			auto ope = operation_.set_metadata();
			std::string key = ope.key();
			protocol::KeyPair keypair_e ;
			int64_t version = ope.version();
			bool delete_flag = ope.delete_flag();
			if (delete_flag){
				if (source_account_->GetMetaData(key, keypair_e)){
					if (version != 0) {
						if (keypair_e.version() != version) {
							result_.set_code(protocol::ERRCODE_INVALID_DATAVERSION);
							result_.set_desc(utils::String::Format("Data version(" FMT_I64 ") not valid", version));
							break;
						}
					}
					if (key.empty()){
						result_.set_code(protocol::ERRCODE_INVALID_PARAMETER);
						result_.set_desc(utils::String::Format("Data key is empty,key(%s)", key.c_str()));
						break;
					}
					source_account_->DeleteMetaData(keypair_e);
				}
				else{
					result_.set_code(protocol::ERRCODE_NOT_EXIST);
					result_.set_desc(utils::String::Format("The key(%s) to be deleted is not exist", key.c_str()));
					break;
				}
			}
			else{
				if (source_account_->GetMetaData(key, keypair_e)) {

					if (version != 0) {
						if (keypair_e.version() + 1 != version) {
							result_.set_code(protocol::ERRCODE_INVALID_DATAVERSION);
							result_.set_desc(utils::String::Format("Data version(" FMT_I64 ") is not valid", version));
							break;
						}
					}

					keypair_e.set_version(keypair_e.version() + 1);
					keypair_e.set_value(ope.value());
					source_account_->SetMetaData(keypair_e);

				}
				else {
					if (version != 1 && version != 0) {
						result_.set_code(protocol::ERRCODE_INVALID_DATAVERSION);
						result_.set_desc(utils::String::Format("Data version(" FMT_I64 ") is not valid", version));
						break;
					}
					protocol::KeyPair keypair;
					keypair.set_value(ope.value());
					keypair.set_key(ope.key());
					keypair.set_version(1);
					source_account_->SetMetaData(keypair);
				}
			}			
		} while (false);

	}

	void OperationFrm::SetSignerWeight(std::shared_ptr<Environment> environment) {
		const protocol::OperationSetSignerWeight &ope = operation_.set_signer_weight();
		do {

			if (ope.master_weight() >= 0) {
				source_account_->SetProtoMasterWeight(ope.master_weight());
			}

			for (int32_t i = 0; i < ope.signers_size(); i++) {

				int64_t weight = ope.signers(i).weight();
				source_account_->UpdateSigner(ope.signers(i).address(), weight);
			}

		} while (false);
	}

	void OperationFrm::SetThreshold(std::shared_ptr<Environment> environment) {
		const protocol::OperationSetThreshold &ope = operation_.set_threshold();
		std::shared_ptr<AccountFrm> source_account = nullptr;

		do {
			if (ope.tx_threshold() >= 0) {
				source_account_->SetProtoTxThreshold(ope.tx_threshold());
			}

			for (int32_t i = 0; i < ope.type_thresholds_size(); i++) {
				source_account_->UpdateTypeThreshold(ope.type_thresholds(i).type(),
					ope.type_thresholds(i).threshold());
			}
		} while (false);
	}

	void OperationFrm::PayCoin(std::shared_ptr<Environment> environment) {
		auto ope = operation_.pay_coin();
		std::string address = ope.dest_address();
		std::shared_ptr<AccountFrm> dest_account_ptr = nullptr;
		int64_t reserve_coin = LedgerManager::Instance().GetCurFeeConfig().base_reserve();
		do {
			protocol::Account& proto_source_account = source_account_->GetProtoAccount();
			int64_t min_balance=0;
			if (!utils::SafeIntAdd(ope.amount(), reserve_coin, min_balance)){
				result_.set_code(protocol::ERRCODE_MATH_OVERFLOW);
				result_.set_desc(utils::String::Format("PayCoin overflow: account(%s) , amount(" FMT_I64 "), reserve_coin:(" FMT_I64 ") ",
					proto_source_account.address().c_str(), ope.amount(), reserve_coin));
				break;
			}

			if (proto_source_account.balance() < min_balance) {
				result_.set_code(protocol::ERRCODE_ACCOUNT_LOW_RESERVE);
				result_.set_desc(utils::String::Format("Account(%s)'s balance(" FMT_I64 ") - base_reserve(" FMT_I64 ") is not enough for payment (" FMT_I64 ") ",
					proto_source_account.address().c_str(),
					proto_source_account.balance(),
					reserve_coin,
					ope.amount()					
					));
				break;
			}

			if (!environment->GetEntry(address, dest_account_ptr)) {
				if (ope.amount() < reserve_coin) {
					result_.set_code(protocol::ERRCODE_ACCOUNT_INIT_LOW_RESERVE);
					result_.set_desc(utils::String::Format("Account(%s)'s init balance(" FMT_I64 ") is not enough for reserve(" FMT_I64 ")",
						address.c_str(),
						ope.amount(),
						reserve_coin
						));
					break;
				}

				protocol::Account account;
				account.set_balance(0);
				account.mutable_priv()->set_master_weight(1);
				account.mutable_priv()->mutable_thresholds()->set_tx_threshold(1);
				account.set_address(ope.dest_address());
				dest_account_ptr = std::make_shared<AccountFrm>(account);
				environment->AddEntry(ope.dest_address(), dest_account_ptr);

			}

			int64_t src_balance=0;
			if (!utils::SafeIntSub(proto_source_account.balance(), ope.amount(), src_balance)) {
				result_.set_code(protocol::ERRCODE_MATH_OVERFLOW);
				result_.set_desc(utils::String::Format("PayCoin overflow: source account(%s), balance(" FMT_I64 "), payment amount:(" FMT_I64 ") ",
					proto_source_account.address().c_str(), proto_source_account.balance(), ope.amount()));
				break;
			}
			proto_source_account.set_balance(src_balance);
			
			int64_t dest_balance=0;
			protocol::Account& proto_dest_account = dest_account_ptr->GetProtoAccount();
			if (!utils::SafeIntAdd(proto_dest_account.balance(), ope.amount(), dest_balance)) {
				result_.set_code(protocol::ERRCODE_MATH_OVERFLOW);
				result_.set_desc(utils::String::Format("PayCoin overflowed: dest account(%s), balance(" FMT_I64 "), payment amount:(" FMT_I64 ") ",
					proto_dest_account.address().c_str(), proto_dest_account.balance(), ope.amount()));
				break;
			}
			proto_dest_account.set_balance(dest_balance);

			std::string javascript = dest_account_ptr->GetProtoAccount().contract().payload();
			if (!javascript.empty()) {
				TransactionFrm::pointer bottom_tx = transaction_->ledger_->lpledger_context_->GetBottomTx();
				ContractParameter parameter;
				parameter.code_ = javascript;
				parameter.input_ = ope.input();
				parameter.this_address_ = ope.dest_address();
				parameter.ledger_context_ = transaction_->ledger_->lpledger_context_;
				parameter.init_ = false;
				
				//for block
				parameter.block_.number_ = transaction_->ledger_->value_->ledger_seq();
				parameter.block_.timestamp_ = transaction_->ledger_->value_->close_time();

				//for tx
				parameter.tx_.sender_ = bottom_tx->GetOperatingSourceAddress();
				parameter.tx_.initiator_ = bottom_tx->GetSourceAddress();
				parameter.tx_.fee_limit_ = bottom_tx->GetFeeLimit();
				parameter.tx_.gas_price_ = bottom_tx->GetGasPrice();
				parameter.tx_.hash_ = utils::String::BinToHexString(bottom_tx->GetContentHash());

				//for msg
				parameter.msg_.coin_amount_ = ope.amount();
				parameter.msg_.initiator_ = transaction_->GetSourceAddress();
				parameter.msg_.sender_ = source_account_->GetAccountAddress();
				parameter.msg_.operation_index_ = transaction_->GetProcessingOperation();
				parameter.msg_.nonce_ = transaction_->GetNonce();
				

				result_ = ContractManager::Instance().Execute(Contract::TYPE_V8, parameter);
				LOG_TRACE("%s", result_.contract_result().toFastString().c_str());
			}
		} while (false);
	}

	void OperationFrm::Log(std::shared_ptr<Environment> environment) {}

	void OperationFrm::SetPrivilege(std::shared_ptr<Environment> environment) {
		const protocol::OperationSetPrivilege &set_priv_opt = operation_.set_privilege();

		//for master_weight
		if (!set_priv_opt.master_weight().empty()) {
			int64_t master_weight = utils::String::Stoi64(set_priv_opt.master_weight());
			if (master_weight >= 0)
			{
				source_account_->SetProtoMasterWeight(master_weight);
			}
		}

		//for Signer
		for (int32_t i = 0; i < set_priv_opt.signers_size(); i++) {
			int64_t weight = set_priv_opt.signers(i).weight();
			source_account_->UpdateSigner(set_priv_opt.signers(i).address(), weight);
		}

		//for thresholds
		if (!set_priv_opt.tx_threshold().empty()) {
			int64_t tx_threshold = utils::String::Stoi64(set_priv_opt.tx_threshold());
			if (tx_threshold >= 0)
			{
				source_account_->SetProtoTxThreshold(tx_threshold);
			}
		}

		//for type_thresholds
		for (int32_t i = 0; i < set_priv_opt.type_thresholds_size(); i++) {
			source_account_->UpdateTypeThreshold(set_priv_opt.type_thresholds(i).type(),
				set_priv_opt.type_thresholds(i).threshold());
		}
	}

	Result OperationFrm::CheckCreateAccountGt1000(const protocol::OperationCreateAccount& create_account, uint32_t ldcontext_stack_size){
		Result result;
		result.set_code(protocol::ERRCODE_SUCCESS);
		do {
			bool is_create_contract = !create_account.contract().payload().empty();
			const std::string dest_address = create_account.dest_address();
			//If the ledger version is greater than 1000, you must leave the dest address of the smart contract empty. An address will be created automatically.
			bool has_dest_address = !dest_address.empty();
			bool unimportant_address = (dest_address != GET_CONTRACT_VALIDATOR_ADDRESS) && (dest_address != GET_CONTRACT_FEE_ADDRESS);
			if (is_create_contract && has_dest_address && unimportant_address) {
				result.set_code(protocol::ERRCODE_INVALID_ADDRESS);
				result.set_desc(utils::String::Format("The dest address(%s) must be empty when create contract account", dest_address.c_str()));
				break;
			}

			if (!is_create_contract) {
				if (!bumo::PublicKey::IsAddressValid(dest_address)) {
					result.set_code(protocol::ERRCODE_INVALID_ADDRESS);
					result.set_desc(utils::String::Format("Dest account address(%s) invalid", dest_address.c_str()));
					break;
				}
			}

			if (!create_account.has_priv()) {
				result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
				result.set_desc(utils::String::Format("Dest account address(%s) has no priv object", dest_address.c_str()));
				break;
			}

			const protocol::AccountPrivilege &priv = create_account.priv();
			if (priv.master_weight() < 0 || priv.master_weight() > UINT32_MAX) {
				result.set_code(protocol::ERRCODE_WEIGHT_NOT_VALID);
				result.set_desc(utils::String::Format("Master weight(" FMT_I64 ") is larger than %u or less than 0", priv.master_weight(), UINT32_MAX));
				break;
			}

			//for threshold
			if (!priv.has_thresholds()) {
				result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
				result.set_desc(utils::String::Format("dest account address(%s) has no threshold object", dest_address.c_str()));
				break;
			}

			const protocol::AccountThreshold &threshold = priv.thresholds();
			if (threshold.tx_threshold() < 0) {
				result.set_code(protocol::ERRCODE_THRESHOLD_NOT_VALID);
				result.set_desc(utils::String::Format("Low threshold(" FMT_I64 ") is less than 0", threshold.tx_threshold()));
				break;
			}

			std::set<int32_t> duplicate_type;
			for (int32_t i = 0; i < threshold.type_thresholds_size(); i++) {
				const protocol::OperationTypeThreshold  &type_thresholds = threshold.type_thresholds(i);
				if (type_thresholds.type() > 100 || type_thresholds.type() <= 0) {
					result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
					result.set_desc(utils::String::Format("Operation type(%u) is not supported", type_thresholds.type()));
					break;
				}

				if (type_thresholds.threshold() < 0) {
					result.set_code(protocol::ERRCODE_THRESHOLD_NOT_VALID);
					result.set_desc(utils::String::Format("Operation type(%d) threshold(" FMT_I64 ") is less than 0", (int32_t)type_thresholds.type(), type_thresholds.threshold()));
					break;
				}

				if (duplicate_type.find(type_thresholds.type()) != duplicate_type.end()) {
					result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
					result.set_desc(utils::String::Format("Operation type(%u) duplicated", type_thresholds.type()));
					break;
				}

				duplicate_type.insert(type_thresholds.type());
			}

			//If it's contract then {master_weight:0 , thresholds:{tx_threshold:1} }
			if (is_create_contract){
				if (create_account.contract().payload().size() > General::CONTRACT_CODE_LIMIT) {
					result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
					result.set_desc(utils::String::Format("Contract payload size(" FMT_SIZE ") > limit(%d)",
						create_account.contract().payload().size(), General::CONTRACT_CODE_LIMIT));
					break;
				}

				if (!(priv.master_weight() == 0 &&
					priv.signers_size() == 0 &&
					threshold.tx_threshold() == 1 &&
					threshold.type_thresholds_size() == 0
					)) {
					result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
					result.set_desc(utils::String::Format("Contract account 'priv' configuration must be({master_weight:0, thresholds:{tx_threshold:1}})"));
					break;
				}

				std::string src = create_account.contract().payload();
				result = ContractManager::Instance().SourceCodeCheck(Contract::TYPE_V8, src, ldcontext_stack_size);
			}
			else {
				//If it's common and the version is greater than 1000, then you must set master_weight:1 and tx_threshold:1	
				if (!(priv.master_weight() == 1 &&
					priv.signers_size() == 0 &&
					threshold.tx_threshold() == 1 &&
					threshold.type_thresholds_size() == 0
					)) {
					result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
					result.set_desc(utils::String::Format("Common account 'priv' configuration must be({master_weight:1, thresholds:{tx_threshold:1}})"));
					break;
				}
			}

			for (int32_t i = 0; i < create_account.metadatas_size(); i++){
				const auto kp = create_account.metadatas(i);
				if (kp.key().size() == 0 || kp.key().size() > General::METADATA_KEY_MAXSIZE) {
					result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
					result.set_desc(utils::String::Format("Length of the key should be between [1, %d]. key=%s,key.length=%d",
						General::METADATA_KEY_MAXSIZE, kp.key().c_str(), kp.key().length()));
				}

				if (kp.value().size() > General::METADATA_MAX_VALUE_SIZE){
					result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
					result.set_desc(utils::String::Format("Length of the value should be between [1, %d].key=%s,value.length=%d",
						General::METADATA_MAX_VALUE_SIZE, kp.key().c_str(), kp.value().length()));
				}
			}
		} while (false);

		return result;
	}

	Result OperationFrm::CheckSetPrivilege(const protocol::OperationSetPrivilege &set_privilege, const std::string &source_address){
		Result result;
		result.set_code(protocol::ERRCODE_SUCCESS);
		do  {
			//check master weight
			if (!set_privilege.master_weight().empty()) {
				int64_t master_weight = -1;
				bool int64_check = utils::String::SafeStoi64(set_privilege.master_weight(), master_weight);
				if (!int64_check || master_weight <  0 || master_weight > UINT32_MAX) {
					result.set_code(protocol::ERRCODE_WEIGHT_NOT_VALID);
					result.set_desc(utils::String::Format("Parameter master_weight(%s) is larger than %u or less than -1",
						set_privilege.master_weight().c_str(), UINT32_MAX));
					break;
				}
			}

			//for signers
			std::set<std::string> duplicate_set;
			bool shouldBreak = false;
			for (int32_t i = 0; i < set_privilege.signers_size(); i++) {
				const protocol::Signer &signer = set_privilege.signers(i);
				if (signer.weight() < 0 || signer.weight() > UINT32_MAX) {
					result.set_code(protocol::ERRCODE_WEIGHT_NOT_VALID);
					result.set_desc(utils::String::Format("Signer weight(" FMT_I64 ") is larger than %u or less 0", signer.weight(), UINT32_MAX));
					shouldBreak = true;
					break;
				}

				if (signer.address() == source_address) {
					result.set_code(protocol::ERRCODE_INVALID_ADDRESS);
					result.set_desc(utils::String::Format("The signer address(%s) can't be equal to the source address", signer.address().c_str()));
					shouldBreak = true;
					break;
				}

				if (!PublicKey::IsAddressValid(signer.address())) {
					result.set_code(protocol::ERRCODE_INVALID_ADDRESS);
					result.set_desc(utils::String::Format("Signer address(%s) is invalid", signer.address().c_str()));
					shouldBreak = true;
					break;
				}

				if (duplicate_set.find(signer.address()) != duplicate_set.end()) {
					result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
					result.set_desc(utils::String::Format("Signer address(%s) duplicated", signer.address().c_str()));
					shouldBreak = true;
					break;
				}

				duplicate_set.insert(signer.address());
			}

			if (shouldBreak) break;

			//for threshold
			if (!set_privilege.tx_threshold().empty()){
				int64_t tx_threshold = -1;
				bool int64_check = utils::String::SafeStoi64(set_privilege.tx_threshold(), tx_threshold);
				if (!int64_check || tx_threshold < 0) {
					result.set_code(protocol::ERRCODE_THRESHOLD_NOT_VALID);
					result.set_desc(utils::String::Format("Parameter tx_threshold(%s) is less 0",
						set_privilege.tx_threshold().c_str()));
					break;
				}
			}

			//check type and threshold
			std::set<int32_t> duplicate_type;
			for (int32_t i = 0; i < set_privilege.type_thresholds_size(); i++) {
				const protocol::OperationTypeThreshold  &type_thresholds = set_privilege.type_thresholds(i);
				if (type_thresholds.type() > 100 || type_thresholds.type() <= 0) {
					result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
					result.set_desc(utils::String::Format("Operation type(%u) is not supported", type_thresholds.type()));
					break;
				}

				if (type_thresholds.threshold() < 0) {
					result.set_code(protocol::ERRCODE_THRESHOLD_NOT_VALID);
					result.set_desc(utils::String::Format("Operation type(%d) threshold(" FMT_I64 ") is less than 0", (int32_t)type_thresholds.type(), type_thresholds.threshold()));
					break;
				}

				if (duplicate_type.find(type_thresholds.type()) != duplicate_type.end()) {
					result.set_code(protocol::ERRCODE_INVALID_PARAMETER);
					result.set_desc(utils::String::Format("Operation type(%u) duplicated", type_thresholds.type()));
					break;
				}

				duplicate_type.insert(type_thresholds.type());
			}
		} while (false);
		
		return result;
	}

	void OperationFrm::ChangeCreateContractAmount(const int64_t &init_amount, int64_t &changed_amount){
		if (CHECK_VERSION_GT_1001){
			changed_amount = init_amount;
		}
	}
}


