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
#include <utils/headers.h>
#include <common/general.h>
#include <main/configure.h>
#include <overlay/peer_manager.h>
#include <ledger/ledger_manager.h>
#include <api/websocket_server.h>
#include "glue_manager.h"

namespace bumo {

	int64_t const  MAX_LEDGER_TIMESPAN_SECONDS = 20 * utils::MICRO_UNITS_PER_SEC;
	GlueManager::GlueManager() {
		time_start_consenus_ = 0;
		ledgerclose_check_timer_ = 0;
		check_interval_ = 2 * utils::MICRO_UNITS_PER_SEC;
		start_consensus_timer_ = 0;
		process_uptime_ = 0;
	}
	GlueManager::~GlueManager() {}

	bool GlueManager::Initialize() {

		tx_pool_ = std::make_shared<TransactionQueue>(Configure::Instance().ledger_configure_.queue_limit_,  Configure::Instance().ledger_configure_.queue_per_account_txs_limit_);
		process_uptime_ = time(NULL);
		consensus_ = ConsensusManager::Instance().GetConsensus();
		consensus_->SetNotify(this);

		//Start consensus
		start_consensus_timer_ = utils::Timer::Instance().AddTimer(3 * utils::MICRO_UNITS_PER_SEC, 0, [this](int64_t data) {
			StartConsensus("");
		});

		protocol::LedgerHeader lcl = LedgerManager::Instance().GetLastClosedLedger();
		if (lcl.version() < General::LEDGER_VERSION) {
			ledger_upgrade_.ConfNewVersion(General::LEDGER_VERSION);
		}

		//init hardfork points
		const utils::StringList &conf_hardfork_points = Configure::Instance().ledger_configure_.hardfork_points_;
		for (utils::StringList::const_iterator iter = conf_hardfork_points.begin();
			iter != conf_hardfork_points.end();
			iter++) {
			hardfork_points_.insert(utils::String::HexStringToBin(*iter));
		}

		StatusModule::RegisterModule(this);
		TimerNotify::RegisterModule(this);
		StartLedgerCloseTimer();
		return true;
	}

	void GlueManager::StartLedgerCloseTimer() {
		//Kill the ledger check timer
		utils::Timer::Instance().DelTimer(ledgerclose_check_timer_);
		ledgerclose_check_timer_ = utils::Timer::Instance().AddTimer(MAX_LEDGER_TIMESPAN_SECONDS + 10 * utils::MICRO_UNITS_PER_SEC, 0,
			[this](int64_t data) {
			LOG_INFO("Block closed timeout, triggering consensus view change.");
			consensus_->OnTxTimeout();
		});
	}

	bool GlueManager::Exit() {
		return true;
	}

