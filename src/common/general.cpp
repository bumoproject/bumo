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

#include <utils/logger.h>
#include <utils/sm3.h>
#include "general.h"
#include "utils/strings.h"
#include "proto/cpp/common.pb.h"

namespace bumo {
	const uint32_t General::OVERLAY_VERSION = 1000;
	const uint32_t General::OVERLAY_MIN_VERSION = 1000;
	/*
		Based on ledger 1000, the following changes have been modified.
		1.Create a common or contract account without signers.
		2.Add the set privilege option.
		3.Create a contract account without destination address, and it will be created automatically.
	*/
	const uint32_t General::LEDGER_VERSION_HISTORY_1000 = 1000;
	const uint32_t General::LEDGER_VERSION_HISTORY_1001 = 1001;
	const uint32_t General::LEDGER_VERSION_HISTORY_1002 = 1002;
	const uint32_t General::LEDGER_VERSION = 1003;
	const uint32_t General::LEDGER_MIN_VERSION = 1000;
	const uint32_t General::MONITOR_VERSION = 1000;
	const char *General::BUMO_VERSION = "1.3.2";

#ifdef WIN32
	const char *General::DEFAULT_KEYVALUE_DB_PATH = "data/keyvalue.db";
	const char *General::DEFAULT_LEDGER_DB_PATH = "data/ledger.db";
	const char *General::DEFAULT_ACCOUNT_DB_PATH = "data/account.db";
	
	const char *General::CONFIG_FILE = "config/bumo.json";
	const char *General::MONITOR_CONFIG_FILE = "config/monitor.json";
	const char *General::LOGGER_FILE = "log/bumo.log";

#else
	const char *General::DEFAULT_KEYVALUE_DB_PATH = "data/keyvalue.db";
	const char *General::DEFAULT_LEDGER_DB_PATH = "data/ledger.db";
	const char *General::DEFAULT_ACCOUNT_DB_PATH = "data/account.db";

	const char *General::CONFIG_FILE = "config/bumo.json";
	const char *General::LOGGER_FILE = "log/bumo.log";
#endif

	volatile long General::tx_new_count = 0;
	volatile long General::tx_delete_count = 0;
	volatile long General::txset_new_count = 0;
	volatile long General::txset_delete_count = 0;
	volatile long General::peermsg_new_count = 0;
	volatile long General::peermsg_delete_count = 0;
	volatile long General::account_new_count = 0;
	volatile long General::account_delete_count = 0;
	volatile long General::trans_low_new_count = 0;
	volatile long General::trans_low_delete_count = 0;

	const char *General::STATISTICS = "statistics";
	const char *General::KEY_LEDGER_SEQ = "max_seq";
	const char *General::KEY_GENE_ACCOUNT = "genesis_account";
	const char *General::VALIDATORS = "validators";
	const char *General::PEERS_TABLE = "peers_table";
	const char *General::LAST_TX_HASHS = "last_tx_hashs";
	const char *General::LAST_PROOF = "last_proof";

	const char *General::CONSENSUS_PREFIX = "consensus";

	const char *General::LEDGER_PREFIX = "ldg";
	const char *General::TRANSACTION_PREFIX = "tx";
	const char *General::LEDGER_TRANSACTION_PREFIX = "lgtx";
	const char *General::CONSENSUS_VALUE_PREFIX = "cosv";

	const char *General::ACCOUNT_PREFIX = "acc";
	const char *General::ASSET_PREFIX = "ast";
	const char *General::METADATA_PREFIX = "meta";

	const char *General::CHECK_TIME_FUNCTION = "internal_check_time";

	//bumo::PublicKey pub_key;
	//pub_key.Init("1");
	//std::string account1 = pub_key.GetEncAddress(); "buQtxgoaDrVJGtoPT66YnA2S84yE8FbBqQDJ"
	//pub_key.Init("2");
	//std::string account2 = pub_key.GetEncAddress();"buQiQgRerQM1fUM3GkqUftpNxGzNg2AdJBpe"
	//pub_key.Init("3");
	//std::string account3 = pub_key.GetEncAddress();"buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss"
	//pub_key.Init("4");
	//std::string account4 = pub_key.GetEncAddress();"buQcHGxxpPDBcPKys8cD5NY1CAiyAfFBjgtg"
	//pub_key.Init("5");
	//std::string account5 = pub_key.GetEncAddress();"buQraU2Y5niuYyDdS7FNfJYRrGe8q21RmWc3"
	//pub_key.Init("6");
	//std::string account6 = pub_key.GetEncAddress();"buQXoma4baahLbAcnZdC7hFqCHoAxbnytYxd"
	//pub_key.Init("7");
	//std::string account7 = pub_key.GetEncAddress();"buQVpA6eKpuDcNfubXsgG9ihN32psE1jyygm"
	//pub_key.Init("8");
	//std::string account8 = pub_key.GetEncAddress();"buQa9pRC8B38zd8Z47q97D4X5WwqhMry3VEm"

