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

#include <overlay/peer_manager.h>
#include <glue/glue_manager.h>
#include <api/websocket_server.h>
#include <monitor/monitor_manager.h>
#include "ledger_manager.h"
#include <contract/contract_manager.h>
#include "fee_calculate.h"

namespace bumo {
	LedgerManager::LedgerManager() : tree_(NULL) {
		check_interval_ = 500 * utils::MICRO_UNITS_PER_MILLI;
		timer_name_ = "Ledger Mananger";
		chain_max_ledger_probaly_ = 0;
	}

	LedgerManager::~LedgerManager() {
		if (tree_) {
			delete tree_;
			tree_ = NULL;
		}
	}

	bool LedgerManager::GetValidators(int64_t seq, protocol::ValidatorSet& validators_set) {
		LedgerFrm frm;
		if (!frm.LoadFromDb(seq)) {
			return false;
		}

		std::string hashhex = utils::String::BinToHexString(frm.GetProtoHeader().validators_hash());
		std::string key = utils::String::Format("validators-%s", hashhex.c_str());
		auto db = Storage::Instance().account_db();
		std::string str;
		if (!db->Get(key, str)) {
			return false;
		}
		return validators_set.ParseFromString(str);
	}

	bool LedgerManager::CheckAndRepairLedgerSeq(){
#ifdef OS_LINUX
		return true;
#endif
		auto ledger_db = Storage::Instance().ledger_db();
		auto account_db = Storage::Instance().account_db();

		std::string ledger_db_seq;
		std::string account_db_seq;
		bool ledger_state = !ledger_db->Get(General::KEY_LEDGER_SEQ, ledger_db_seq);
		bool account_state = !account_db->Get(General::KEY_LEDGER_SEQ, account_db_seq);

		if (ledger_state && account_state){
			return true;
		}

		if (ledger_state) {
			LOG_ERROR("Failed to get ledger seq from ledger-db\n");
			return false;
		}

		if (account_state) {
			LOG_ERROR("Failed to get ledger seq from account-db\n");
			return false;
		}

		int64_t int_ledger_db_seq = utils::String::Stoi64(ledger_db_seq);
		int64_t int_account_db_seq = utils::String::Stoi64(account_db_seq);
		if (int_account_db_seq == int_ledger_db_seq){
			return true;
		}

		if (int_account_db_seq != int_ledger_db_seq - 1) {
			LOG_ERROR("Ledger seq (%s) from ledger-db not equal with seq (%s) + 1 from account-db",
				ledger_db_seq.c_str(), account_db_seq.c_str());
			return false;
		}

		if (!ledger_db->Put(General::KEY_LEDGER_SEQ, account_db_seq)) {
			LOG_ERROR("Failed to set ledger seq to account-db");
			return false;
		}

		LOG_INFO("Set ledger seq to " FMT_I64 " successfully", int_account_db_seq);
		return true;
	}

	bool LedgerManager::Initialize() {
		if (!CheckAndRepairLedgerSeq()){
			LOG_ERROR("Failed to CheckAndRepairLedgerSeq");
			return false;
		}

		HashWrapper::SetLedgerHashType(Configure::Instance().ledger_configure_.hash_type_);

		tree_ = new KVTrie();
		auto batch = std::make_shared<WRITE_BATCH>();
		tree_->Init(Storage::Instance().account_db(), batch, General::ACCOUNT_PREFIX, 4);

		context_manager_.Initialize();

		auto kvdb = Storage::Instance().account_db();
		std::string str_max_seq;
		int64_t seq_kvdb = 0;
		if (kvdb->Get(General::KEY_LEDGER_SEQ, str_max_seq)) {
			seq_kvdb = utils::String::Stoi64(str_max_seq);
			int64_t seq_rational = GetMaxLedger();
			if (seq_kvdb != seq_rational) {
				LOG_ERROR("Failed to ledger_seq from kvdb(" FMT_I64 ") != ledger_seq from rational db(" FMT_I64 ")",
					seq_kvdb, seq_rational);
			}

			LOG_INFO("The maximum ledger sequence that is closed=" FMT_I64, seq_rational);
			last_closed_ledger_ = std::make_shared<LedgerFrm>();
			if (!last_closed_ledger_->LoadFromDb(seq_rational)) {
				return false;
			}
		}
		else {
			if (!CreateGenesisAccount()) {
				LOG_ERROR("Failed to create genesis account.");
				return false;
			}
			seq_kvdb = 1;
		}

		std::string str;
		if (kvdb->Get(General::STATISTICS, str)) {
			statistics_.fromString(str);
		}
		//avoid dead lock
		utils::WriteLockGuard guard(lcl_header_mutex_);
		lcl_header_ = last_closed_ledger_->GetProtoHeader();

		tree_->UpdateHash();
		const protocol::LedgerHeader& lclheader = last_closed_ledger_->GetProtoHeader();
		std::string validators_hash = lclheader.validators_hash();
		if (!ValidatorsGet(validators_hash, validators_)) {
			LOG_ERROR("Failed to get validators!");
			return false;
		}

		//fee
		std::string fees_hash = lclheader.fees_hash();
		if (!FeesConfigGet(fees_hash, fees_)) {
			LOG_ERROR("Failed to get config fee!");
			return false;
		}

		bumo::General::SetSelfChainId(lclheader.chain_id());

		LOG_INFO("Gas price :" FMT_I64 " Base reserve:" FMT_I64 " .", fees_.gas_price(), fees_.base_reserve());

		//load proof
		Storage::Instance().account_db()->Get(General::LAST_PROOF, proof_);

		//Update consensus configuration.
		Global::Instance().GetIoService().post([this]() {
			GlueManager::Instance().UpdateValidators(validators_, proof_);
		});

		std::string account_tree_hash = lclheader.account_tree_hash();
		if (account_tree_hash != tree_->GetRootHash()) {
			LOG_ERROR("ledger account_tree_hash(%s)!=account_root_hash(%s)",
				utils::String::Bin4ToHexString(account_tree_hash).c_str(),
				utils::String::Bin4ToHexString(tree_->GetRootHash()).c_str());
			return false;
		}

		if (lclheader.version() > General::LEDGER_VERSION) {
			PROCESS_EXIT("Consensus ledger version:%d, software ledger version:%d", lclheader.version(), General::LEDGER_VERSION);
		}

		TimerNotify::RegisterModule(this);
		StatusModule::RegisterModule(this);
		return true;
	}