	bool GlueManager::StartConsensus(const std::string &last_consavlue) {

		time_start_consenus_ = utils::Timestamp::HighResolution();
		if (!consensus_->IsLeader()) {
			LOG_INFO("The current node is not a leader node and does not do any processing.");
			return true;
		} 

		LOG_INFO("The current node is the leader node and starting consensus processing.");

		protocol::LedgerHeader lcl = LedgerManager::Instance().GetLastClosedLedger();
		protocol::TransactionEnvSet txset_raw = tx_pool_->TopTransaction(Configure::Instance().ledger_configure_.max_trans_per_ledger_);

		int64_t next_close_time = utils::Timestamp::Now().timestamp();
		if (next_close_time < lcl.close_time() + Configure::Instance().ledger_configure_.close_interval_) {
			next_close_time = lcl.close_time() + Configure::Instance().ledger_configure_.close_interval_;
		}

		if (!last_consavlue.empty()) {
			LOG_INFO("The last PREPARED message value is not empty. Value digest(%s)", 
				utils::String::BinToHexString(HashWrapper::Crypto(last_consavlue)).c_str());

			if (CheckValue(last_consavlue) == Consensus::CHECK_VALUE_VALID) {
				protocol::ConsensusValue propose_value;
				propose_value.ParseFromString(last_consavlue);

				LOG_INFO("Take the last consensus value as the proposal. The number of transactions in consensus value is %d, and the last closed ledger's hash is %s.", propose_value.txset().txs_size(),
					utils::String::Bin4ToHexString(lcl.hash()).c_str());

				return consensus_->Request(propose_value.SerializeAsString());
			}
		}

		// Get previous block proof
		std::string proof;
		Storage::Instance().account_db()->Get(General::LAST_PROOF, proof);

		protocol::ConsensusValue propose_value;
		do {
			*propose_value.mutable_txset() = txset_raw;
			propose_value.set_close_time(next_close_time);
			propose_value.set_ledger_seq(lcl.seq() + 1);
			propose_value.set_previous_ledger_hash(lcl.hash());
			propose_value.set_previous_proof(proof);

			//Check whether we need to upgrade the ledger.
			protocol::ValidatorSet validator_set;
			size_t quorum_size = 0;
			consensus_->GetValidation(validator_set, quorum_size);
			protocol::LedgerUpgrade up;
			if (ledger_upgrade_.GetValid(validator_set, quorum_size + 1, up)) {
				LOG_INFO("Get the upgrade information of the validation node(%s) successfully.", Proto2Json(up).toFastString().c_str());

				if (lcl.version() < up.new_ledger_version() && up.new_ledger_version() <= General::LEDGER_VERSION) {
					LOG_ERROR("Invalid upgrade information.");
					*propose_value.mutable_ledger_upgrade() = up;
				}
			}

			ProposeTxsResult propose_result;
			LedgerManager::Instance().context_manager_.SyncPreProcess(propose_value, true, propose_result);
			if (propose_result.block_timeout_) {
				int64_t total_tx_count = 0;
				if (propose_result.tx_execute_count_ == 0){
					total_tx_count = txset_raw.txs_size();
				}
				else{
					total_tx_count = MIN(propose_result.tx_execute_count_, txset_raw.txs_size());
				}

				//Remove the timeout tx, and reduct to 1/2
				protocol::TransactionEnvSet tmp_raw;
				int64_t next_tx_size = int64_t(total_tx_count * 0.7 + 0.5);
				LOG_ERROR("Block pre-execution timeout, the number of transactions in consensus value is: (" FMT_I64 "), and block number is: (" FMT_I64 "), executed tx count: (" FMT_I64 "), next tx count: (" FMT_I64 ")",
					txset_raw.txs_size(), propose_value.ledger_seq(), propose_result.tx_execute_count_, next_tx_size);
				for (int32_t i = 0; i < next_tx_size; i++) {
					*tmp_raw.add_txs() = txset_raw.txs(i);
				}

				txset_raw = tmp_raw;

				continue;
			}

			//Need drop some tx
			if (propose_result.need_dropped_tx_.size()) {
				protocol::TransactionEnvSet *txs = propose_value.mutable_txset();
				txs->clear_txs();

				protocol::TransactionEnvSet tmp_raw;
				for (int32_t i = 0; i < txset_raw.txs_size(); i++) {
					if (propose_result.need_dropped_tx_.find(i) != propose_result.need_dropped_tx_.end()) {
						//Remove from the cache
						*tmp_raw.add_txs() = txset_raw.txs(i);
					} else{
						*txs->add_txs() = txset_raw.txs(i);
					}
				}
				tx_pool_->RemoveTxs(tmp_raw);
			} 

			if (propose_result.cons_validation_.error_tx_ids_size() > 0 ||
				propose_result.cons_validation_.expire_tx_ids_size() > 0) {
				*propose_value.mutable_validation() = propose_result.cons_validation_;
			}

			LOG_INFO("Transaction pre-execution results: Number of transactions with excessive resource consumption is %d, \
				and the number of transactions that were pre-executed incorrectly is %d.) ",
				propose_result.cons_validation_.expire_tx_ids_size(), propose_result.cons_validation_.error_tx_ids_size());

			break;
		} while (true);

		LOG_INFO("The number of transactions in the proposal is %d, and the last ledger's hash is %s.", propose_value.txset().txs_size(),
			utils::String::Bin4ToHexString(lcl.hash()).c_str());
		consensus_->Request(propose_value.SerializeAsString());
		return true;
	}

