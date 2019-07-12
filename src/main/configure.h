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

#ifndef CONFIGURE_H_
#define CONFIGURE_H_

#include <common/configure_base.h>

namespace bumo {

	class P2pNetwork {
	public:
		P2pNetwork();
		~P2pNetwork();

		size_t target_peer_connection_;
		size_t max_connection_;
		int64_t connect_timeout_;
		int64_t heartbeat_interval_;
		int32_t listen_port_;
		utils::StringList known_peer_list_;
		bool Load(const Json::Value &value);
	};

	class P2pConfigure {
	public:
		P2pConfigure();
		~P2pConfigure();

		std::string node_private_key_;
		int64_t network_id_;
		P2pNetwork consensus_network_configure_;

		bool Load(const Json::Value &value);
	};

	class WsServerConfigure {
	public:
		WsServerConfigure();
		~WsServerConfigure();

		utils::InetAddress listen_address_;

		bool Load(const Json::Value &value);
	};

	class WebServerConfigure {
	public:
		WebServerConfigure();
		~WebServerConfigure();

		utils::InetAddressList listen_addresses_;

		std::string directory_;
		std::string index_name_;
		bool ssl_enable_;
		uint32_t query_limit_;
		uint32_t multiquery_limit_;
		SSLConfigure ssl_configure_;
		uint32_t thread_count_;
		std::string allow_origin_;
		bool Load(const Json::Value &value);
	};

	class LedgerConfigure {
	public:
		LedgerConfigure();
		~LedgerConfigure();

		std::string validation_type_;
		int64_t close_interval_;
		std::string validation_privatekey_;
		std::string validation_address_;
		bool validation_random;
		uint32_t hash_type_;
		uint32_t max_trans_per_ledger_;
		uint32_t max_ledger_per_message_;
		uint32_t max_trans_in_memory_;
		uint32_t max_apply_ledger_per_round_;
		uint32_t queue_limit_;
		uint32_t queue_per_account_txs_limit_;
		utils::StringList hardfork_points_;
		utils::StringList forbid_addrs_;
		bool use_atom_map_;
		bool Load(const Json::Value &value);
	};

	class GenesisConfigure {
	public:
		GenesisConfigure();
		~GenesisConfigure();

		struct FeeConfigure {
			int64_t gas_price_;
			int64_t base_reserve_;
		};
		FeeConfigure fees_;
		std::string slogan_;

		std::string account_;
		utils::StringList validators_;

		int64_t chain_id_;
		bool Load(const Json::Value &value);
	};

	class MonitorConfigure {
	public:
		MonitorConfigure();
		~MonitorConfigure();

		std::string id_;
		std::string center_;
		std::string disk_path_;
		bool enabled_;
		bool Load(const Json::Value &value);
	};

	class Configure : public ConfigureBase, public utils::Singleton<Configure> {
		friend class utils::Singleton<Configure>;
		Configure();
		~Configure();

	public:
		DbConfigure db_configure_;
		LoggerConfigure logger_configure_;

		WebServerConfigure webserver_configure_;
		WsServerConfigure wsserver_configure_; //Websocket server
		
		P2pConfigure p2p_configure_;
		LedgerConfigure ledger_configure_;
		GenesisConfigure genesis_configure_;

		MonitorConfigure monitor_configure_;

		virtual bool LoadFromJson(const Json::Value &values);
	};
}

#endif
