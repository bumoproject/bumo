
#include <utils/headers.h>
#include <utils/common.h>
#include <common/general.h>
#include <notary/chain_rpc.h>
#include <proto/cpp/chain.pb.h>

namespace bumo {
	BaseChainRpc::BaseChainRpc(INotifyRpc *notify, const ChainConfigure &config){
		notify_ = notify;
		config_ = config;
	}

	BaseChainRpc::~BaseChainRpc(){

	}

	BumoChainRpc::BumoChainRpc(INotifyRpc *notify, const ChainConfigure &config) : BaseChainRpc(notify, config), private_key_(config.private_key_){
	}

	BumoChainRpc::~BumoChainRpc(){

	}

	bool BumoChainRpc::GetCommContractInfo(const std::string &address, CommContractInfo &comm_info){
		//Invoke contract query interface to get contract sender information
		Json::Value recv_relay_obj;
		do
		{
			std::string path = utils::String::Format("/getAccountMetaData?address=%s&key=receive_relay", address.c_str());
			std::string context = HttpGet(PackUrl(path));
			if (context.empty()){
				LOG_ERROR("%s:Bumo chain get comm contract info %s error", config_.output_data_.c_str(), config_.comm_contract_.c_str());
				return false;
			}
			Json::Value result;
			result.fromString(context);
			if (result["error_code"].asInt() != 0){
				return false;
			}
			recv_relay_obj.fromString(result["result"]["receive_relay"]["value"].asString());
		} while (false);

		Json::Value send_relay_obj;
		do
		{
			std::string path = utils::String::Format("/getAccountMetaData?address=%s&key=send_relay", address.c_str());
			std::string context = HttpGet(PackUrl(path));
			if (context.empty()){
				LOG_ERROR("%s:Bumo chain get comm contract info %s error", config_.output_data_.c_str(), config_.comm_contract_.c_str());
				return false;
			}

			Json::Value result;
			result.fromString(context);
			if (result["error_code"].asInt() != 0){
				return false;
			}
			send_relay_obj.fromString(result["result"]["send_relay"]["value"].asString());
		} while (false);

		//解析合约信息
		comm_info.comm_unique = send_relay_obj["f_chain_id"].asString();

		//recv参数
		comm_info.recv_finish_seq = recv_relay_obj["complete_seq"].asInt64();
		comm_info.recv_max_seq = recv_relay_obj["last_seq"].asInt64();
		const Json::Value &recv_notary_array = send_relay_obj["notary_list"];

		if (!send_relay_obj.isMember("notary_list")){
			LOG_ERROR("BUChain no notary list");
			return false;
		}

		for (Json::UInt i = 0; i < recv_notary_array.size(); i++){
			comm_info.recv_notarys.push_back(recv_notary_array[i].asString());
		}

		//send参数
		comm_info.send_finish_seq = send_relay_obj["complete_seq"].asInt64();
		comm_info.send_max_seq = send_relay_obj["last_seq"].asInt64();
		const Json::Value &send_notary_array = send_relay_obj["notary_list"];
		for (Json::UInt i = 0; i < send_notary_array.size(); i++){
			comm_info.send_notarys.push_back(send_notary_array[i].asString());
		}

		do
		{
			std::string path = utils::String::Format("/getLedger");
			std::string context = HttpGet(PackUrl(path));
			if (context.empty()){
				LOG_ERROR("%s:Bumo chain get ledger error", config_.output_data_.c_str());
				return false;
			}

			Json::Value result;
			result.fromString(context);
			if (result["error_code"].asInt() != 0){
				return false;
			}
			comm_info.cur_blockchain_seq = result["result"]["header"]["seq"].asInt64();
		} while (false);

		return true;
	}

