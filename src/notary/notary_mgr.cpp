
#include <utils/headers.h>
#include <utils/common.h>
#include <common/general.h>
#include <notary/configure.h>
#include <notary/notary_mgr.h>
#include <utils/file.h>

const int64_t max_submit_nums = 3;

namespace bumo {
	ChainObj::ChainObj(const ChainConfigure &config) : chain_config_(config){
		ResetChainInfo();
		peer_chain_ = nullptr;
		chain_rpc_ = CreateChainRpc(this, config);
	}


	ChainObj::~ChainObj(){

	}

	void ChainObj::OnSlowTimer(int64_t current_time){
		chain_rpc_->OnTimer();

		CreateVotingProposals(ProposalType::PROPOSAL_SEND);

		CreateVotingProposals(ProposalType::PROPOSAL_RECV);

		CommitVotingProposals();
	}

	void ChainObj::OnFastTimer(int64_t current_time){
		//Reset
		ResetChainInfo();

		//请求通信合约的信息
		RequestCommContractInfo();

		//Get the latest send list and sort outmap
		RequestProposal(ProposalType::PROPOSAL_SEND);

		//Get the latest recv list and sort the inputmap
		RequestProposal(ProposalType::PROPOSAL_RECV);

		CheckTxError();
	}

	void ChainObj::SetPeerChain(std::shared_ptr<ChainObj> peer_chain){
		peer_chain_ = peer_chain;
	}

	bool ChainObj::GetProposalInfo(ProposalType type, int64_t seq, ProposalInfo &info){
		utils::MutexGuard guard(lock_);
		ProposalRecord *record = GetProposalRecord(type);
		auto itr_info = record->proposal_info_map.find(seq);
		if (itr_info == record->proposal_info_map.end()){
			if (!GetProposal(type, seq)){
				return false;
			}

			ProposalRecord *second_record = GetProposalRecord(type);
			auto second_itr_info = second_record->proposal_info_map.find(seq);
			if (second_itr_info == second_record->proposal_info_map.end()){
				LOG_ERROR("%s:Error for find proposal:" FMT_I64 ", type:%d", chain_config_.output_data_.c_str(), seq, type);
				return false;
			}
			info = second_itr_info->second;
			return true;
		}
		info = itr_info->second;
		return true;
	}

	std::string ChainObj::GetChainName(){
		return chain_config_.chain_name_;
	}

	Json::Value ChainObj::ToJson(){
		Json::Value data;
		data["comm_info"] = comm_info_.ToJson();
		data["send"] = send_record_.ToJson();
		data["recv"] = recv_record_.ToJson();
		return data;
	}

	void ChainObj::RequestCommContractInfo(){
		if (!chain_rpc_->GetCommContractInfo(chain_config_.comm_contract_, comm_info_)){
			return;
		}

		//保存发送端公证人列表
		if (comm_info_.recv_notarys.size() >= 100){
			LOG_ERROR("Notary nums is no more than 100");
			return;
		}
		memset(&recv_notary_list_, 0, sizeof(recv_notary_list_));
		for (uint32_t i = 0; i < comm_info_.recv_notarys.size(); i++) {
			memcpy(&recv_notary_list_[i], comm_info_.recv_notarys[i].data(), comm_info_.recv_notarys[i].size());
		}

		//保存接送端公证人列表
		if (comm_info_.send_notarys.size() >= 100){
			LOG_ERROR("Notary nums is no more than 100");
			return;
		}
		memset(&send_notary_list_, 0, sizeof(send_notary_list_));
		for (uint32_t i = 0; i < comm_info_.send_notarys.size(); i++) {
			memcpy(&send_notary_list_[i], comm_info_.send_notarys[i].data(), comm_info_.send_notarys[i].size());
		}

		//保存最大确认序号和最大序号
		recv_record_.affirm_max_seq = MAX(recv_record_.affirm_max_seq, comm_info_.recv_finish_seq);
		recv_record_.max_seq = MAX(recv_record_.max_seq, comm_info_.recv_max_seq);

		send_record_.affirm_max_seq = MAX(send_record_.affirm_max_seq, comm_info_.send_finish_seq);
		send_record_.max_seq = MAX(send_record_.max_seq, comm_info_.send_max_seq);
	}