	bool LedgerManager::Exit() {
		LOG_INFO("Ledger manager stoping...");

		if (tree_) {
			delete tree_;
			tree_ = NULL;
		}
		LOG_INFO("Ledger manager stoped. [OK]");
		return true;
	}

	int LedgerManager::GetAccountNum() {
		utils::MutexGuard guard(gmutex_);
		return statistics_["account_count"].asInt();
	}

	utils::ReadWriteLock& LedgerManager::GetTreeMutex()  {
		return tree_mutex_;
	}


	void LedgerManager::OnTimer(int64_t current_time) {
		int64_t next_seq = 0;
		std::set<int64_t> active_peers = PeerManager::Instance().ConsensusNetwork().GetActivePeerIds();
		std::set<int64_t> enable_peers;
		protocol::GetLedgers gl;

		do {
			utils::MutexGuard guard(gmutex_);
			if (current_time - sync_.update_time_ <= 30 * utils::MICRO_UNITS_PER_SEC) {
				return;
			}

			for (auto it = sync_.peers_.begin(); it != sync_.peers_.end();) {
				int64_t pid = it->first;
				if (active_peers.find(pid) == active_peers.end()) {
					it = sync_.peers_.erase(it);
				}
				else {
					it++;
				}
			}

			next_seq = last_closed_ledger_->GetProtoHeader().seq() + 1;

			sync_.update_time_ = current_time;
			LOG_INFO("OnTimer. Request maximum ledger sequence from neighbours. BEGIN");

			gl.set_begin(next_seq);
			gl.set_end(next_seq);
			gl.set_timestamp(current_time);
			gl.set_chain_id(General::GetSelfChainId());

			for (std::set<int64_t>::iterator it = active_peers.begin(); it != active_peers.end(); it++) {
				int64_t pid = *it;
				auto iter = sync_.peers_.find(pid);
				if (iter != sync_.peers_.end()) {
					SyncStat& st = iter->second;
					if (current_time - st.send_time_ > 30 * utils::MICRO_UNITS_PER_SEC) {
						st.send_time_ = 0;
					}
					if (st.probation_ > current_time) {
						continue;
					}
					if (st.send_time_ != 0) {
						continue;
					}
				}
				enable_peers.insert(pid);
			}
			LOG_INFO("OnTimer. Request maximum ledger sequence from neighbours. END");
		} while (false);

		for (auto it = enable_peers.begin(); it != enable_peers.end(); it++)
			RequestConsensusValues(*it, gl, current_time);
	}

	void LedgerManager::OnSlowTimer(int64_t current_time) {

	}

	protocol::LedgerHeader LedgerManager::GetLastClosedLedger() {
		utils::ReadLockGuard guard(lcl_header_mutex_);
		return lcl_header_;
	}

	void LedgerManager::ValidatorsSet(std::shared_ptr<WRITE_BATCH> batch, const protocol::ValidatorSet& validators) {
		//should be recoded
		std::string hash = HashWrapper::Crypto(validators.SerializeAsString());
		batch->Put(utils::String::Format("validators-%s", utils::String::BinToHexString(hash).c_str()), validators.SerializeAsString());
	}

	bool LedgerManager::ValidatorsGet(const std::string& hash, protocol::ValidatorSet& vlidators_set) {
		std::string key = utils::String::Format("validators-%s", utils::String::BinToHexString(hash).c_str());
		auto db = Storage::Instance().account_db();
		std::string str;
		if (!db->Get(key, str)) {
			return false;
		}
		return vlidators_set.ParseFromString(str);
	}

	void LedgerManager::FeesConfigSet(std::shared_ptr<WRITE_BATCH> batch, const protocol::FeeConfig &fee) {
		std::string hash = HashWrapper::Crypto(fee.SerializeAsString());
		batch->Put(utils::String::Format("fees-%s", utils::String::BinToHexString(hash).c_str()), fee.SerializeAsString());
	}

	bool LedgerManager::FeesConfigGet(const std::string& hash, protocol::FeeConfig &fee) {
		std::string key = utils::String::Format("fees-%s", utils::String::BinToHexString(hash).c_str());
		auto db = Storage::Instance().account_db();
		std::string str;
		if (!db->Get(key, str)) {
			return false;
		}
		return fee.ParseFromString(str);
	}

