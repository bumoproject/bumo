
#ifndef NOTARY_MGR_H_
#define NOTARY_MGR_H_

#include <utils/singleton.h>
#include <utils/thread.h>
#include <notary/chain_rpc.h>
#include <notary/configure.h>

namespace bumo {
	//save and control self chain
	typedef enum ExecuteState{
		EXECUTE_STATE_INITIAL = 1,
		EXECUTE_STATE_PROCESSING = 2,
		EXECUTE_STATE_FAIL = 3,
		EXECUTE_STATE_SUCCESS = 4
	};

	class ChainObj : public INotifyRpc{
	public:
		ChainObj(const ChainConfigure &config);
		~ChainObj();

		typedef std::map<int64_t, ProposalInfo> ProposalInfoMap;

		typedef struct tagProposalRecord{
			ProposalInfoMap proposal_info_map;
			int64_t max_seq;
			int64_t affirm_max_seq;

			void Reset(){
				proposal_info_map.clear();
				max_seq = -1;
				affirm_max_seq = -1;
			}

			Json::Value ToJson(){
				Json::Value data;
				data["max_seq"] = max_seq;
				data["affirm_max_seq"] = affirm_max_seq;
				data["proposal_map"] = Json::Value(Json::arrayValue);
				Json::Value &proposal_map = data["proposal_map"];;
				for (auto itr = proposal_info_map.begin(); itr != proposal_info_map.end(); itr++){
					Json::Value proposal;
					proposal["seq"] = itr->first;
					proposal["proposal"] = itr->second.ToJson();
					proposal_map[proposal_map.size()] = proposal;
				}

				return data;
			}

		}ProposalRecord;

		void ResetChainInfo() {
			send_record_.Reset();
			recv_record_.Reset();
			comm_info_.Reset();
			error_tx_times_ = -1;
			tx_history_.clear();
			memset(&recv_notary_list_, 0, sizeof(recv_notary_list_));
			memset(&send_notary_list_, 0, sizeof(send_notary_list_));
		}

		void SetPeerChain(std::shared_ptr<ChainObj> peer_chain);
		void OnSlowTimer(int64_t current_time);
		void OnFastTimer(int64_t current_time);
		bool GetProposalInfo(ProposalType type, int64_t seq, ProposalInfo &info);
		std::string GetChainName();

		Json::Value ToJson();

	private:
		//事件处理
		virtual void HandleTxResult(const std::string &hash, int32_t code) override;

		//内部函数处理
		void RequestCommContractInfo();
		void RequestProposal(ProposalType type);
		void CheckTxError();
		void CreateVotingProposals(ProposalType type);
		bool BuildSingleVoting(ProposalType type, const ProposalRecord &record, ProposalType peer_type, int64_t next_proposal_seq);
		void CommitVotingProposals();

		bool GetProposal(ProposalType type, int64_t seq);

		ProposalRecord* GetProposalRecord(ProposalType type); //使用的时候，需要在外部加锁

	private:
		utils::Mutex lock_;
		std::shared_ptr<ChainObj> peer_chain_;
		const ChainConfigure &chain_config_;
		ProposalInfoVector proposal_info_vector_;

		//Chain Info
		ProposalRecord send_record_;
		ProposalRecord recv_record_;
		int64_t error_tx_times_;
		utils::StringList tx_history_;
		std::string recv_notary_list_[100];
		std::string send_notary_list_[100];
		std::shared_ptr<BaseChainRpc> chain_rpc_;
		CommContractInfo comm_info_;
	};

	typedef std::vector<std::shared_ptr<ChainObj>> ChainObjVector;

	class NotaryMgr : public utils::Singleton<NotaryMgr>, public TimerNotify{
		friend class utils::Singleton<bumo::NotaryMgr>;
	public:
		NotaryMgr(){}
		~NotaryMgr(){}

		bool Initialize();
		bool Exit();

	private:
		virtual void OnTimer(int64_t current_time) override;
		virtual void OnSlowTimer(int64_t current_time) {}

	private:
		ChainObjVector chain_obj_vector_;
		int64_t last_update_time_;
		int64_t update_times_;
	};
}

#endif