	bool GlueManager::OnTransaction(TransactionFrm::pointer tx, Result &err) {
		std::string hash_value = tx->GetContentHash();
		std::string address = tx->GetSourceAddress();

		do {
			if (tx_pool_->IsExist(tx->GetContentHash())){
				//Break when a transaction is replayed;
				err.set_code(protocol::ERRCODE_ALREADY_EXIST);
				err.set_desc(utils::String::Format("Received duplicate transaction message. The transaction's source address is %s, and hash is %s", address.c_str(), utils::String::Bin4ToHexString(hash_value).c_str()));
				LOG_TRACE("Received duplicate transation message. The transaction's source address is %s, and hash is %s.", address.c_str(), utils::String::Bin4ToHexString(hash_value).c_str());
				break;
			}

			//Validate a transaction upon received.
			int64_t nonce = 0;
			if (!tx->CheckValid(/*high_sequence*/ -1, true, nonce)) {
				err = tx->GetResult();
				Json::Value js;
				js["action"] = "apply";
				js["error_code"] = err.code();
				js["desc"] = err.desc();
				LOG_ERROR("Transaction verification failed. The transaction's source address: %s, nonce: (" FMT_I64 "), hash: %s, return value: %s.",
					address.c_str(), tx->GetNonce(), utils::String::Bin4ToHexString(hash_value).c_str(), js.toFastString().c_str());
				break;
			}

			if (!tx_pool_->Import(tx, nonce, err)) {
				LOG_ERROR("Failed to insert transaction into transaction queue. The transaction's source address: %s, hash: %s.",
					address.c_str(), utils::String::Bin4ToHexString(hash_value).c_str());
			}

		} while (false);


		return err.code() == protocol::ERRCODE_SUCCESS;
	}

	bool GlueManager::OnConsensus(const ConsensusMsg &msg) {
		return consensus_->OnRecv(msg);
	}

	void GlueManager::OnTimer(int64_t current_time) {
		//Check the timeout transaction

		std::vector<TransactionFrm::pointer> timeout_txs;
		tx_pool_->CheckTimeoutAndDel(current_time, timeout_txs);

		if (timeout_txs.size() > 0 ){
			NotifyErrTx(timeout_txs);
		} 

		ledger_upgrade_.OnTimer(current_time);
	}


	void GlueManager::NotifyErrTx(std::vector<TransactionFrm::pointer> &txs) {
		for (std::vector<TransactionFrm::pointer>::iterator iter = txs.begin();
			iter != txs.end();
			iter++) {

//			TransactionFrm::pointer tx = *iter;
//			WebSocketServer::Instance().BroadcastChainTxMsg(tx->GetContentHash(), tx->GetSourceAddress(), 
//				tx->GetResult(), tx->GetResult().code() == protocol::ERRCODE_SUCCESS ? protocol::ChainTxStatus_TxStatus_COMPLETE : protocol::ChainTxStatus_TxStatus_FAILURE);
		}
	}

	void GlueManager::UpdateValidators(const protocol::ValidatorSet &validators, const std::string &proof) {
		consensus_->UpdateValidators(validators, proof);
	}

	void GlueManager::LedgerHasUpgrade() {
		ledger_upgrade_.LedgerHasUpgrade();
	}

	void GlueManager::OnRecvLedgerUpMsg(const protocol::LedgerUpgradeNotify &msg) {
		ledger_upgrade_.Recv(msg);
	}

	protocol::Signature GlueManager::SignConsensusData(const std::string &data) {
		return consensus_->SignData(data);
	}

