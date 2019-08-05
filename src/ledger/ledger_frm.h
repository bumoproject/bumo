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

#ifndef LEDGER_FRM_H_
#define LEDGER_FRM_H_

#include <utils/utils.h>
#include <proto/cpp/monitor.pb.h>
#include "transaction_frm.h"
#include "glue/transaction_set.h"
#include "account.h"
#include "proto/cpp/consensus.pb.h"

namespace bumo {

	class ProposeTxsResult {
	public:
		ProposeTxsResult();
		~ProposeTxsResult();

		bool block_timeout_;
		bool exec_result_;
		int64_t tx_execute_count_;
		protocol::ConsensusValueValidation cons_validation_;
		std::set<int32_t> need_dropped_tx_;

		void SetApply(ProposeTxsResult &result);
	};

	class LedgerContext;
	class LedgerFrm {
	public:
		typedef std::shared_ptr <LedgerFrm>	pointer;

		typedef enum tagAPPLY_MODE {
			APPLY_MODE_PROPOSE = 0,
			APPLY_MODE_CHECK = 1,
			APPLY_MODE_FOLLOW = 2
		} APPLY_MODE;

		LedgerFrm();
		~LedgerFrm();

		protocol::LedgerHeader GetProtoHeader() const {
		  	return ledger_.header();
		}

		protocol::Ledger &ProtoLedger();


		bool ApplyFollow(const protocol::ConsensusValue& request, 
			LedgerContext *ledger_context);

		bool ApplyPropose(const protocol::ConsensusValue& request,
			LedgerContext *ledger_context,
			ProposeTxsResult &proposed_result);

		bool ApplyCheck(const protocol::ConsensusValue& request,
			LedgerContext *ledger_context);

		bool Cancel();

		bool AddToDb(WRITE_BATCH& batch);

		bool LoadFromDb(int64_t seq);

		size_t GetTxCount() {
			return apply_tx_frms_.size();
		}

		size_t GetTxOpeCount() {
			size_t ope_count = 0;
			for (size_t i = 0; i < apply_tx_frms_.size(); i++) {
				const protocol::Transaction &tx = apply_tx_frms_[i]->GetTransactionEnv().transaction();
				ope_count += (tx.operations_size() > 0 ? tx.operations_size() : 1);
			}
			return ope_count;
		}

		static bool CheckConsValueValidation(const protocol::ConsensusValue& request,
			std::set<int32_t> &expire_txs_status,
			std::set<int32_t> &error_txs_status);
		static void SetValidationToProto(std::set<int32_t> expire_txs,
			std::set<int32_t> error_txs,
			protocol::ConsensusValueValidation &validation);

		Json::Value ToJson();

		bool Commit(KVTrie* trie, int64_t& new_count, int64_t& change_count);

		bool AllocateReward();
		bool AllocateRewardDirectly(int64_t total_reward);

		void SetTestMode(bool test_mode);
		bool IsTestMode();

	private:
		protocol::Ledger ledger_;
		bool is_test_mode_;
	public:
		std::shared_ptr<protocol::ConsensusValue> value_;
		std::vector<TransactionFrm::pointer> apply_tx_frms_;
		std::vector<TransactionFrm::pointer> dropped_tx_frms_;
		std::shared_ptr<Environment> environment_;
		LedgerContext *lpledger_context_;
		int64_t apply_time_;
		bool enabled_;
		int64_t total_fee_;
	};
}
#endif //end of ifndef