	bool BumoChainRpc::GetProposal(const CommContractInfo &comm_info, const std::string &address, ProposalType type, int64_t seq, ProposalInfo &proposal_info){
		//调取合约查询接口，获取合约信息
		std::string path = utils::String::Format("/getAccountMetaData?address=%s&key=", address.c_str());
		std::string key;
		if (type == PROPOSAL_SEND){
			key = utils::String::Format("send_proposal_" FMT_I64 "", seq);
			path = utils::String::AppendFormat(path, "%s", key.c_str());
		}

		if (type == PROPOSAL_RECV){
			key = utils::String::Format("receive_proposal_" FMT_I64 "", seq);
			path = utils::String::AppendFormat(path, "%s", key.c_str());
		}

		if (seq <= 0){
			return false;
		}


		std::string context = HttpGet(PackUrl(path));
		if (context.empty()){
			LOG_TRACE("%s:Bumo chain get proposal info %s error", config_.output_data_.c_str(), config_.comm_contract_.c_str());
			return false;
		}

		Json::Value obj;
		obj.fromString(context);
		if (obj["error_code"].size() != 0){
			LOG_ERROR("%s:No proposals, address:%s, result:%s", config_.output_data_.c_str(), address.c_str(), context.c_str());
			return false;
		}

		Json::Value proposalBbj;
		proposalBbj.fromString(obj["result"][key]["value"].asString());
		if (proposalBbj["proposals"].size() <= 0){
			//LOG_TRACE("%s:No proposals, address:%s", config_.output_data_.c_str(), address.c_str());
			return false;
		}

		//解析提案信息
		const Json::Value &arrayObj = proposalBbj["proposals"][Json::UInt(0)]["votes"];
		for (Json::UInt i = 0; i < arrayObj.size(); i++){
			proposal_info.confirmed_notarys.push_back(arrayObj[i].asString());
		}
		proposal_info.proposal_body = proposalBbj.toFastString();
		proposal_info.proposal_id = proposalBbj["proposals"][Json::UInt(0)]["seq"].asInt64();
		proposal_info.status = proposalBbj["status"].asInt();
		proposal_info.type = type;
		proposal_info.inited_ = true;
		proposal_info.completed_ = false;
		if (proposal_info.status == 3 || proposal_info.status == 4){
			proposal_info.completed_ = true;
		}

		return true;
	}

	utils::StringVector BumoChainRpc::CommitVotedProposals(const ProposalInfoVector &proposal_vector){
		utils::StringVector hash_array;
		nonce_++;
		Json::Value trans_obj;
		trans_obj["fee_limit"] = "10000000";
		trans_obj["gas_price"] = 1000;
		trans_obj["nonce"] = nonce_;
		trans_obj["chain_id"] = 0;
		trans_obj["source_address"] = config_.notary_address_;
		trans_obj["operations"] = Json::Value(Json::arrayValue);
		Json::Value &operations = trans_obj["operations"];

		//打包操作
		for (unsigned i = 0; i < proposal_vector.size(); i++){
			Json::Value opt;
			opt["type"] = 7;
			opt["pay_coin"]["amount"] = 0;
			opt["pay_coin"]["dest_address"] = config_.comm_contract_;

			const ProposalInfo &info = proposal_vector[i];
			Json::Value input_value;
			Json::Value paramsObj;
			paramsObj.fromString(info.proposal_body);
			input_value["params"] = paramsObj;
			if (info.type == ProposalType::PROPOSAL_RECV){
				input_value["method"] = "receiveProposal";
			}
			else{
				input_value["method"] = "proposalAck";
			}
			opt["pay_coin"]["input"] = input_value.toFastString();
			operations[operations.size()] = opt;
		}

		std::string input = trans_obj.toFastString();
		std::string context = HttpPost(PackUrl("/getTransactionBlob"), input);
		if (context.empty()){
			LOG_ERROR("%s:Bumo chain get getTransactionBlob error", config_.output_data_.c_str());
			return hash_array;
		}

		Json::Value http_result;
		http_result.fromString(context);
		if (!http_result.isMember("error_code")){
			LOG_ERROR("%s:error_code is null", config_.output_data_.c_str());
			return hash_array;
		}

		if (http_result["error_code"].asInt() != 0){
			LOG_ERROR("%s:error_code is not 0, %d", config_.output_data_.c_str(), http_result["error_code"].asInt());
			return hash_array;
		}

		std::string hash = http_result["result"]["hash"].asString();
		std::string transaction_blob = http_result["result"]["transaction_blob"].asString();

		//构造签名交易，并发送交易
		std::string public_key = private_key_.GetEncPublicKey();
		std::string sign_data = utils::String::BinToHexString(private_key_.Sign(utils::String::HexStringToBin(transaction_blob)));
		Json::Value post_trans;
		post_trans["items"] = Json::Value(Json::arrayValue);
		Json::Value &items = post_trans["items"];
		Json::Value item;
		item["transaction_blob"] = transaction_blob;
		item["signatures"] = Json::Value(Json::arrayValue);
		Json::Value &signatures = item["signatures"];
		Json::Value signature;
		signature["sign_data"] = sign_data;
		signature["public_key"] = public_key;
		signatures[signatures.size()] = signature;
		items[items.size()] = item;

		std::string contextSubmit = HttpPost(PackUrl("/submitTransaction"), post_trans.toFastString());
		if (contextSubmit.empty()){
			LOG_ERROR("%s:Bumo chain get getTransactionBlob error", config_.output_data_.c_str());
			return hash_array;
		}

		Json::Value submit_result;
		submit_result.fromString(contextSubmit);
		if (!submit_result.isMember("success_count")){
			LOG_ERROR("%s:success_count is null", config_.output_data_.c_str());
			return hash_array;
		}

		if (submit_result["success_count"].asInt() <= 0){
			LOG_ERROR("%s:success_count <= 0, %d", config_.output_data_.c_str(), submit_result["success_count"].asInt());
			return hash_array;
		}

		hash_array.push_back(hash);
		return hash_array;
	}

