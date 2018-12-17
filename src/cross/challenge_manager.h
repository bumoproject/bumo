#ifndef CHALLENGE_MANAGER_H_
#define CHALLENGE_MANAGER_H_

#include <utils/singleton.h>
#include <utils/thread.h>
#include "ledger/ledger_frm.h"
#include <cross/cross_utils.h>
namespace bumo {

	typedef std::map<int64_t, protocol::LedgerHeader> LedgerMap;
	class ChallengeSubmitHead
	{
	public:
		ChallengeSubmitHead();
		~ChallengeSubmitHead();
		void InitSeq();
		void UpdateStatus();
		void SortMap();
		void RequestLost();
	private:
		utils::Mutex common_lock_;
		LedgerMap ledger_map_;
		int64_t chain_head_seq_;
		int64_t recv_max_seq_;
		int64_t latest_seq_;
	};

	typedef std::map<int64_t, protocol::MessageChannelWithdrawal> WithdrawalMap;
	class ChallengeWithdrawal
	{
	public:
		ChallengeWithdrawal();
		~ChallengeWithdrawal();
		void InitSeq();
	private:
		WithdrawalMap withdrawal_map;
		int64_t chain_withdrawal_seq_;
		int64_t recv_max_seq;
		int64_t latest_seq;
	};

	class ChallengeManager :public utils::Runnable{
	public:
		ChallengeManager();
		virtual ~ChallengeManager();
		bool Initialize();
		bool Exit();
		void ChallengeNotify(const protocol::MessageChannel &message_channel);
	private:
		void Run(utils::Thread *thread);
		void HandleChallengeSubmitHead(const protocol::MessageChannel &message_channel);
		void HandleChallengeWithdrawal(const protocol::MessageChannel &message_channel);
		void InitSeq();
	private:
		bool enabled_;
		utils::Thread *thread_ptr_;
		std::shared_ptr<ChallengeSubmitHead> challenge_submit_head_;
		std::shared_ptr<ChallengeWithdrawal> challenge_withdrawal_;
	};
}

#endif