	std::string GlueManager::OnValueCommited(int64_t request_seq, const std::string &value, const std::string &proof, bool calculate_total) {
		//Write to db
		int64_t time_start = utils::Timestamp::HighResolution();
		
		protocol::ConsensusValue req;
		req.ParseFromString(value);
		//Call consensus
		LedgerManager::Instance().OnConsent(req, proof);

		int64_t time_use = utils::Timestamp::HighResolution() - time_start;

		//Delete the cache 
		tx_pool_->RemoveTxs(req.txset(),true);

		//Start calculating the time to start the next block.
		int64_t next_interval = GetIntervalTime(req.txset().txs_size() == 0);
		int64_t next_timestamp = next_interval + req.close_time();
		int64_t seq = req.ledger_seq();

		Global::Instance().GetIoService().post([next_timestamp, time_use, seq, this]() {
			int64_t waiting_time = next_timestamp - utils::Timestamp::Now().timestamp();
			if (waiting_time <= 0)  waiting_time = 1;

			if (consensus_->IsLeader()) {
				start_consensus_timer_ = utils::Timer::Instance().AddTimer(waiting_time, 0, [this](int64_t data) {
					StartConsensus("");
				});

				LOG_INFO("Ledger(" FMT_I64 ") closed successfully, time used (" FMT_I64 ")ms, next consensus in(" FMT_I64 ")ms",
					seq, (int64_t)(time_use / utils::MILLI_UNITS_PER_SEC), (int64_t)(waiting_time / utils::MILLI_UNITS_PER_SEC));
			}
			else {
				LOG_INFO("Ledger(" FMT_I64 ") closed successfully, time used (" FMT_I64 ")ms, next consensus checked in(" FMT_I64 ")ms",
					seq, (int64_t)(time_use / utils::MILLI_UNITS_PER_SEC), (int64_t)(waiting_time / utils::MILLI_UNITS_PER_SEC));
			}

		});

		StartLedgerCloseTimer();
		protocol::LedgerHeader lcl1 = LedgerManager::Instance().GetLastClosedLedger();
		return lcl1.hash();
	}

	void GlueManager::OnViewChanged(const std::string &last_consvalue) {
		LOG_INFO("On view changed.");
		StartConsensus(last_consvalue);
		StartLedgerCloseTimer();
	}

	bool GlueManager::CheckValueAndProof(const std::string &consensus_value, const std::string &proof) {
		protocol::ConsensusValue proto_value;
		if (!proto_value.ParseFromString(consensus_value)) {
			LOG_ERROR("Failed to parse consensus value.");
			return false;
		}

		protocol::ValidatorSet set;
		if (!LedgerManager::Instance().GetValidators(proto_value.ledger_seq() - 1, set)) {
			LOG_ERROR("Failed to get validator of ledger(" FMT_I64 ")",
				proto_value.ledger_seq() - 1);
			return false;
		}
		
		//If a hardfork point is found on a node, we ignore the proof of the block before the fork point.
		std::string consensus_value_hash = HashWrapper::Crypto(consensus_value);
		std::set<std::string>::const_iterator iter = hardfork_points_.find(consensus_value_hash);
		return CheckValueHelper(proto_value, -1) == Consensus::CHECK_VALUE_VALID &&   //-1 not check time
			(consensus_->CheckProof(set, HashWrapper::Crypto(consensus_value), proof)
			|| iter != hardfork_points_.end());
	}

	int32_t GlueManager::CheckValue(const std::string &value) {
		protocol::ConsensusValue consensus_value;
		if (!consensus_value.ParseFromString(value)) {
			LOG_ERROR("Failed to parse consensus value.");
			return Consensus::CHECK_VALUE_MAYVALID;
		}

		int32_t check_helper_ret = CheckValueHelper(consensus_value, utils::Timestamp::Now().timestamp());
		if (check_helper_ret > 0) {
			return check_helper_ret;
		}

		ProposeTxsResult ignor_cons_validation;
		if (!LedgerManager::Instance().context_manager_.SyncPreProcess(consensus_value,
			false,
			ignor_cons_validation)) {
			LOG_ERROR("Failed to preprocess the consensus value.");
			return Consensus::CHECK_VALUE_MAYVALID;
		}

		return Consensus::CHECK_VALUE_VALID;
	}