	bool LedgerManager::CreateGenesisAccount() {
		LOG_INFO("There is no ledger, then create an init ledger.");

		//Set the calculated hash values in the global ledger header.
		int32_t account_count = 0;
		//Create the genesis account.
		AccountFrm::pointer acc_frm = AccountFrm::CreatAccountFrm(Configure::Instance().genesis_configure_.account_, 100000000000000000);
		tree_->Set(DecodeAddress(acc_frm->GetAccountAddress()), acc_frm->Serializer());
		account_count++;

		//Load the list of validators. Create validators' accounts.
		const utils::StringList &list = Configure::Instance().genesis_configure_.validators_;
		for (utils::StringList::const_iterator iter = list.begin(); iter != list.end(); iter++) {
			if (*iter == Configure::Instance().genesis_configure_.account_){
				LOG_ERROR("Validator's address could not be equal to genesis account address.");
				return false;
			}

			auto validator = validators_.add_validators();
			validator->set_address(*iter);
			validator->set_pledge_coin_amount(0);

			AccountFrm::pointer acc_frm = AccountFrm::CreatAccountFrm(*iter, 0);
			tree_->Set(DecodeAddress(acc_frm->GetAccountAddress()), acc_frm->Serializer());
			account_count++;
		}
		statistics_["account_count"] = account_count;

		tree_->UpdateHash();
		protocol::Ledger ledger;
		protocol::LedgerHeader *header = ledger.mutable_header();
		protocol::ConsensusValue request;
		request.mutable_ledger_upgrade()->set_new_ledger_version(1000);
		request.set_close_time(0);

		header->set_previous_hash("");
		header->set_seq(1);
		header->set_close_time(0);
		header->set_consensus_value_hash(HashWrapper::Crypto(request.SerializeAsString()));
		header->set_chain_id(Configure::Instance().genesis_configure_.chain_id_);

		header->set_version(1000);
		header->set_tx_count(0);
		header->set_account_tree_hash(tree_->GetRootHash());

		//for validators
		std::string validators_hash = HashWrapper::Crypto(validators_.SerializeAsString());
		header->set_validators_hash(validators_hash);

		//for fee
		fees_.set_gas_price(Configure::Instance().genesis_configure_.fees_.gas_price_);
		fees_.set_base_reserve(Configure::Instance().genesis_configure_.fees_.base_reserve_);
		std::string fees_hash = HashWrapper::Crypto(fees_.SerializeAsString());
		header->set_fees_hash(fees_hash);
		header->set_reserve(Configure::Instance().genesis_configure_.slogan_);
		header->set_hash("");
		header->set_hash(HashWrapper::Crypto(ledger.SerializeAsString()));

		last_closed_ledger_ = std::make_shared<LedgerFrm>();
		last_closed_ledger_->ProtoLedger().CopyFrom(ledger);
		auto batch = tree_->batch_;
		batch->Put(bumo::General::KEY_LEDGER_SEQ, "1");
		batch->Put(bumo::General::KEY_GENE_ACCOUNT, Configure::Instance().genesis_configure_.account_);
		ValidatorsSet(batch, validators_);
		FeesConfigSet(batch, fees_);

		WRITE_BATCH batch_ledger;
		if (!last_closed_ledger_->AddToDb(batch_ledger)) {
			PROCESS_EXIT("AddToDb failed");
		}

		batch->Put(General::STATISTICS, statistics_.toFastString());
		if (!Storage::Instance().account_db()->WriteBatch(*batch)) {
			PROCESS_EXIT("Failed to write account to database, %s", Storage::Instance().account_db()->error_desc().c_str());
		}

		return true;
	}

