
#ifndef CHAIN_RPC_H_
#define CHAIN_RPC_H_

#include <utils/singleton.h>
#include <utils/thread.h>
#include <notary/configure.h>
#include <common/private_key.h>
#include <notary/http_client.h>

namespace bumo {

	struct CommContractInfo{
		utils::StringVector recv_notarys;
		utils::StringVector send_notarys;
		std::string comm_unique;
		int64_t recv_finish_seq;
		int64_t recv_max_seq;
		int64_t send_finish_seq;
		int64_t send_max_seq;
		std::string send_f_comm_addr;
		std::string send_t_comm_addr;
		std::string f_chain_id;
		std::string t_chain_id;
		int64_t cur_blockchain_seq;

		void Reset(){
			recv_notarys.clear();
			send_notarys.clear();
			comm_unique = "";
			recv_finish_seq = 0;
			recv_max_seq = 0;
			send_finish_seq = 0;
			send_max_seq = 0;
			cur_blockchain_seq = 0;
		}
	};

	enum ProposalType{
		PROPOSAL_NONE = 0,
		PROPOSAL_SEND = 1,
		PROPOSAL_RECV = 2,
	};

	struct ProposalInfo{
		ProposalType type;
		int64_t proposal_id;
		std::string proposal_body;
		int32_t status;
		utils::StringVector confirmed_notarys;
		bool inited_;
		bool completed_;
	};

	typedef std::vector<ProposalInfo> ProposalInfoVector;

	class INotifyRpc{
	public:
		virtual void HandleTxResult(const std::string &hash, int32_t code) = 0;
	};

	class BaseChainRpc{
	public:
		BaseChainRpc(INotifyRpc *notify, const ChainConfigure &config);
		~BaseChainRpc();

	public:
		virtual bool GetCommContractInfo(const std::string &address, CommContractInfo &comm_info) = 0;
		virtual bool GetProposal(const CommContractInfo &comm_info, const std::string &address, ProposalType type, int64_t seq, ProposalInfo &proposal_info) = 0;
		virtual utils::StringVector CommitVotedProposals(const ProposalInfoVector &proposal_vector) = 0;
		virtual void OnTimer() = 0;

	protected:
		INotifyRpc *notify_;
		ChainConfigure config_;
	};

	class BumoChainRpc : public BaseChainRpc{
	public:
		BumoChainRpc(INotifyRpc *notify, const ChainConfigure &config);
		~BumoChainRpc();
		virtual bool GetCommContractInfo(const std::string &address, CommContractInfo &comm_info) override;
		virtual bool GetProposal(const CommContractInfo &comm_info, const std::string &address, ProposalType type, int64_t seq, ProposalInfo &proposal_info) override;
		virtual utils::StringVector CommitVotedProposals(const ProposalInfoVector &proposal_vector) override;
		virtual void OnTimer() override;

	private:
		int64_t nonce_;
		PrivateKey private_key_;

	private:
		std::string PackUrl(const std::string &path);
	};


	class EthChainRpc : public BaseChainRpc{
	public:
		EthChainRpc(INotifyRpc *notify, const ChainConfigure &config);
		~EthChainRpc();
		virtual bool GetCommContractInfo(const std::string &address, CommContractInfo &comm_info) override;
		virtual bool GetProposal(const CommContractInfo &comm_info, const std::string &address, ProposalType type, int64_t seq, ProposalInfo &proposal_info) override;
		virtual utils::StringVector CommitVotedProposals(const ProposalInfoVector &proposal_vector) override;
		virtual void OnTimer() override {}

	private:
		std::string SendEthTx(const std::string &api, const std::string &paras);

	private:
		std::string PackUrl(const std::string &path);
	};

	std::shared_ptr<BaseChainRpc> CreateChainRpc(INotifyRpc *notify, const ChainConfigure &config);

}

#endif