	const char *General::CONTRACT_VALIDATOR_ADDRESS = "buQtxgoaDrVJGtoPT66YnA2S84yE8FbBqQDJ";
	const char *General::CONTRACT_FEE_ADDRESS = "buQiQgRerQM1fUM3GkqUftpNxGzNg2AdJBpe";
	const char *General::RESERVED_CREATOR_ADDRESS = "buQgCBN4MSZMWRfJbksh9PWpKqgY3tSEMM4d";
	const char *General::CONTRACT_DPOS_ADDRESS = "buQqzdS9YSnokDjvzg4YaNatcFQfkgXqk6ss";
	const char *General::RESERVED_ADDRESS4 = "buQcHGxxpPDBcPKys8cD5NY1CAiyAfFBjgtg";
	const char *General::RESERVED_ADDRESS5 = "buQraU2Y5niuYyDdS7FNfJYRrGe8q21RmWc3";
	const char *General::RESERVED_ADDRESS6 = "buQXoma4baahLbAcnZdC7hFqCHoAxbnytYxd";
	const char *General::RESERVED_ADDRESS7 = "buQVpA6eKpuDcNfubXsgG9ihN32psE1jyygm";
	const char *General::RESERVED_ADDRESS8 = "buQa9pRC8B38zd8Z47q97D4X5WwqhMry3VEm";


	const int32_t General::TRANSACTION_LIMIT_SIZE = utils::BYTES_PER_MEGA;
	const int32_t General::TXSET_LIMIT_SIZE = 16 * utils::BYTES_PER_MEGA;

	int64_t General::chain_id_ = 0;

	Result::Result(){
		code_ = protocol::ERRCODE_SUCCESS;
		contract_result_ = Json::Value(Json::nullValue);
	}

	Result::Result(const Result &result) {
		code_ = result.code_;
		desc_ = result.desc_;
		contract_result_ = result.contract_result_;
	}

	Result::~Result(){};

	int32_t Result::code() const{
		return code_;
	}

	std::string Result::desc() const{
		return desc_;
	}

	const Json::Value &Result::contract_result() const {
		return contract_result_;
	}

	void Result::set_code(int32_t code){
		code_ = code;
	}

	void Result::set_desc(const std::string desc){
		desc_ = desc;
	}

	void Result::set_contract_result(const Json::Value &contract_result) {
		contract_result_ = contract_result;
	}

	bool Result::operator=(const Result &result){
		code_ = result.code();
		desc_ = result.desc();
		contract_result_ = result.contract_result();
		return true;
	}

	std::list<StatusModule *> StatusModule::modules_;
	Json::Value *StatusModule::modules_status_ = NULL;
	utils::ReadWriteLock StatusModule::status_lock_;

	void StatusModule::GetModulesStatus(Json::Value &nData){
		for (auto &item : modules_) {
			Json::Value json_item = Json::Value(Json::objectValue);
			int64_t begin_time = utils::Timestamp::HighResolution();
			item->GetModuleStatus(json_item);
			json_item["time"] = utils::String::Format(FMT_I64 " ms", (utils::Timestamp::HighResolution() - begin_time) / utils::MICRO_UNITS_PER_MILLI);
			std::string key = json_item["name"].asString();
			json_item.removeMember("name");
			nData[key] = json_item;
		}
	}

	std::list<TimerNotify *> TimerNotify::notifys_;

	SlowTimer::SlowTimer(){
	}

	SlowTimer::~SlowTimer(){}

	bool SlowTimer::Initialize(size_t thread_count){
		for (size_t i = 0; i < thread_count; i++){
			utils::Thread *thread_p = new utils::Thread(this);
			if (!thread_p->Start(utils::String::Format("slowtimer-%d", i))){
				return false;
			}

			thread_ptrs_.push_back(thread_p);
		}

		return true;
	}