	//warn
	void LedgerManager::CreateHardforkLedger() {
		LOG_INFO("Are you sure to create hardfork ledger? Press y to continue.");
		char ch;
		std::cin >> ch;
		if (ch != 'y') {
			LOG_INFO("Do nothing.");
			return;
		}

		//get last ledger
		do {
			HashWrapper::SetLedgerHashType(Configure::Instance().ledger_configure_.hash_type_);
			KeyValueDb *account_db = Storage::Instance().account_db();

			//load max seq
			std::string str_max_seq;
			account_db->Get(General::KEY_LEDGER_SEQ, str_max_seq);
			int64_t seq_kvdb = utils::String::Stoi64(str_max_seq);
			LOG_INFO("The maximum ledger sequence that is closed =" FMT_I64, seq_kvdb);

			//load ledger from db
			protocol::LedgerHeader last_closed_ledger_hdr;
			bumo::KeyValueDb *ledger_db = bumo::Storage::Instance().ledger_db();
			std::string ledger_header;
			if (ledger_db->Get(ComposePrefix(General::LEDGER_PREFIX, seq_kvdb), ledger_header) <= 0) {
				LOG_ERROR("Failed to load ledger from database, error(%s)", ledger_db->error_desc().c_str());
				break;
			}
			if (!last_closed_ledger_hdr.ParseFromString(ledger_header)) {
				LOG_ERROR("Failed to parse last closed ledger.");
				break;
			}

			//load validators
			protocol::ValidatorSet validator_set;
			ValidatorsGet(last_closed_ledger_hdr.validators_hash(), validator_set);

			//load proof
			std::string str_proof;
			account_db->Get(General::LAST_PROOF, str_proof);

			//this validator 
			PrivateKey private_key(Configure::Instance().ledger_configure_.validation_privatekey_);
			std::string this_node_address = private_key.GetEncAddress();

			//Compose the new ledger
			LedgerFrm::pointer ledger_frm = std::make_shared<LedgerFrm>();
			protocol::Ledger &ledger = ledger_frm->ProtoLedger();
			protocol::LedgerHeader *header = ledger.mutable_header();
			protocol::ConsensusValue request;
			protocol::LedgerUpgrade *ledger_upgrade = request.mutable_ledger_upgrade();
			ledger_upgrade->set_new_validator(this_node_address);
			//for validators
			protocol::ValidatorSet new_validator_set;
			auto validator = new_validator_set.add_validators();
			validator->set_address(this_node_address);
			validator->set_pledge_coin_amount(0);

			request.set_previous_ledger_hash(last_closed_ledger_hdr.hash());
			request.set_close_time(last_closed_ledger_hdr.close_time() + Configure::Instance().ledger_configure_.close_interval_);
			request.set_ledger_seq(last_closed_ledger_hdr.seq() + 1);
			request.set_previous_proof(str_proof);
			std::string consensus_value_hash = HashWrapper::Crypto(request.SerializeAsString());

			header->set_previous_hash(last_closed_ledger_hdr.hash());
			header->set_seq(request.ledger_seq());
			header->set_close_time(request.close_time());
			header->set_consensus_value_hash(consensus_value_hash);
			header->set_chain_id(last_closed_ledger_hdr.chain_id());
			header->set_version(last_closed_ledger_hdr.version());
			header->set_tx_count(last_closed_ledger_hdr.tx_count());
			header->set_fees_hash(last_closed_ledger_hdr.fees_hash());

			std::string validators_hash = HashWrapper::Crypto(new_validator_set.SerializeAsString());
			header->set_validators_hash(validators_hash);

			//calculate block reward
			ProposeTxsResult prop_result;
			ledger_frm->ApplyPropose(request, NULL, prop_result);
			int64_t new_count = 0, change_count = 0;
			ledger_frm->Commit(LedgerManager::GetInstance()->tree_, new_count, change_count);

			//Update account hash
			LedgerManager::GetInstance()->tree_->UpdateHash();
			header->set_account_tree_hash(LedgerManager::GetInstance()->tree_->GetRootHash());

			//Write account db
			auto batch_account = LedgerManager::GetInstance()->tree_->batch_;
			if (!Storage::Instance().account_db()->WriteBatch(*batch_account)) {
				PROCESS_EXIT("Failed to write account to database, %s", Storage::Instance().account_db()->error_desc().c_str());
			}

			header->set_hash(HashWrapper::Crypto(ledger_frm->ProtoLedger().SerializeAsString()));

			std::shared_ptr<WRITE_BATCH> batch = std::make_shared<WRITE_BATCH>();
			batch->Put(bumo::General::KEY_LEDGER_SEQ, utils::String::ToString(header->seq()));
			batch->Put(bumo::General::LAST_PROOF, "");

			ValidatorsSet(batch, new_validator_set);

			//Write ledger db
			WRITE_BATCH batch_ledger;
			batch_ledger.Put(bumo::General::KEY_LEDGER_SEQ, utils::String::ToString(header->seq()));
			batch_ledger.Put(ComposePrefix(General::LEDGER_PREFIX, header->seq()), header->SerializeAsString());
			batch_ledger.Put(ComposePrefix(General::CONSENSUS_VALUE_PREFIX, header->seq()), request.SerializeAsString());
			if (!ledger_db->WriteBatch(batch_ledger)) {
				PROCESS_EXIT("Failed to write ledger and transaction to database(%s)", ledger_db->error_desc().c_str());
			}

			//Write account db
			if (!Storage::Instance().account_db()->WriteBatch(*batch)) {
				PROCESS_EXIT("Failed to write account to database, %s", Storage::Instance().account_db()->error_desc().c_str());
			}

			LOG_INFO("Created hard fork ledger successfully: sequence(" FMT_I64 "), consensus value hash(%s)",
				header->seq(),
				utils::String::BinToHexString(header->consensus_value_hash()).c_str());

		} while (false);
	}


	bool LedgerManager::ConsensusValueFromDB(int64_t seq, protocol::ConsensusValue& consensus_value) {
		KeyValueDb *ledger_db = Storage::Instance().ledger_db();
		std::string str_cons;
		if (ledger_db->Get(ComposePrefix(General::CONSENSUS_VALUE_PREFIX, seq), str_cons) <= 0) {
			return false;
		}

		return consensus_value.ParseFromString(str_cons);
	}

	protocol::FeeConfig LedgerManager::GetCurFeeConfig() {
		utils::ReadLockGuard guard(fee_config_mutex_);
		return fees_;
	}

	int LedgerManager::OnConsent(const protocol::ConsensusValue &consensus_value, const std::string& proof) {
		LOG_INFO("OnConsent ledger: consensus value sequence(" FMT_I64 ")", consensus_value.ledger_seq());

		utils::MutexGuard guard(gmutex_);
		if (last_closed_ledger_->GetProtoHeader().seq() >= consensus_value.ledger_seq()) {
			LOG_ERROR("Received duplicated consensus, maximum ledger closed(" FMT_I64 ")>= received request(" FMT_I64 ")",
				last_closed_ledger_->GetProtoHeader().seq(),
				consensus_value.ledger_seq());
			return 1;
		}

		if (last_closed_ledger_->GetProtoHeader().seq() + 1 == consensus_value.ledger_seq()) {
			sync_.update_time_ = utils::Timestamp::HighResolution();
			CloseLedger(consensus_value, proof);
		}
		return 0;
	}

	int64_t LedgerManager::GetMaxLedger() {
		KeyValueDb *ledger_db = Storage::Instance().ledger_db();
		std::string str_value;
		int32_t ret = ledger_db->Get(General::KEY_LEDGER_SEQ, str_value);
		if (ret < 0) {
			PROCESS_EXIT_ERRNO("Failed to get max ledger sequence, error desc(%s)", ledger_db->error_desc().c_str());
		}
		else if (ret == 0) {
			return 0;
		}

		return utils::String::Stoi64(str_value);
	}

