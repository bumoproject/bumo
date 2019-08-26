
#ifndef NOTARY_CONFIGURE_H_
#define NOTARY_CONFIGURE_H_

#include <common/configure_base.h>

namespace bumo {
	class ChainConfigure {
	public:
		ChainConfigure(){}
		~ChainConfigure(){}

		std::string comm_unique_;
		std::string rpc_address_;
		std::string chain_name_;
		std::string comm_contract_;
		std::string notary_address_;
		std::string private_key_;

		std::string output_data_;

		bool Load(const Json::Value &value);
	};

	class CommConfigure {
	public:
		CommConfigure(){}
		~CommConfigure(){}

		bool use_contract_;
		std::string address_;
		bool use_output_;

		bool Load(const Json::Value &value);
	};

	typedef std::map<std::string, ChainConfigure> ChainMap;

	//congfig

	class Configure : public ConfigureBase, public utils::Singleton<Configure> {
		friend class utils::Singleton<Configure>;
		Configure(){}
		~Configure(){}

	public:
		CommConfigure comm_config_;
		LoggerConfigure logger_configure_;
		ChainMap chain_map_;

		virtual bool LoadFromJson(const Json::Value &values);

	private:
		std::string GetCommFromContract(const std::string &url, const std::string &key);
	};
}

#endif
