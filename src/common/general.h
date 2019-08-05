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

#ifndef GENERAL_H_
#define GENERAL_H_

#include <asio.hpp>
#include <utils/headers.h>
#include <json/value.h>
#include <utils/sm3.h>
#include "data_secret_key.h"

namespace bumo {
	class General {
	public:
		const static uint32_t OVERLAY_VERSION;
		const static uint32_t OVERLAY_MIN_VERSION;
		const static uint32_t LEDGER_VERSION_HISTORY_1000;
		const static uint32_t LEDGER_VERSION_HISTORY_1001;
		const static uint32_t LEDGER_VERSION_HISTORY_1002;
		const static uint32_t LEDGER_VERSION;
		const static uint32_t LEDGER_MIN_VERSION;
		const static uint32_t MONITOR_VERSION;
		const static char *BUMO_VERSION;

		const static int CONSENSUS_PORT = 16001;
		const static int WEBSERVER_PORT = 16002;

		const static int METADATA_KEY_MAXSIZE = 1024;
		const static int METADATA_MAX_VALUE_SIZE = 256 * utils::BYTES_PER_KILO;
		const static int ASSET_CODE_MAX_SIZE = 64;
		const static int EXPRCONDITION_MAXSIZE = 256;

		//Contract tx->A(1), A->B(2), B->C(3), C->D(4), D can do anything but call contract.
		// The max RECURSIVE DEPTH is 4, the 4th can't call contract.
		const static int CONTRACT_MAX_RECURSIVE_DEPTH = 4;

		//At most 512 transactions can be created when a contract is executed.
		const static int CONTRACT_TRANSACTION_LIMIT = 512;

		const static int CONTRACT_CODE_LIMIT = 256 * utils::BYTES_PER_KILO;
		const static int CONTRACT_STEP_LIMIT = 10 * utils::BYTES_PER_KILO;
		const static int CONTRACT_MEMORY_LIMIT = 30 * utils::BYTES_PER_MEGA; //Limit memory to 30M
		const static int CONTRACT_STACK_LIMIT = 256 * utils::BYTES_PER_KILO;

		const static int TX_EXECUTE_TIME_OUT = utils::MICRO_UNITS_PER_SEC;
		const static int BLOCK_EXECUTE_TIME_OUT = 5 * utils::MICRO_UNITS_PER_SEC;

		const static int LAST_TX_HASHS_LIMIT = 100;

		const static size_t BU_DECIMALS = 8;  // 10^8

		const static char *DEFAULT_KEYVALUE_DB_PATH;
		const static char *DEFAULT_ACCOUNT_DB_PATH;

		const static char *DEFAULT_LEDGER_DB_PATH;
		const static char *DEFAULT_RATIONAL_DB_PATH;

		const static char *CONFIG_FILE;
		const static char *MONITOR_CONFIG_FILE;
		const static char *CA_CONFIG_FILE;
		const static char *LOGGER_FILE;

		const static char* STATISTICS;
		const static char *CONSENSUS_PREFIX;

		const static char *LEDGER_PREFIX;
		const static char *TRANSACTION_PREFIX;
		const static char *LEDGER_TRANSACTION_PREFIX;
		const static char *CONSENSUS_VALUE_PREFIX;
		const static char *PEERS_TABLE;
		const static char *LAST_TX_HASHS;
		const static char *LAST_PROOF;

		const static int ACCOUNT_LENGTH_MAX = 40;

		const static char *KEY_LEDGER_SEQ;
		const static char *KEY_GENE_ACCOUNT;
		const static char *VALIDATORS;

		const static char *ACCOUNT_PREFIX;
		const static char *ASSET_PREFIX;
		const static char *METADATA_PREFIX;

		const static char *CHECK_TIME_FUNCTION;