	void BumoChainRpc::OnTimer(){
		//获取nonce值
		std::string path = utils::String::Format("/getAccount?address=%s", config_.notary_address_.c_str());
		std::string context = HttpGet(PackUrl(path));
		if (context.empty()){
			LOG_ERROR("%s:Bumo chain get getAccount error", config_.output_data_.c_str());
			return;
		}

		Json::Value rpc_result;
		rpc_result.fromString(context);
		if (rpc_result["error_code"].asInt() != 0){
			LOG_ERROR("%s:error_code is not 0, %s", config_.output_data_.c_str(), context.c_str());
			return;
		}

		Json::Value &result = rpc_result["result"];
		if (!result.isMember("nonce")){
			nonce_ = 0;
			return;
		}

		nonce_ = result["nonce"].asInt64();
	}

	std::string BumoChainRpc::PackUrl(const std::string &path){
		return (config_.rpc_address_ + path);
	}

	EthChainRpc::EthChainRpc(INotifyRpc *notify, const ChainConfigure &config) : BaseChainRpc(notify, config){
	}

	EthChainRpc::~EthChainRpc(){

	}

	bool EthChainRpc::GetCommContractInfo(const std::string &address, CommContractInfo &comm_info){
		//调取合约查询接口，获取合约发送端信息
		Json::Value com_info_obj;
		std::string path = utils::String::Format("/queryCommInfo?contract=%s", address.c_str());
		std::string context = HttpGet(PackUrl(path));
		if (context.empty()){
			LOG_ERROR("%s:Eth chain get comm contract info %s error", config_.output_data_.c_str(), config_.comm_contract_.c_str());
			return false;
		}
		com_info_obj.fromString(context);
		if (com_info_obj["error_code"].asInt() != 0){
			return false;
		}

		Json::Value &result = com_info_obj["result"];

		//解析合约信息
		comm_info.comm_unique = result["f_chain_id"].asString();

		//recv参数
		comm_info.recv_finish_seq = result["recv_complete_seq"].asInt64();
		comm_info.recv_max_seq = result["recv_last_seq"].asInt64();
		comm_info.recv_notarys.clear();//TODO 暂不填写

		//send参数
		comm_info.send_finish_seq = result["send_complete_seq"].asInt64();
		comm_info.send_max_seq = result["send_last_seq"].asInt64();
		comm_info.send_notarys.clear();//TODO 暂不填写

		comm_info.send_f_comm_addr = result["send_f_comm_addr"].asString();
		comm_info.send_t_comm_addr = result["send_t_comm_addr"].asString();
		comm_info.f_chain_id = result["f_chain_id"].asString();
		comm_info.t_chain_id = result["t_chain_id"].asString();

		do 
		{
			Json::Value blockchain_obj;
			std::string path = utils::String::Format("/queryChainInfo");
			std::string context = HttpGet(PackUrl(path));
			if (context.empty()){
				LOG_ERROR("%s:Eth chain get chain info %s error", config_.output_data_.c_str(), config_.comm_contract_.c_str());
				return false;
			}
			blockchain_obj.fromString(context);
			if (blockchain_obj["error_code"].asInt() != 0){
				return false;
			}

			comm_info.cur_blockchain_seq = blockchain_obj["result"]["seq"].asInt64();
		} while (false);

		return true;
	}