	bool ChainObj::GetProposal(ProposalType type, int64_t seq){
		ProposalInfo proposal_info;
		if (!chain_rpc_->GetProposal(comm_info_, chain_config_.comm_contract_, type, seq, proposal_info)){
			return false;
		}

		ProposalRecord *proposal = GetProposalRecord(proposal_info.type);

		proposal->proposal_info_map[proposal_info.proposal_id] = proposal_info;
		proposal->max_seq = MAX(proposal_info.proposal_id, proposal->max_seq);
		if ((proposal_info.status == EXECUTE_STATE_SUCCESS) || (proposal_info.status == EXECUTE_STATE_FAIL)){
			proposal->affirm_max_seq = MAX(proposal_info.proposal_id, proposal->affirm_max_seq);
		}

		LOG_TRACE("%s,Recv proposal type:%d, id:" FMT_I64 ", status:%d ", chain_config_.output_data_.c_str(), proposal_info.type, proposal_info.proposal_id, proposal_info.status);
		return true;
	}

	void ChainObj::HandleTxResult(const std::string &hash, int32_t code){
		auto iter = std::find(tx_history_.begin(), tx_history_.end(), hash);
		if (iter == tx_history_.end()){
			return;
		}

		if (code == 0){
			error_tx_times_ = 0;
			tx_history_.clear();
			return;
		}
		error_tx_times_++;
		LOG_ERROR("%s:Failed to Do Transaction,tx hash is %s,err_code is %d", chain_config_.output_data_.c_str(), hash.c_str(), code);
	}

	void ChainObj::RequestProposal(ProposalType type){
		utils::MutexGuard guard(lock_);
		ProposalRecord *record = GetProposalRecord(type);
		int64_t max_nums = MIN(max_submit_nums, (record->max_seq - record->affirm_max_seq));
		if (max_nums <= 0){
			max_nums = 1;
		}
		//循环处理
		for (int64_t i = 1; i <= max_nums; i++){
			int64_t seq = record->affirm_max_seq + i;
			if (!GetProposal(type, seq)){
				break;
			}
		}
	}

	void ChainObj::CheckTxError(){
		//TODO 处理异常的交易
	}

	void ChainObj::CreateVotingProposals(ProposalType type){
		utils::MutexGuard guard(lock_);
		ProposalRecord *record = GetProposalRecord(type);

		ProposalType get_peer_type;
		if (type == ProposalType::PROPOSAL_SEND){
			get_peer_type = ProposalType::PROPOSAL_RECV;
		}
		else if (type == ProposalType::PROPOSAL_RECV){
			get_peer_type = ProposalType::PROPOSAL_SEND;
		}
		else{
			LOG_ERROR("%s:Unknown proposal type.", chain_config_.output_data_.c_str());
			return;
		}

		record->affirm_max_seq = MAX(0, record->affirm_max_seq);
		int64_t next_proposal_seq = record->affirm_max_seq + 1;
		for (int64_t i = 0; i < max_submit_nums; i++){
			if (!BuildSingleVoting(type, *record, get_peer_type, next_proposal_seq)){
				break;
			}
			next_proposal_seq++;
		}
	}

	bool ChainObj::BuildSingleVoting(ProposalType type, const ProposalRecord &record, ProposalType peer_type, int64_t next_proposal_seq){
		ProposalInfo vote_proposal;
		//先获取对端列表
		if (!peer_chain_->GetProposalInfo(peer_type, next_proposal_seq, vote_proposal)){
			//不存在在直接返回
			//LOG_INFO("Chain %s,no proposl from peer type :%d", comm_unique_.c_str(), peer_type);
			return false;
		}

		//当获取对端为send时候，需要保证其状态非ok状态
		if (peer_type == ProposalType::PROPOSAL_SEND){
			if (vote_proposal.status == EXECUTE_STATE_SUCCESS || vote_proposal.status == EXECUTE_STATE_FAIL){
				LOG_INFO("%s:If peers' output is sucess or fail, ignore it.%d", chain_config_.output_data_.c_str(), vote_proposal.status);
				return false;
			}
			if (!vote_proposal.inited_){
				LOG_INFO("%s:Peers' send proposal is waiting 6 blocks for init. seq:" FMT_I64 "", chain_config_.output_data_.c_str(), next_proposal_seq);
				return false;
			}
		}
		//当获取对端为recv时候，需要保证其状态为成功状态或者失败状态
		else if (peer_type == ProposalType::PROPOSAL_RECV){
			if (vote_proposal.status == EXECUTE_STATE_INITIAL || vote_proposal.status == EXECUTE_STATE_PROCESSING){
				LOG_INFO("%s:If peers' input is init or processing, ignore it, status:%d", chain_config_.output_data_.c_str(), vote_proposal.status);
				return false;
			}

			if (!vote_proposal.completed_){
				LOG_INFO("%s:Peers' send proposal is waiting 6 blocks for completed. seq:" FMT_I64 "", chain_config_.output_data_.c_str(), next_proposal_seq);
				return false;
			}

		}

		//再判断自己提案列表
		const ProposalInfoMap &proposal_info_map = record.proposal_info_map;
		auto itr = proposal_info_map.find(next_proposal_seq);
		if (itr != proposal_info_map.end()){
			//如果存在则判断自己是否投过票，投过票则忽略处理
			const ProposalInfo &proposal = itr->second;
			for (uint32_t i = 0; i < proposal.confirmed_notarys.size(); i++){
				if (proposal.confirmed_notarys[i] == chain_config_.notary_address_){
					return true;
				}
			}
		}

		//改变提案状态类型，保存提案队列。
		vote_proposal.type = type;
		vote_proposal.confirmed_notarys.clear();

		proposal_info_vector_.push_back(vote_proposal);
		return true;
	}