	void LedgerManager::GetModuleStatus(Json::Value &data) {
		utils::MutexGuard guard(gmutex_);
		int64_t begin_time = utils::Timestamp::HighResolution();
		data["name"] = "ledger_manager";
		data["tx_count"] = GetLastClosedLedger().tx_count();
		data["account_count"] = GetAccountNum();
		data["validation_address"] = Configure::Instance().ledger_configure_.validation_random ? "" : Configure::Instance().ledger_configure_.validation_address_;
		data["ledger_sequence"] = GetLastClosedLedger().seq(); 
		data["time"] = utils::String::Format(FMT_I64 " ms",
			(utils::Timestamp::HighResolution() - begin_time) / utils::MICRO_UNITS_PER_MILLI);
		data["hash_type"] = HashWrapper::GetLedgerHashType() == HashWrapper::HASH_TYPE_SM3 ? "sm3" : "sha256";
		data["sync"] = sync_.ToJson();
		context_manager_.GetModuleStatus(data["ledger_context"]);

		data["chain_max_ledger_seq"] = chain_max_ledger_probaly_ > data["ledger_sequence"].asInt64() ?
		chain_max_ledger_probaly_ : data["ledger_sequence"].asInt64();
	}

	bool LedgerManager::CloseLedger(const protocol::ConsensusValue& consensus_value, const std::string& proof) {
		if (!GlueManager::Instance().CheckValueAndProof(consensus_value.SerializeAsString(), proof)) {

			protocol::PbftProof proof_proto;
			proof_proto.ParseFromString(proof);

			LOG_ERROR("Failed to execute CheckValueAndProof function:%s\n proof=%s",
				Proto2Json(consensus_value).toFastString().c_str(),
				Proto2Json(proof_proto).toFastString().c_str());

			return false;
		}

		std::string con_str = consensus_value.SerializeAsString();
		std::string chash = HashWrapper::Crypto(con_str);
		LedgerFrm::pointer closing_ledger = context_manager_.SyncProcess(consensus_value);
		if (closing_ledger == NULL){
			return false;
		}

		protocol::Ledger& ledger = closing_ledger->ProtoLedger();
		auto header = ledger.mutable_header();
		header->set_seq(consensus_value.ledger_seq());
		header->set_close_time(consensus_value.close_time());
		header->set_previous_hash(consensus_value.previous_ledger_hash());
		header->set_consensus_value_hash(chash);
		header->set_chain_id(General::GetSelfChainId());
		header->set_version(last_closed_ledger_->GetProtoHeader().version());

		int64_t time0 = utils::Timestamp().HighResolution();
		int64_t new_count = 0, change_count = 0;
		closing_ledger->Commit(tree_, new_count, change_count);
		statistics_["account_count"] = statistics_["account_count"].asInt64() + new_count;

		int64_t time1 = utils::Timestamp().HighResolution();

		tree_->UpdateHash();
		int64_t time2 = utils::Timestamp().HighResolution();

		header->set_account_tree_hash(tree_->GetRootHash());
		header->set_tx_count(last_closed_ledger_->GetProtoHeader().tx_count() + closing_ledger->ProtoLedger().transaction_envs_size());

		protocol::ValidatorSet new_set;
		bool has_upgrade = consensus_value.has_ledger_upgrade();
		if (has_upgrade) {
			const protocol::LedgerUpgrade &ledger_upgrade = consensus_value.ledger_upgrade();

			//for ledger version
			if (ledger_upgrade.new_ledger_version() > 0) {
				header->set_version(ledger_upgrade.new_ledger_version());
			}

			//for hardfork new validator
			if (ledger_upgrade.new_validator().size() > 0) {
				new_set.Clear();
				auto newValidator = new_set.add_validators();
				newValidator->set_address(ledger_upgrade.new_validator());
				newValidator->set_pledge_coin_amount(0);
			}
		}

		header->set_hash("");

		int64_t ledger_seq = closing_ledger->GetProtoHeader().seq();
		std::shared_ptr<WRITE_BATCH> account_db_batch = tree_->batch_;
		account_db_batch->Put(bumo::General::KEY_LEDGER_SEQ, utils::String::Format(FMT_I64, ledger_seq));

		//for validator upgrade
		if (new_set.validators_size() > 0 || closing_ledger->environment_->GetVotedValidators(validators_, new_set)) {
			ValidatorsSet(account_db_batch, new_set);
			validators_ = new_set;
		}
		header->set_validators_hash(HashWrapper::Crypto(new_set.SerializeAsString()));//TODO

		//for fee
		protocol::FeeConfig new_fees;
		if (closing_ledger->environment_->GetVotedFee(fees_, new_fees)) {
			FeesConfigSet(account_db_batch, new_fees);
			utils::WriteLockGuard guard(fee_config_mutex_);
			fees_ = new_fees;
		}
		header->set_fees_hash(HashWrapper::Crypto(fees_.SerializeAsString()));

		//This header must be for the latest block.
		header->set_hash(HashWrapper::Crypto(closing_ledger->ProtoLedger().SerializeAsString()));

		//proof
		account_db_batch->Put(bumo::General::LAST_PROOF, proof);
		account_db_batch->Put(bumo::General::STATISTICS, statistics_.toFastString());
		proof_ = proof;

		//consensus value
		WRITE_BATCH ledger_db_batch;
		ledger_db_batch.Put(ComposePrefix(General::CONSENSUS_VALUE_PREFIX, consensus_value.ledger_seq()), consensus_value.SerializeAsString());

		do {
			utils::WriteLockGuard guard(Storage::Instance().account_ledger_lock_);

			if (!closing_ledger->AddToDb(ledger_db_batch)) {
				PROCESS_EXIT("Failed to write ledger to database.");
			}

			if (!Storage::Instance().account_db()->WriteBatch(*account_db_batch)) {
				PROCESS_EXIT("Failed to write accounts to database: %s", Storage::Instance().account_db()->error_desc().c_str());
			}

		} while (false);

		//Update the variable when the write is successful.
		last_closed_ledger_ = closing_ledger;

		int64_t time3 = utils::Timestamp().HighResolution();
		tree_->batch_ = std::make_shared<WRITE_BATCH>();
		{
			utils::WriteLockGuard guard(tree_mutex_);
			tree_->FreeMemory(4);
		}
		LOG_INFO("ledger(" FMT_I64 "): closed transaction count(" FMT_SIZE "), ledger hash(%s), time of apply ledger ="  FMT_I64_EX(-8) " time of calculating hash="  FMT_I64_EX(-8) " time of addtodb=" FMT_I64_EX(-8)
			" total=" FMT_I64_EX(-8) " LoadValue=" FMT_I64 " tsize=" FMT_SIZE,
			closing_ledger->GetProtoHeader().seq(),
			closing_ledger->GetTxOpeCount(),
			utils::String::Bin4ToHexString(closing_ledger->GetProtoHeader().hash()).c_str(),
			time1 - time0 + closing_ledger->apply_time_,
			time2 - time1,
			time3 - time2,
			time3 - time0 + closing_ledger->apply_time_,
			tree_->time_,
			closing_ledger->GetTxCount());

		NotifyLedgerClose(closing_ledger, has_upgrade);

		return true;
	}