		const static char *CONTRACT_VALIDATOR_ADDRESS;
		const static char *CONTRACT_FEE_ADDRESS;
		const static char *RESERVED_CREATOR_ADDRESS;
		const static char *CONTRACT_DPOS_ADDRESS;
		const static char *RESERVED_ADDRESS4;
		const static char *RESERVED_ADDRESS5;
		const static char *RESERVED_ADDRESS6;
		const static char *RESERVED_ADDRESS7;
		const static char *RESERVED_ADDRESS8;

		const static int32_t TRANSACTION_LIMIT_SIZE;
		const static int32_t TXSET_LIMIT_SIZE;

		const static int TRANSACTION_LOG_TOPIC_MAXSIZE = 128;
		const static int TRANSACTION_LOG_DATA_MAXSIZE = 1024;

		const static int PEER_DB_COUNT = 1000;

		const static int64_t REWARD_PERIOD = (5 * 365 * 24 * 60 * 60) / 10;
		const static int64_t REWARD_INIT_VALUE = 8 * 100000000;

		typedef enum WARNINGCODE_ {
			WARNING,
			NOWARNING
		} WARNINGCODE;

		volatile static long tx_new_count;
		volatile static long tx_delete_count;
		volatile static long txset_new_count;
		volatile static long txset_delete_count;
		volatile static long peermsg_new_count;
		volatile static long peermsg_delete_count;
		volatile static long account_new_count;
		volatile static long account_delete_count;
		volatile static long trans_low_new_count;
		volatile static long trans_low_delete_count;

	public:
		static void SetSelfChainId(int64_t chain_id) { chain_id_ = chain_id; }
		static int64_t GetSelfChainId() { return chain_id_; }

	private:
		static int64_t chain_id_;
	};

	class Result {
		int32_t code_;
		std::string desc_;

		//if has value, must a json, OR nullValue
		//if has value, like, code_ == 0, {"type": bool/string, "value":true/"hello the world"}
		Json::Value contract_result_; 
	public:
		Result();
		Result(const Result &result);
		~Result();

		int32_t code() const;
		std::string desc() const;
		const Json::Value &contract_result() const;

		void set_code(int32_t code);
		void set_desc(const std::string desc);
		void set_contract_result(const Json::Value &contract_result);

		bool operator=(const Result &result);
	};

	class TimerNotify {
	protected:
		int64_t last_check_time_;
		int64_t last_slow_check_time_;

		int64_t check_interval_;

		int64_t last_execute_complete_time_;
		int64_t last_slow_execute_complete_time_;
		std::string timer_name_;
	public:
		static std::list<TimerNotify *> notifys_;
		static bool RegisterModule(TimerNotify *module) { notifys_.push_back(module); return true; };

		TimerNotify() :last_check_time_(0), 
			last_slow_check_time_(0), 
			check_interval_(0),
			last_execute_complete_time_(0),
			last_slow_execute_complete_time_(0) {};
		~TimerNotify() {};

		void TimerWrapper(int64_t current_time) {
			last_execute_complete_time_ = 0; //clear first
			if (current_time > last_check_time_ + check_interval_) {
				last_check_time_ = current_time;
				OnTimer(current_time);
				last_execute_complete_time_ = utils::Timestamp::HighResolution();
			}
		};

		void SlowTimerWrapper(int64_t current_time) {
			last_slow_execute_complete_time_ = 0;//clear first
			if (current_time > last_slow_check_time_ + check_interval_) {
				last_slow_check_time_ = current_time;
				OnSlowTimer(current_time);
				last_slow_execute_complete_time_ = utils::Timestamp::HighResolution();
			}
		};

		bool IsSlowExpire(int64_t time_out) {
			return last_slow_execute_complete_time_ - last_slow_check_time_ > time_out;
		}

		bool IsExpire(int64_t time_out) {
			return last_execute_complete_time_ - last_check_time_ > time_out;
		}

		int64_t GetSlowLastExecuteTime() {
			return last_slow_execute_complete_time_ - last_slow_check_time_;
		}

