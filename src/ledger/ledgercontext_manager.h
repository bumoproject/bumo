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

#ifndef LEDGER_CONTEXT_MANAGER_H_
#define LEDGER_CONTEXT_MANAGER_H_

#include <utils/headers.h>
#include <common/general.h>
#include <proto/cpp/chain.pb.h>
#include "ledger_frm.h"
#include <contract/contract_manager.h>

namespace bumo {

	class LedgerContextManager;
	class LedgerContext;
	typedef std::function< void(bool check_result)> PreProcessCallback;
	class LedgerContext : public utils::Thread {
		std::stack<int64_t> contract_ids_; //The contract_ids may be called by checking the thread or executing the thread, so contract_ids needs to be locked.
		//parameter
		int32_t type_; // -1 : normal, 0 : test v8 , 1: test evm ,2 test transaction
		ContractTestParameter parameter_; // when type_ >= 0

		std::string hash_;
		LedgerContextManager *lpmanager_;
		int64_t start_time_;

		LedgerFrm::APPLY_MODE apply_mode_;

		Json::Value logs_;
		Json::Value rets_;
	public:
		LedgerContext(
			LedgerContextManager *lpmanager,
			const std::string &chash, 
			const protocol::ConsensusValue &consvalue,
			bool propose);

		LedgerContext(
			const std::string &chash, 
			const protocol::ConsensusValue &consvalue);

		//for test
		LedgerContext(
			int32_t type,
			const ContractTestParameter &parameter);
		LedgerContext(
			int32_t type,
			const protocol::ConsensusValue &consensus_value,
			int64_t timeout);

		~LedgerContext();

		enum ACTION_TYPE{
			AT_NORMAL = -1,
			AT_TEST_V8,
			AT_TEST_EVM,
			AT_TEST_TRANSACTION
		};

		protocol::ConsensusValue consensus_value_;
		int64_t tx_timeout_;

		LedgerFrm::pointer closing_ledger_;
		std::vector<std::shared_ptr<TransactionFrm>> transaction_stack_;
		
		//result
		//bool exe_result_;
		int32_t timeout_tx_index_;
		//protocol::ConsensusValueValidation consvalue_validation_;
		ProposeTxsResult propose_result_;

		utils::Mutex lock_;

		virtual void Run();
		void Do();
		bool TestV8();
		bool TestTransaction();
		void Cancel();
		bool CheckExpire(int64_t total_timeout);
		
		void PushContractId(int64_t id);
		void PopContractId();
		int64_t GetTopContractId();

		void PushLog(const std::string &address, const utils::StringList &logs);
		void GetLogs(Json::Value &logs);

		void PushRet(const std::string &address, const Json::Value &ret);
		void GetRets(Json::Value &rets);
		
		std::string GetHash();
		int32_t GetTxTimeoutIndex();

		void PushLog();
		std::shared_ptr<TransactionFrm> GetBottomTx();
		std::shared_ptr<TransactionFrm> GetTopTx();
	};

	typedef std::multimap<std::string, LedgerContext *> LedgerContextMultiMap;
	typedef std::multimap<int64_t, LedgerContext *> LedgerContextTimeMultiMap;
	typedef std::map<std::string, LedgerContext *> LedgerContextMap;
	class LedgerContextManager :
		public bumo::TimerNotify {
		utils::Mutex ctxs_lock_;
		LedgerContextMultiMap running_ctxs_;
		LedgerContextMap completed_ctxs_;
		LedgerContextTimeMultiMap delete_ctxs_;
	public:
		LedgerContextManager();
		~LedgerContextManager();

		void Initialize();
		virtual void OnTimer(int64_t current_time);
		virtual void OnSlowTimer(int64_t current_time);
		void MoveRunningToComplete(LedgerContext *ledger_context);
		void MoveRunningToDelete(LedgerContext *ledger_context);
		void RemoveCompleted(int64_t ledger_seq);
		void GetModuleStatus(Json::Value &data);

		bool SyncTestProcess(LedgerContext::ACTION_TYPE type,
			TestParameter *parameter, 
			int64_t total_timeout, 
			Result &result, 
			Json::Value &logs,
			Json::Value &txs,
			Json::Value &rets,
			Json::Value &stat,
			int32_t signature_number = 0);

		//<0 : notfound 1: found and success 0: found and failed
		int32_t CheckComplete(const std::string &chash);
		bool SyncPreProcess(const protocol::ConsensusValue& consensus_value, bool propose, ProposeTxsResult &propose_result);

		//<0 : processing 1: found and success 0: found and failed
//		int32_t AsyncPreProcess(const protocol::ConsensusValue& consensus_value, int64_t timeout, PreProcessCallback callback, int32_t &timeout_tx_index);
		LedgerFrm::pointer SyncProcess(const protocol::ConsensusValue& consensus_value); //for ledger closing
	};

}
#endif //end of ifndef