	int32_t GlueManager::CheckValueHelper(const protocol::ConsensusValue &consensus_value, int64_t now) {
		if (consensus_value.ByteSize() >= General::TXSET_LIMIT_SIZE + (int32_t)(2 * utils::BYTES_PER_MEGA)) {
			LOG_ERROR("The byte size(%d) of consensus value exceed the upper limit(%d).",
				consensus_value.ByteSize(),
				General::TXSET_LIMIT_SIZE + 2 * utils::BYTES_PER_MEGA);
			return Consensus::CHECK_VALUE_MAYVALID;
		}

		protocol::LedgerHeader lcl = LedgerManager::Instance().GetLastClosedLedger();

		//Check the previous ledger sequence.
		if (consensus_value.ledger_seq() != lcl.seq() + 1) {
			LOG_ERROR("Previous ledger's sequence(" FMT_I64 ") + 1 is not equal to ledger sequence( " FMT_I64 ") in consensus message.",
				lcl.seq(),
				consensus_value.ledger_seq());
			return Consensus::CHECK_VALUE_MAYVALID;
		}

		//Check the previous hash.
		if (consensus_value.previous_ledger_hash() != lcl.hash()) {
			LOG_ERROR("Previous ledger(" FMT_I64 ") hash(%s) in current node is not equal to the previous ledger hash(%s) in consensus value.",
				lcl.seq(),
				utils::String::Bin4ToHexString(lcl.hash()).c_str(),
				utils::String::Bin4ToHexString(consensus_value.previous_ledger_hash()).c_str());
			return Consensus::CHECK_VALUE_MAYVALID;
		}

		//not closed, can tolerate 1 second 
		if (now != -1 && 
			!(
			now > (consensus_value.close_time() - utils::MICRO_UNITS_PER_SEC)
			&& 
			consensus_value.close_time() >= lcl.close_time() + Configure::Instance().ledger_configure_.close_interval_
			)
			) {
			LOG_WARN("Current time(" FMT_I64 ") > Close time(" FMT_I64 ") > (last ledger closed time(" FMT_I64 ") + interval(" FMT_I64")) is not tenable. Consensus network time error!", 
				now, consensus_value.close_time() ,
				lcl.close_time(), Configure::Instance().ledger_configure_.close_interval_ );
			return Consensus::CHECK_VALUE_MAYVALID;
		}

		if (consensus_value.has_ledger_upgrade()) {
			const protocol::LedgerUpgrade &upgrade = consensus_value.ledger_upgrade();
			if (upgrade.new_ledger_version() != 0) {
				if (lcl.version() >= upgrade.new_ledger_version()) {
					LOG_ERROR("New ledger's version(" FMT_I64 ") is less than or equal to last closed ledger's version(" FMT_I64 ")",
						upgrade.new_ledger_version(), lcl.version());
					return Consensus::CHECK_VALUE_MAYVALID;
				}

				if (upgrade.new_ledger_version() > General::LEDGER_VERSION) {
					LOG_ERROR("New ledger's version (" FMT_I64 ") is larger than program's version(%u).",
						upgrade.new_ledger_version(),
						General::LEDGER_VERSION);
					return Consensus::CHECK_VALUE_MAYVALID;
				}
			}

			//The 'upgrade' field should not exist in normal blocks.
			bool new_validator_exist = !upgrade.new_validator().empty();
			std::string consensus_value_hash = HashWrapper::Crypto(consensus_value.SerializeAsString());
			if (hardfork_points_.end() == hardfork_points_.find(consensus_value_hash) && new_validator_exist) {
				return Consensus::CHECK_VALUE_MAYVALID;
			} 
		}

		//Check the second block
		if (lcl.seq() == 1 && consensus_value.previous_proof() != "") {
			LOG_ERROR("The second block's previous consensus proof must be empty.");
			return Consensus::CHECK_VALUE_MAYVALID;
		}

		//Check this proof 
		if (lcl.seq() > 1) {
			//Get the validator set for the pre pre ledger.
			protocol::ValidatorSet set;
			if (!LedgerManager::Instance().GetValidators(consensus_value.ledger_seq() - 2, set)) {
				LOG_ERROR("Failed to get validator of ledger (" FMT_I64 ").",
					consensus_value.ledger_seq() - 2);
				return Consensus::CHECK_VALUE_MAYVALID;
			}

			//Check if the consensus value hash is forked
			std::set<std::string>::const_iterator iter = hardfork_points_.find(lcl.consensus_value_hash());
			if (iter == hardfork_points_.end() && !consensus_->CheckProof(set, lcl.consensus_value_hash(), consensus_value.previous_proof())) {
					LOG_ERROR("Failed to check the value because the proof is not valid.");
					return Consensus::CHECK_VALUE_MAYVALID;
			}
		}

		return Consensus::CHECK_VALUE_VALID;
	}