	void LedgerManager::NotifyLedgerClose(LedgerFrm::pointer closing_ledger, bool has_upgrade) {
		//Avoid dead lock
		protocol::LedgerHeader tmp_lcl_header;
		do {
			utils::WriteLockGuard guard(lcl_header_mutex_);
			tmp_lcl_header = lcl_header_ = last_closed_ledger_->GetProtoHeader();
		} while (false);

		protocol::ValidatorSet tmp_v = validators_;
		std::string tmp_proof = proof_;
		Global::Instance().GetIoService().post([tmp_v, tmp_proof, has_upgrade]() { //avoid deadlock
			GlueManager::Instance().UpdateValidators(tmp_v, tmp_proof);
			if (has_upgrade) GlueManager::Instance().LedgerHasUpgrade();
		});

		context_manager_.RemoveCompleted(tmp_lcl_header.seq());

		//Broadcast that the ledger is closed.
		WebSocketServer::Instance().BroadcastMsg(protocol::CHAIN_LEDGER_HEADER, tmp_lcl_header.SerializeAsString());

		// The broadcast message is applied.
		for (size_t i = 0; i < closing_ledger->apply_tx_frms_.size(); i++) {
			TransactionFrm::pointer tx = closing_ledger->apply_tx_frms_[i];
			protocol::TransactionEnvStore apply_tx_msg;
			*apply_tx_msg.mutable_transaction_env() = closing_ledger->apply_tx_frms_[i]->GetTransactionEnv();
			apply_tx_msg.set_ledger_seq(closing_ledger->GetProtoHeader().seq());
			apply_tx_msg.set_close_time(closing_ledger->GetProtoHeader().close_time());
			apply_tx_msg.set_error_code(tx->GetResult().code());
			apply_tx_msg.set_error_desc(tx->GetResult().desc());
			apply_tx_msg.set_hash(tx->GetContentHash());
			if (tx->GetResult().code() != 0){
				apply_tx_msg.set_actual_fee(tx->GetFeeLimit());
			}
			else {
				int64_t actual_fee = 0;
				if (!utils::SafeIntMul(tx->GetActualGas(), tx->GetGasPrice(), actual_fee)){
					LOG_ERROR("Overflowed when caculate actual fee.");
				}
				apply_tx_msg.set_actual_fee(actual_fee);
			}

			WebSocketServer::Instance().BroadcastChainTxMsg(apply_tx_msg);

			if (tx->GetResult().code() == protocol::ERRCODE_SUCCESS)
			for (size_t j = 0; j < tx->instructions_.size(); j++) {
				const protocol::TransactionEnvStore &env_sto = tx->instructions_.at(j);
				WebSocketServer::Instance().BroadcastChainTxMsg(env_sto);
			}
		}
		// notice dropped
		/*
		for (size_t i = 0; i < closing_ledger->dropped_tx_frms_.size(); i++) {
		TransactionFrm::pointer tx = closing_ledger->dropped_tx_frms_[i];
		protocol::TransactionEnvStore dropTxMsg;
		*dropTxMsg.mutable_transaction_env() = closing_ledger->dropped_tx_frms_[i]->GetTransactionEnv();
		dropTxMsg.set_ledger_seq(closing_ledger->GetProtoHeader().seq());
		dropTxMsg.set_close_time(closing_ledger->GetProtoHeader().close_time());
		dropTxMsg.set_error_code(tx->GetResult().code());
		dropTxMsg.set_error_desc(tx->GetResult().desc());
		dropTxMsg.set_hash(tx->GetContentHash());
		WebSocketServer::Instance().BroadcastChainTxMsg(dropTxMsg);
		}
		*/

		// monitor
		monitor::LedgerStatus ledger_status;
		ledger_status.mutable_ledger_header()->CopyFrom(tmp_lcl_header);
		ledger_status.set_transaction_size(GlueManager::Instance().GetTransactionCacheSize());
		ledger_status.set_account_count(GetAccountNum());
		ledger_status.set_timestamp(utils::Timestamp::HighResolution());
		MonitorManager::Instance().SendMonitor(monitor::MONITOR_MSGTYPE_LEDGER, ledger_status.SerializeAsString());
	}