	bool EthChainRpc::GetProposal(const CommContractInfo &comm_info, const std::string &address, ProposalType type, int64_t seq, ProposalInfo &proposal_info){
		//调取合约查询接口，获取合约信息
		Json::Value query_proposal_obj;
		Json::Value query_proposal_state_obj;
		do 
		{
			std::string path = utils::String::Format("/queryProposal?contract=%s&from=%s&proposal_type=%d&seq=" FMT_I64 "", address.c_str(), config_.notary_address_.c_str(), type, seq);
			std::string context = HttpGet(PackUrl(path));
			if (context.empty()){
				LOG_ERROR("%s:Bumo chain get comm contract info %s error", config_.output_data_.c_str(), config_.comm_contract_.c_str());
				return false;
			}
			query_proposal_obj.fromString(context);
			if (query_proposal_obj["error_code"].asInt() != 0){
				LOG_ERROR("%s:Get proposals, error address:%s, result:%s", config_.output_data_.c_str(), address.c_str(), context.c_str());
				return false;
			}
		} while (false);
		
		do
		{
			std::string path = utils::String::Format("/queryProposalState?contract=%s&proposal_type=%d&seq=" FMT_I64 "", address.c_str(), type, seq);
			std::string context = HttpGet(PackUrl(path));
			if (context.empty()){
				LOG_ERROR("%s:Bumo chain get comm contract info %s error", config_.output_data_.c_str(), config_.comm_contract_.c_str());
				return false;
			}
			query_proposal_state_obj.fromString(context);
			if (query_proposal_state_obj["error_code"].asInt() != 0){
				LOG_ERROR("%s:Get proposals state, error address:%s, result:%s", config_.output_data_.c_str(), address.c_str(), context.c_str());
				return false;
			}
		} while (false);
		
		std::string temp = query_proposal_obj.toFastString();
		std::string temp1 = query_proposal_state_obj.toFastString();

		Json::Value &s_proposal = query_proposal_obj["result"];
		Json::Value &s_proposal_state = query_proposal_state_obj["result"];

		if (s_proposal["from"].asString().empty() || s_proposal["to"].asString().empty()){
			return false;
		}

		//解析提案信息
		if (s_proposal["self_voted"].asInt() == 1){
			//自己投过票的，将自己的信息填充
			proposal_info.confirmed_notarys.push_back(config_.notary_address_);
		}
		Json::Value proposal_body_obj;
		proposal_body_obj["status"] = s_proposal["status"].asInt();
		proposal_body_obj["voted_count"] = s_proposal_state["voted_count"].asInt();

		//Donot check PROPOSAL_SEND or RECV
		proposal_body_obj["proposals"] = Json::Value(Json::arrayValue);
		Json::Value &proposals = proposal_body_obj["proposals"];
		Json::Value t_proposal;
		t_proposal["seq"] = seq;
		t_proposal["asset_code"] = s_proposal["asset_code"].asString();
		t_proposal["from"] = s_proposal["from"].asString();;
		t_proposal["to"] = s_proposal["to"].asString();
		t_proposal["op_type"] = s_proposal["op_type"].asInt();
		t_proposal["data"]["amount"] = s_proposal["amount"].asString();;
		t_proposal["data"]["type"] = 0;
		t_proposal["data"]["asset_code"] = s_proposal["asset_code"].asString();
		if (s_proposal["op_type"].asInt() == 1){
			t_proposal["data"]["t_asset_addr"] = s_proposal["t_asset_addr"].asString();
			t_proposal["data"]["asset_code"] = s_proposal["asset_code"].asString();
		}
		if (s_proposal["op_type"].asInt() == 3){
			t_proposal["data"]["f_asset_addr"] = s_proposal["f_assets_addr"].asString();
		}
		if (type == PROPOSAL_SEND){
			t_proposal["f_comm_addr"] = comm_info.send_f_comm_addr;
			t_proposal["t_comm_addr"] = comm_info.send_t_comm_addr;
			t_proposal["f_chain_id"] = comm_info.f_chain_id;
			t_proposal["t_chain_id"] = comm_info.t_chain_id;
		}
		else{
			t_proposal["f_comm_addr"] = comm_info.send_t_comm_addr;
			t_proposal["t_comm_addr"] = comm_info.send_f_comm_addr; 
			t_proposal["f_chain_id"] = comm_info.t_chain_id;
			t_proposal["t_chain_id"] = comm_info.f_chain_id;
		}
		
		proposals[proposals.size()] = t_proposal;

		proposal_info.proposal_body = proposal_body_obj.toFastString();
		proposal_info.proposal_id = seq;
		proposal_info.status = s_proposal["status"].asInt();
		proposal_info.type = type;

		proposal_info.inited_ = false;
		proposal_info.completed_ = false;
		//六个区块确认
		int64_t inited_block_seq = s_proposal_state["inited_block_seq"].asInt64();
		if (inited_block_seq > 0 && (comm_info.cur_blockchain_seq - inited_block_seq) >= 6){
			proposal_info.inited_ = true;
		}

		int64_t completed_block_seq = s_proposal_state["completed_block_seq"].asInt64();
		if (completed_block_seq > 0 && (comm_info.cur_blockchain_seq - completed_block_seq) >= 6){
			proposal_info.completed_ = true;
		}

		//LOG_INFO("Type %d, seq:" FMT_I64 " state is waiting 6 blocks, [" FMT_I64 ", " FMT_I64 "].%s", type, seq, inited_block_seq, comm_info.cur_blockchain_seq, s_proposal_state.toFastString().c_str());
		return true;
	}