		int64_t GetLastExecuteTime() {
			return last_execute_complete_time_ - last_check_time_;
		}

		std::string GetTimerName() {
			return timer_name_;
		}

		virtual void OnTimer(int64_t current_time) = 0;
		virtual void OnSlowTimer(int64_t current_time) = 0;
	};

	class StatusModule {
	public:
		static std::list<StatusModule *> modules_;
		static Json::Value *modules_status_;
		static utils::ReadWriteLock status_lock_;
		static bool RegisterModule(StatusModule *module) { modules_.push_back(module); return true; };
		static void GetModulesStatus(Json::Value &nData);

		StatusModule() {};
		~StatusModule() {};

		virtual void GetModuleStatus(Json::Value &nData) = 0;
	};

	class SlowTimer : public utils::Singleton<bumo::SlowTimer>, public utils::Runnable {
	public:
		SlowTimer();
		~SlowTimer();

		bool Initialize(size_t thread_count);
		bool Exit();

		asio::io_service io_service_;
		//utils::Thread *thread_ptr_;
		std::vector<utils::Thread *> thread_ptrs_;
		virtual void Run(utils::Thread *thread) override;
		void Stop();
	};

	class Global : public utils::Singleton<bumo::Global>, public TimerNotify {
		asio::io_service io_service_;
		asio::io_service::work work_;
		int64_t main_thread_id_;
	public:
		Global();
		~Global();
		bool Initialize();
		bool Exit();
		virtual void OnTimer(int64_t current_time) override;
		virtual void OnSlowTimer(int64_t current_time) override {};
		asio::io_service &GetIoService();
		int64_t GetMainThreadId();
	};

#define  ASSERT_MAIN_THREAD assert(utils::Thread::current_thread_id() == Global::Instance().GetMainThreadId());

	class HashWrapper : public utils::NonCopyable {
		int32_t type_;// 0 : protocol::LedgerUpgrade::SHA256, 1: protocol::LedgerUpgrade::SM3
		utils::Hash *hash_;
	public:
		enum HashType {
			HASH_TYPE_SHA256 = 0,
			HASH_TYPE_SM3 = 1,
			HASH_TYPE_MAX = 2
		};

		HashWrapper();
		HashWrapper(int32_t type);
		~HashWrapper();

		void Update(const std::string &input);
		void Update(const void *buffer, size_t len);
		std::string Final();

		static void SetLedgerHashType(int32_t type);
		static int32_t GetLedgerHashType();

		static std::string Crypto(const std::string &input);
		static void Crypto(unsigned char* str, int len, unsigned char *buf);
		static void Crypto(const std::string &input, std::string &str);
	};

	std::string GetDataSecuretKey();
	std::string ComposePrefix(const std::string &prefix, const std::string &value);
	std::string ComposePrefix(const std::string &prefix, int64_t value);
	int64_t GetBlockReward(const int64_t cur_block_height);

#define CHECK_VERSION_GT_1000 (LedgerManager::Instance().GetLastClosedLedger().version() > General::LEDGER_VERSION_HISTORY_1000)
#define CHECK_VERSION_GT_1001 (LedgerManager::Instance().GetLastClosedLedger().version() > General::LEDGER_VERSION_HISTORY_1001)
#define CHECK_VERSION_GT_1002 (LedgerManager::Instance().GetLastClosedLedger().version() > General::LEDGER_VERSION_HISTORY_1002)
#define GET_CONTRACT_VALIDATOR_ADDRESS (CHECK_VERSION_GT_1002 ? General::CONTRACT_DPOS_ADDRESS : General::CONTRACT_VALIDATOR_ADDRESS)
#define GET_CONTRACT_FEE_ADDRESS (CHECK_VERSION_GT_1002 ? General::CONTRACT_DPOS_ADDRESS : General::CONTRACT_FEE_ADDRESS)
}

#endif