	void LedgerManager::OnRequestLedgers(const protocol::GetLedgers &message, int64_t peer_id) {
		if (message.chain_id() != General::GetSelfChainId()){
			LOG_TRACE("Failed to check same chain, node self id(" FMT_I64 ") is not eq (" FMT_I64 ")",
				General::GetSelfChainId(), message.chain_id());
			return;
		}

		bool ret = true;
		protocol::Ledgers ledgers;
		ledgers.set_chain_id(General::GetSelfChainId());

		do {
			utils::MutexGuard guard(gmutex_);
			LOG_TRACE("OnRequestLedgers pid(" FMT_I64 "),[" FMT_I64 ", " FMT_I64 "]", peer_id, message.begin(), message.end());
			if (message.end() - message.begin() + 1 > 5) {
				LOG_ERROR("Only 5 blocks can be requested at a time while try to (" FMT_I64 ")", message.end() - message.begin());
				return;
			}

			if (message.end() - message.begin() < 0) {
				LOG_ERROR("Begin is greater than end [" FMT_I64 "," FMT_I64 "]", message.begin(), message.end());
				return;
			}

			if (last_closed_ledger_->GetProtoHeader().seq() < message.end()) {
				LOG_INFO("Peer node(" FMT_I64 ") request ledger[" FMT_I64 "," FMT_I64 "] while the max consensus value is (" FMT_I64 ")",
					peer_id, message.begin(), message.end(), last_closed_ledger_->GetProtoHeader().seq());
				return;
			}


			ledgers.set_max_seq(last_closed_ledger_->GetProtoHeader().seq());

			for (int64_t i = message.begin(); i <= message.end(); i++) {
				protocol::ConsensusValue item;
				if (!ConsensusValueFromDB(i, item)) {
					ret = false;
					LOG_ERROR("Failed to get consensus value from database: consensus value sequence=" FMT_I64, i);
					break;
				}
				ledgers.add_values()->CopyFrom(item);
			}

			int64_t seq = message.end();
			protocol::ConsensusValue next;

			if (seq == last_closed_ledger_->GetProtoHeader().seq())
				ledgers.set_proof(proof_);
			else if (ConsensusValueFromDB(seq + 1, next))
				ledgers.set_proof(next.previous_proof());
			else {
				LOG_ERROR("");
			}
		} while (false);
		if (ret) {
			bumo::WsMessagePointer ws = std::make_shared<protocol::WsMessage>();
			ws->set_data(ledgers.SerializeAsString());
			ws->set_type(protocol::OVERLAY_MSGTYPE_LEDGERS);
			ws->set_request(false);
			LOG_TRACE("Send ledgers[" FMT_I64 "," FMT_I64 "] to(" FMT_I64 ")", message.begin(), message.end(), peer_id);
			PeerManager::Instance().ConsensusNetwork().SendMsgToPeer(peer_id, ws);
		}
	}

	void LedgerManager::OnReceiveLedgers(const protocol::Ledgers &ledgers, int64_t peer_id) {
		if (ledgers.chain_id() != General::GetSelfChainId()){
			LOG_TRACE("Failed to check same chain, node self id(" FMT_I64 ") is not eq (" FMT_I64 ")",
				General::GetSelfChainId(), ledgers.chain_id());
			return;
		}

		bool valid = false;
		int64_t next = 0;

		do {
			utils::MutexGuard guard(gmutex_);
			if (ledgers.values_size() == 0) {
				LOG_ERROR("Received empty Ledgers from(" FMT_I64 ")", peer_id);
				break;
			}
			int64_t begin = ledgers.values(0).ledger_seq();
			int64_t end = ledgers.values(ledgers.values_size() - 1).ledger_seq();

			LOG_INFO("OnReceiveLedgers [" FMT_I64 "," FMT_I64 "] from peer node(" FMT_I64 ")", begin, end, peer_id);

			if (sync_.peers_.find(peer_id) == sync_.peers_.end()) {
				LOG_ERROR("Received unexpected ledgers [" FMT_I64 "," FMT_I64 "] from (" FMT_I64 ")",
					begin, end, peer_id);
				break;
			}

			auto& itm = sync_.peers_[peer_id];
			itm.send_time_ = 0;

			if (begin != itm.gl_.begin() || end != itm.gl_.end()) {
				LOG_ERROR("Received unexpected ledgers[" FMT_I64 "," FMT_I64 "] while expect[" FMT_I64 "," FMT_I64 "]",
					begin, end, itm.gl_.begin(), itm.gl_.end());
				itm.probation_ = utils::Timestamp::HighResolution() + 60 * utils::MICRO_UNITS_PER_SEC;
				itm.gl_.set_begin(0);
				itm.gl_.set_end(0);
				break;
			}

			itm.gl_.set_begin(0);
			itm.gl_.set_end(0);

			for (int i = 0; i < ledgers.values_size(); i++) {
				const protocol::ConsensusValue& consensus_value = ledgers.values(i);
				std::string proof;
				if (i < ledgers.values_size() - 1) {
					proof = ledgers.values(i + 1).previous_proof();
				}
				else {
					proof = ledgers.proof();
				}
				if (consensus_value.ledger_seq() == last_closed_ledger_->GetProtoHeader().seq() + 1) {
					if (!CloseLedger(consensus_value, proof)) {
						valid = false;
						itm.probation_ = utils::Timestamp::HighResolution() + 60 * utils::MICRO_UNITS_PER_SEC;
						break;
					}
					valid = true;
				}
			}
			next = last_closed_ledger_->GetProtoHeader().seq() + 1;
		} while (false);


		if (valid) {
			int64_t current_time = utils::Timestamp::HighResolution();
			if (next <= ledgers.max_seq()) {
				protocol::GetLedgers gl;
				gl.set_begin(next);
				gl.set_end(MIN(ledgers.max_seq(), next + 4));
				gl.set_timestamp(current_time);
				gl.set_chain_id(General::GetSelfChainId());
				RequestConsensusValues(peer_id, gl, current_time);
			}

			if (ledgers.max_seq() > chain_max_ledger_probaly_) {
				chain_max_ledger_probaly_ = ledgers.max_seq();
			}
		}
	}