	void ChainObj::CommitVotingProposals(){
		if (proposal_info_vector_.empty()){
			return;
		}
		
		utils::StringVector hash_vector = chain_rpc_->CommitVotedProposals(proposal_info_vector_);
		if (!hash_vector.empty()){
			for (uint32_t i = 0; i < hash_vector.size(); i++){
				LOG_INFO("%s:CommitVotingProposals hash :%s", chain_config_.output_data_.c_str(), hash_vector[i].c_str());
			}
		}
		
		proposal_info_vector_.clear();
	}

	ChainObj::ProposalRecord* ChainObj::GetProposalRecord(ProposalType type){
		ProposalRecord *record = nullptr;
		if (type == ProposalType::PROPOSAL_SEND){
			record = &send_record_;
		}
		else if (type == ProposalType::PROPOSAL_RECV){
			record = &recv_record_;
		}
		else{
			LOG_ERROR("%s:Canot find proposal, type:%d", chain_config_.output_data_.c_str(), type);
		}
		return record;
	}

	bool NotaryMgr::Initialize(){
		TimerNotify::RegisterModule(this);

		last_update_time_ = utils::Timestamp::HighResolution();
		update_times_ = 0;
		
		LOG_INFO("Initialized notary mgr successfully");

		ChainMap::iterator itr = Configure::Instance().chain_map_.begin();
		const ChainConfigure &config_a = itr->second;
		itr++;
		const ChainConfigure &config_b = itr->second;
		std::shared_ptr<ChainObj> a = std::make_shared<ChainObj>(config_a);
		std::shared_ptr<ChainObj> b = std::make_shared<ChainObj>(config_b);

		chain_obj_vector_.push_back(a);
		chain_obj_vector_.push_back(b);

		a->SetPeerChain(b);
		b->SetPeerChain(a);

		return true;
	}

	bool NotaryMgr::Exit(){
		return true;
	}

	void NotaryMgr::OnTimer(int64_t current_time){
		if ((current_time - last_update_time_) < 3 * utils::MICRO_UNITS_PER_SEC){
			return;
		}
		last_update_time_ = current_time;
		update_times_++;

		//3秒更新状态
		for (uint32_t i = 0; i < chain_obj_vector_.size(); i++){
			chain_obj_vector_[i]->OnFastTimer(current_time);
		}

		//12秒提交提案，并输出日志到文件
		if (update_times_ % 4 == 0){
			LOG_INFO("On timer for heartbeat ..");
			for (uint32_t i = 0; i < chain_obj_vector_.size(); i++){
				chain_obj_vector_[i]->OnSlowTimer(current_time);
			}
			if (bumo::Configure::Instance().comm_config_.use_output_){
				Json::Value debug;
				for (uint32_t i = 0; i < chain_obj_vector_.size(); i++){
					debug[chain_obj_vector_[i]->GetChainName()] = chain_obj_vector_[i]->ToJson();
				}

				std::string output_data = debug.toFastString();
				std::string output_file = utils::String::Format("%s/debug-output", utils::File::GetBinHome().c_str());
				if (utils::File::IsExist(output_file)){
					utils::File::Delete(output_file);
				}
				utils::File file;
				if (!file.Open(output_file, utils::File::FILE_M_WRITE | utils::File::FILE_M_TEXT)){
					LOG_ERROR("Open output file error!");
					return;
				}

				file.Write(output_data.data(), 1, output_data.size());
				file.Close();
			}
		}
	}
}