	utils::StringVector EthChainRpc::CommitVotedProposals(const ProposalInfoVector &proposal_vector){
		utils::StringVector hash_array;
		for (unsigned i = 0; i < proposal_vector.size(); i++){
			const ProposalInfo &info = proposal_vector[i];
			Json::Value body;
			body.fromString(info.proposal_body);

			std::string api;
			Json::Value paraObj;
			if (info.type == PROPOSAL_RECV){
				api = "receiveProposal";
				paraObj["contract"] = config_.comm_contract_.c_str();
				paraObj["from"] = config_.notary_address_.c_str();
				paraObj["priv"] = config_.private_key_.c_str();
				paraObj["params"]["seq"] = info.proposal_id;
				paraObj["params"]["asset_code"] = body["proposals"][Json::UInt(0)]["asset_code"].asString();
				paraObj["params"]["from"] = body["proposals"][Json::UInt(0)]["from"].asString();
				paraObj["params"]["to"] = body["proposals"][Json::UInt(0)]["to"].asString();  //TODO 转换小写

				int32_t op_type = body["proposals"][Json::UInt(0)]["op_type"].asInt();
				paraObj["params"]["op_type"] = op_type;
				if (op_type == 1){
					paraObj["params"]["arg1"] = body["proposals"][Json::UInt(0)]["data"]["t_asset_addr"].asString(); //TODO 转换小写
				}
				else if (op_type == 3){
					paraObj["params"]["arg1"] = "";
				}
				paraObj["params"]["arg2"] = body["proposals"][Json::UInt(0)]["data"]["amount"].asString();
				paraObj["params"]["arg3"] = body["proposals"][Json::UInt(0)]["data"]["type"].asString();
			}
			else if (info.type == PROPOSAL_SEND){
				api = "proposalAck";
				paraObj["contract"] = config_.comm_contract_.c_str();
				paraObj["from"] = config_.notary_address_.c_str();
				paraObj["val"] = 1;
				paraObj["priv"] = config_.private_key_.c_str();
				paraObj["params"]["seq"] = info.proposal_id;
				paraObj["params"]["ack_result"] = info.status;
			}

			std::string hash = SendEthTx(api, paraObj.toFastString());
			if (hash.empty()){
				return hash_array;
			}

			hash_array.push_back(hash);
			utils::Sleep(1);
		}

		return hash_array;
	}

	std::string EthChainRpc::SendEthTx(const std::string &api, const std::string &paras){
		std::string path = utils::String::Format("/%s", api.c_str());
		std::string contextSubmit = HttpPost(PackUrl(path), paras);
		if (contextSubmit.empty()){
			LOG_ERROR("%s:Eth chain get getTransactionBlob error", config_.output_data_.c_str());
			return "";
		}

		Json::Value submit_result;
		submit_result.fromString(contextSubmit);
		if (submit_result["error_code"].asInt() != 0){
			LOG_ERROR("%s:submit result error, post data:(%s), result:(%s)", config_.output_data_.c_str(), paras.c_str(), contextSubmit.c_str());
			return "";
		}

		return submit_result["result"]["hash"].asString();
	}

	std::string EthChainRpc::PackUrl(const std::string &path){
		return (config_.rpc_address_ + path);
	}

	std::shared_ptr<BaseChainRpc> CreateChainRpc(INotifyRpc *notify, const ChainConfigure &config){
		if (config.chain_name_ == "eth"){
			return std::make_shared<EthChainRpc>(notify, config);
		}

		if (config.chain_name_ == "bumo"){
			return std::make_shared<BumoChainRpc>(notify, config);
		}

		return nullptr;
	}
}