	void GlueManager::SendConsensusMessage(const std::string &message) {
		Global::Instance().GetIoService().post([this, message] (){
			PeerManager::Instance().Broadcast(protocol::OVERLAY_MSGTYPE_PBFT, message);

			protocol::PbftEnv env;
			env.ParseFromString(message);
			ConsensusMsg msg(env);
			LOG_INFO("Received consensus message from self. Node address(%s), sequence(" FMT_I64 "), pbft type(%s)",
				msg.GetNodeAddress(), msg.GetSeq(),PbftDesc::GetMessageTypeDesc(msg.GetPbft().pbft().type()));
			consensus_->OnRecv(msg);
		});
	}

	std::string GlueManager::FetchNullMsg() {
		return "null";
	}

	void GlueManager::GetModuleStatus(Json::Value &data) {
		data["name"] = "glue_manager";
		data["transaction_size"] = (Json::UInt64)tx_pool_->Size();

		Json::Value &system_json = data["system"];
		utils::Timestamp time_stamp(utils::GetStartupTime() * utils::MICRO_UNITS_PER_SEC);
		system_json["uptime"] = time_stamp.ToFormatString(false);
		utils::Timestamp process_time_stamp(process_uptime_ * utils::MICRO_UNITS_PER_SEC);
		system_json["process_uptime"] = process_time_stamp.ToFormatString(false);
		system_json["current_time"] = utils::Timestamp::Now().ToFormatString(false);
		 
		ledger_upgrade_.GetModuleStatus(data["ledger_upgrade"]);
	}

	int64_t GlueManager::GetIntervalTime(bool empty_block) {
		return Configure::Instance().ledger_configure_.close_interval_;
	}

	void GlueManager::OnResetCloseTimer() {
		StartLedgerCloseTimer();
	}

	std::string GlueManager::DescConsensusValue(const std::string &request) {
		protocol::ConsensusValue value;
		value.ParseFromString(request);
		return utils::String::Format("value hash(%s) | close time(" FMT_I64 ") | lcl hash(%s) | ledger seq(" FMT_I64 ") ", 
			utils::String::BinToHexString(HashWrapper::Crypto(request)).c_str(),
			value.close_time(),
			utils::String::Bin4ToHexString(value.previous_ledger_hash()).c_str(),
			value.ledger_seq());
	}

	time_t GlueManager::GetProcessUptime() {
		return process_uptime_;
	}

	size_t GlueManager::GetTransactionCacheSize() {
		return tx_pool_->Size();
	}

	void GlueManager::QueryTransactionCache(const uint32_t& num, std::vector<TransactionFrm::pointer>& txs){
		tx_pool_->Query(num,txs);
	}

	bool GlueManager::QueryTransactionCache(const std::string& hash, TransactionFrm::pointer& tx){
		return tx_pool_->Query(hash, tx);
	}

}
