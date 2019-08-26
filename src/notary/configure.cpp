
#include <utils/utils.h>
#include <utils/file.h>
#include <utils/strings.h>
#include <utils/logger.h>
#include <common/general.h>
#include <notary/http_client.h>
#include "configure.h"

namespace bumo {
	bool ChainConfigure::Load(const Json::Value &value) {
		Configure::GetValue(value, "comm_unique", comm_unique_);
		Configure::GetValue(value, "rpc_address", rpc_address_);
		Configure::GetValue(value, "chain_name", chain_name_);
		Configure::GetValue(value, "comm_contract", comm_contract_);
		Configure::GetValue(value, "notary_address", notary_address_);
		Configure::GetValue(value, "private_key", private_key_);

		output_data_ = utils::String::Format("[%s]", chain_name_.c_str());
		return true;
	}

	bool CommConfigure::Load(const Json::Value &value) {
		use_output_ = false;
		Configure::GetValue(value, "use_contract", use_contract_);
		Configure::GetValue(value, "address", address_);
		Configure::GetValue(value, "use_output", use_output_);
		return true;
	}

	bool Configure::LoadFromJson(const Json::Value &values){
		if (!values.isMember("chain_1") ||
			!values.isMember("chain_2") ||
			!values.isMember("comm_config") ||
			!values.isMember("logger")) {
			LOG_STD_ERR("Some configuration not exist");
			return false;
		}


		comm_config_.Load(values["comm_config"]);

		logger_configure_.Load(values["logger"]);
		//BU chain && ETH chain Read config
		ChainConfigure chain1;
		chain1.Load(values["chain_1"]);
		ChainConfigure chain2;
		chain2.Load(values["chain_2"]);

		assert(chain1.comm_unique_ != chain2.comm_unique_);

		if (comm_config_.use_contract_){

			std::string urlA = utils::String::Format("%s&key=A", comm_config_.address_.c_str());
			std::string urlB = utils::String::Format("%s&key=B", comm_config_.address_.c_str());
			std::string A = GetCommFromContract(urlA, "A");
			std::string B = GetCommFromContract(urlB, "B");
			if (A.empty() || B.empty()){
				printf("GetCommFromContract A:%s, B:%s", A.c_str(), B.c_str());
				return false;
			}

			chain1.comm_contract_ = A;
			chain2.comm_contract_ = B;
		}

		chain_map_[chain1.comm_unique_] = chain1;
		chain_map_[chain2.comm_unique_] = chain2;


		return true;
	}

	std::string Configure::GetCommFromContract(const std::string &url, const std::string &key){
		std::string context = HttpGet(url);
		if (context.empty()){
			printf("error!!!!!! GetCommFromContract");
			return "";
		}

		Json::Value rpc_result;
		rpc_result.fromString(context);
		if (rpc_result["error_code"].asInt() != 0){
			LOG_ERROR("error_code is not 0, %s", context.c_str());
			return "";
		}

		Json::Value &result = rpc_result["result"];

		std::string value  = result[key]["value"].asString();

		return value;
	}
}