	void LedgerManager::RequestConsensusValues(int64_t pid, protocol::GetLedgers& gl, int64_t time) {
		LOG_TRACE("Request consensus values from peer(" FMT_I64 "), [" FMT_I64 "," FMT_I64 "]", pid, gl.begin(), gl.end());
		do {
			utils::MutexGuard guard(gmutex_);
			auto &peer = sync_.peers_[pid];
			peer.gl_.CopyFrom(gl);
			peer.send_time_ = time;
			sync_.update_time_ = time;
		} while (false);

		PeerManager::Instance().ConsensusNetwork().SendRequest(pid, protocol::OVERLAY_MSGTYPE_LEDGERS, gl.SerializeAsString());
	}

	Result LedgerManager::DoTransaction(protocol::TransactionEnv& env, LedgerContext *ledger_context) {
		Result result;
		TransactionFrm::pointer top_tx = ledger_context->GetTopTx();
		std::shared_ptr<AccountFrm> source_account;
		top_tx->environment_->GetEntry(env.transaction().source_address(), source_account);
		env.mutable_transaction()->set_nonce(source_account->GetAccountNonce() + 1);

		auto header = std::make_shared<protocol::LedgerHeader>(ledger_context->closing_ledger_->GetProtoHeader());

		TransactionFrm::pointer new_tx = std::make_shared<bumo::TransactionFrm >(env);
		TransactionFrm::pointer bottom_tx = ledger_context->GetBottomTx();
		do {
			if (!CheckContractDepthSafe(ledger_context->transaction_stack_.size())) {
				new_tx->result_.set_code(protocol::ERRCODE_CONTRACT_TOO_MANY_RECURSION);
				new_tx->result_.set_desc("Too many recursion.");
				//Add byte fee
				TransactionFrm::pointer bottom_tx = ledger_context->GetBottomTx();
				bottom_tx->AddActualGas(new_tx->GetSelfGas());
				break;
			}

			ledger_context->transaction_stack_.push_back(new_tx);
			new_tx->NonceIncrease(ledger_context->closing_ledger_.get(), top_tx->environment_);
			if (new_tx->ValidForParameter(ledger_context->transaction_stack_.size())) {
				std::shared_ptr<Environment> cacheEnv = top_tx->environment_->NewStackFrameEnv();
				new_tx->Apply(ledger_context->closing_ledger_.get(), cacheEnv, true);
			}
			else {
				TransactionFrm::AddActualFee(bottom_tx, new_tx.get());
			}

			//Throw the contract
			if (new_tx->GetResult().code() == protocol::ERRCODE_FEE_NOT_ENOUGH ||
				new_tx->GetResult().code() == protocol::ERRCODE_CONTRACT_TOO_MANY_TRANSACTIONS) {
				result = new_tx->GetResult();
				LOG_ERROR("%s", new_tx->GetResult().desc().c_str());
				// Version check condition can be remove after version 1002
				if (CHECK_VERSION_GT_1002) {
					ledger_context->transaction_stack_.pop_back();
				}
				return result;
			}

			protocol::TransactionEnvStore tx_store;
			tx_store.mutable_transaction_env()->CopyFrom(new_tx->GetProtoTxEnv());
			auto trigger = tx_store.mutable_transaction_env()->mutable_trigger();
			trigger->mutable_transaction()->set_hash(top_tx->GetContentHash());
			trigger->mutable_transaction()->set_index(top_tx->GetProcessingOperation());


			if (new_tx->GetResult().code() == protocol::ERRCODE_SUCCESS) {
				top_tx->instructions_.insert(top_tx->instructions_.end(), new_tx->instructions_.begin(), new_tx->instructions_.end());
				new_tx->environment_->Commit();
			}

			/* txfrm->environment_ was created in txfrm->Apply(...) when txfrm->ValidForParameter() == true,
			   so if txfrm->ValidForParameter() failed, there was no txfrm->environment_,
			   and txfrm->environment_->ClearChangeBuf() would access nonexistent memory,
			   so that would be a serious error, and also seriously is that this problem only
			   exists when operation called by the contract, it means only in Dotransaction function,
			   because there is only one environment in normal operation, txfrm->environment_ is a reference to it*/
			//txfrm->environment_->ClearChangeBuf();
			tx_store.set_error_code(new_tx->GetResult().code());
			tx_store.set_error_desc(new_tx->GetResult().desc());

			top_tx->instructions_.push_back(tx_store);
			ledger_context->transaction_stack_.pop_back();

			result = new_tx->GetResult();
			return result;
		} while (false);

		protocol::TransactionEnvStore tx_store;
		tx_store.set_error_code(new_tx->GetResult().code());
		tx_store.set_error_desc(new_tx->GetResult().desc());

		tx_store.mutable_transaction_env()->CopyFrom(new_tx->GetProtoTxEnv());
		auto trigger = tx_store.mutable_transaction_env()->mutable_trigger();
		trigger->mutable_transaction()->set_hash(top_tx->GetContentHash());
		trigger->mutable_transaction()->set_index(top_tx->GetProcessingOperation());
		top_tx->instructions_.push_back(tx_store);

		result = new_tx->GetResult();
		return result;
	}

	bool LedgerManager::CheckContractDepthSafe(uint32_t tx_size){
		//New version check in ContractManager::Execute and Query.
		if (CHECK_VERSION_GT_1001){
			return true;
		}

		//Old version max depth 4, the 4th can't write or call contract, throw exception. tx_size donot include current tx.
		if (tx_size < General::CONTRACT_MAX_RECURSIVE_DEPTH) {
			return true;
		}

		return false;
	}
}