	bool SlowTimer::Exit(){
		LOG_INFO("SlowTimer stoping...");
		io_service_.stop();
		for (size_t i = 0; i < thread_ptrs_.size(); i++){
			utils::Thread *thread_p = thread_ptrs_[i];
			if (thread_p){
				thread_p->JoinWithStop();
				delete thread_p;
				thread_p = NULL;
			}
		}
		LOG_INFO("SlowTimer stop [OK]");
		return true;
	}

	void SlowTimer::Run(utils::Thread *thread){
		asio::io_service::work work(io_service_);
		while (!io_service_.stopped()){
			asio::error_code err;
			io_service_.poll(err);

			for (auto item : TimerNotify::notifys_){
				item->SlowTimerWrapper(utils::Timestamp::HighResolution());

				if (item->IsSlowExpire(5 * utils::MICRO_UNITS_PER_SEC)){
					LOG_WARN("The execution time(%s) (" FMT_I64 " us) is expired after 5s elapse", item->GetTimerName().c_str(), item->GetSlowLastExecuteTime());
				}
			}

			utils::Sleep(1);
		}
	}

	Global::Global() : work_(io_service_), main_thread_id_(0){
	}

	Global::~Global(){
	}

	bool Global::Initialize(){
		timer_name_ = "Global";
		main_thread_id_ = utils::Thread::current_thread_id();
		TimerNotify::RegisterModule(this);
		return true;
	}

	bool Global::Exit(){
		LOG_INFO("Global stoping...");
		LOG_INFO("Global stop [OK]");
		return true;
	}

	void Global::OnTimer(int64_t current_time){
		//clock_.crank(false);
		asio::error_code err;
		io_service_.poll(err);
	}

	asio::io_service &Global::GetIoService(){
		return io_service_;
	}

	int64_t Global::GetMainThreadId(){
		return main_thread_id_;
	}
	
	static int32_t ledger_type_ = HashWrapper::HASH_TYPE_SHA256;
	HashWrapper::HashWrapper(){
		type_ = ledger_type_;
		if (type_ == HASH_TYPE_SM3){
			hash_ = new utils::Sm3();
		}

		else{
			hash_ = new utils::Sha256();
		}
	}

	HashWrapper::HashWrapper(int32_t type){
		type_ = type;
		if (type_ == HASH_TYPE_SM3){
			hash_ = new utils::Sm3();
		}
		else{
			hash_ = new utils::Sha256();
		}
	}

	HashWrapper::~HashWrapper(){
		if (hash_){
			delete hash_;
		} 
	}

	void HashWrapper::Update(const std::string &input){
		hash_->Update(input);
	}

	void HashWrapper::Update(const void *buffer, size_t len){
		hash_->Update(buffer, len);
	}

	std::string HashWrapper::Final(){
		return hash_->Final();
	}

	void HashWrapper::SetLedgerHashType(int32_t type_){
		ledger_type_ = type_;
	}

	int32_t HashWrapper::GetLedgerHashType(){
		return ledger_type_;
	}

	std::string HashWrapper::Crypto(const std::string &input){
		if (ledger_type_ == HASH_TYPE_SM3){
			return utils::Sm3::Crypto(input);
		}
		else{
			return utils::Sha256::Crypto(input);
		}
	}

	void HashWrapper::Crypto(unsigned char* str, int len, unsigned char *buf){
		if (ledger_type_ == HASH_TYPE_SM3){
			utils::Sm3::Crypto(str, len, buf);
		}
		else{
			utils::Sha256::Crypto(str, len, buf);
		}
	}

	void HashWrapper::Crypto(const std::string &input, std::string &str){
		if (ledger_type_ == HASH_TYPE_SM3){
			utils::Sm3::Crypto(input, str);
		}
		else{
			utils::Sha256::Crypto(input, str);
		}
	}

	std::string ComposePrefix(const std::string &prefix, const std::string &value) {
		std::string result = prefix;
		result += "_";
		result += value;
		return result;
	}

	std::string ComposePrefix(const std::string &prefix, int64_t value) {
		std::string result = prefix;
		result += "_";
		result += utils::String::ToString(value);
		return result;
	}

	int64_t GetBlockReward(const int64_t cur_block_height) {
		int64_t period_index = cur_block_height / General::REWARD_PERIOD;

		//Decrease by 1/4 every period.
		int64_t result = General::REWARD_INIT_VALUE;
		for (int64_t i = 0; i < period_index; i++)
		{
			if (result <= 0)
			{
				return 0;
			}
			else if (result < 4)
			{
				result = result * 3 / 4;
			}
			else
			{
				result = result - (result >> 2);
				continue;
			}
		}

		return result;
	}
}
