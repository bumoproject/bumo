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

#include <utils/utils.h>
#include <utils/file.h>
#include <utils/strings.h>
#include <utils/logger.h>
#include <common/general.h>
#include "configure.h"
#include <common/private_key.h>
#include <utils/net.h>
namespace bumo {

	P2pNetwork::P2pNetwork() :
		target_peer_connection_(10),
		max_connection_(2000),
		connect_timeout_(5),// second
		heartbeat_interval_(1800) {// second
			listen_port_ = General::CONSENSUS_PORT;
	}

	P2pNetwork::~P2pNetwork() {}

	P2pConfigure::P2pConfigure() :
		network_id_(1){}

	P2pConfigure::~P2pConfigure() {}

	bool P2pConfigure::Load(const Json::Value &value) {
		Configure::GetValue(value, "node_private_key", node_private_key_);
		Configure::GetValue(value, "network_id", network_id_);		
		consensus_network_configure_.Load(value["consensus_network"]);
		if (node_private_key_.empty()){
			PrivateKey priv_key(SIGNTYPE_ED25519);
			node_private_key_ = priv_key.GetEncPrivateKey();
		}
		else {
			node_private_key_ = utils::Aes::HexDecrypto(node_private_key_, GetDataSecuretKey());
		}
		return true;
	}

	bool P2pNetwork::Load(const Json::Value &value) {
		int32_t temp = (int32_t)target_peer_connection_;
		Configure::GetValue(value, "target_peer_connection", temp);
		target_peer_connection_ = temp;

		temp = (int32_t)max_connection_;
		Configure::GetValue(value, "max_connection", temp);
		max_connection_ = temp;

		Configure::GetValue(value, "known_peers", known_peer_list_);
		Configure::GetValue(value, "connect_timeout", connect_timeout_);
		Configure::GetValue(value, "heartbeat_interval", heartbeat_interval_);
		Configure::GetValue(value, "listen_port", listen_port_);

		connect_timeout_ = connect_timeout_ * utils::MICRO_UNITS_PER_SEC; //micro second
		heartbeat_interval_ = heartbeat_interval_ * utils::MICRO_UNITS_PER_SEC; //micro second
		return true;
	}

	WsServerConfigure::~WsServerConfigure() {}

	bool WsServerConfigure::Load(const Json::Value &value) {
		std::string address;
		Configure::GetValue(value, "listen_address", address);
		listen_address_ = utils::InetAddress(address);

		return true;
	}

	WsServerConfigure::WsServerConfigure() {
	}

	WebServerConfigure::WebServerConfigure() {
		ssl_enable_ = false;
		query_limit_ = 1000;
		multiquery_limit_ = 100;
		thread_count_ = 0;
	}

	WebServerConfigure::~WebServerConfigure() {}

	bool WebServerConfigure::Load(const Json::Value &value) {
		std::string listen_address_value;
		ConfigureBase::GetValue(value, "listen_addresses", listen_address_value);
		utils::StringVector address_array = utils::String::Strtok(listen_address_value, ',');
		for (size_t i = 0; i < address_array.size(); i++) {
			listen_addresses_.push_back(utils::InetAddress(address_array[i]));
		}
		ConfigureBase::GetValue(value, "index_name", index_name_);
		ConfigureBase::GetValue(value, "directory", directory_);
		ConfigureBase::GetValue(value, "ssl_enable", ssl_enable_);
		ConfigureBase::GetValue(value, "query_limit", query_limit_);
		ConfigureBase::GetValue(value, "multiquery_limit", multiquery_limit_);
		ConfigureBase::GetValue(value, "thread_count", thread_count_);
		ConfigureBase::GetValue(value, "allow_origin", allow_origin_);
		
		
		if (ssl_enable_)
			ssl_configure_.Load(value["ssl"]);
		return true;
	}

	LedgerConfigure::LedgerConfigure() {
		max_trans_per_ledger_ = 2000;
		max_trans_in_memory_ = 50000;
		max_ledger_per_message_ = 20;
		max_apply_ledger_per_round_ = 5;
		close_interval_ = 10;
		use_atom_map_ = true;
		hash_type_ = 0; // 0 : SHA256, 1 :SM2
		queue_limit_ = 10240;
		queue_per_account_txs_limit_ = 64;
		validation_random = false;
	}

	LedgerConfigure::~LedgerConfigure() {
	}

	bool LedgerConfigure::Load(const Json::Value &value) {
		//Configure::GetValue(value, "close_interval", close_interval_);
		Configure::GetValue(value, "validation_type", validation_type_);
		Configure::GetValue(value, "validation_private_key", validation_privatekey_);
		Configure::GetValue(value, "hash_type", hash_type_);
		Configure::GetValue(value, "max_trans_per_ledger", max_trans_per_ledger_);
		Configure::GetValue(value, "max_ledger_per_message", max_ledger_per_message_);
		Configure::GetValue(value, "max_apply_ledger_per_round", max_apply_ledger_per_round_);
		Configure::GetValue(value, "max_trans_in_memory", max_trans_in_memory_);
		Configure::GetValue(value, "hardfork_points", hardfork_points_);

		Configure::GetValue(value, "forbid_addrs", forbid_addrs_);
		Configure::GetValue(value, "use_atom_map", use_atom_map_);

		Configure::GetValue(value["tx_pool"], "queue_limit", queue_limit_);
        Configure::GetValue(value["tx_pool"], "queue_per_account_txs_limit", queue_per_account_txs_limit_);

		if (validation_privatekey_.empty()) {
			PrivateKey tmp_priv(SIGNTYPE_ED25519);
			validation_privatekey_ = tmp_priv.GetEncPrivateKey();
			validation_address_ = tmp_priv.GetEncAddress();
			validation_random = true;
		}
		else {
			if (validation_privatekey_.substr(0, 4) != "priv") {
				validation_privatekey_ = utils::Aes::HexDecrypto(validation_privatekey_, GetDataSecuretKey());
			} 
			PrivateKey priv(validation_privatekey_);
			validation_address_ = priv.GetEncAddress();
		}
		close_interval_ = close_interval_ * utils::MICRO_UNITS_PER_SEC; //micro second

		if (max_apply_ledger_per_round_ == 0
			|| max_trans_in_memory_ / max_apply_ledger_per_round_ == 0) {
			return false;
		}
		return true;
	}

	GenesisConfigure::GenesisConfigure() {
		memset(&fees_, 0, sizeof(fees_));
	}

	GenesisConfigure::~GenesisConfigure() {}

	bool GenesisConfigure::Load(const Json::Value &value) {
		Configure::GetValue(value, "validators", validators_);
		Configure::GetValue(value, "account", account_);
		Configure::GetValue(value, "slogan", slogan_);
		Configure::GetValue(value, "chain_id", chain_id_);
		
		if (validators_.empty()) {
			return false;
		}

		//for fee
		Configure::GetValue(value["fees"], "gas_price", fees_.gas_price_);
		Configure::GetValue(value["fees"], "base_reserve", fees_.base_reserve_);
		if (fees_.gas_price_ < 0)
			fees_.gas_price_ = 0;
		if (fees_.base_reserve_ < 0)
			fees_.base_reserve_ = 0;
		return true;
	}

	MonitorConfigure::MonitorConfigure() {
		center_ = "127.0.0.1:4053";
		disk_path_ = "/";
		enabled_ = false;
	}

	MonitorConfigure::~MonitorConfigure() {
	}

	bool MonitorConfigure::Load(const Json::Value &value) {
		Configure::GetValue(value, "id", id_);
		Configure::GetValue(value, "center", center_);
		Configure::GetValue(value, "disk_path", disk_path_);
		Configure::GetValue(value, "enabled", enabled_);
		return true;
	}

	Configure::Configure() {}

	Configure::~Configure() {}

	bool Configure::LoadFromJson(const Json::Value &values){
		if (!values.isMember("db") ||
			!values.isMember("logger") ||
			!values.isMember("p2p") ||
			!values.isMember("ledger") ||
			!values.isMember("genesis")) {
			LOG_STD_ERR("Some configuration not exist");
			return false;
		}

		db_configure_.Load(values["db"]);
		logger_configure_.Load(values["logger"]);
		p2p_configure_.Load(values["p2p"]);
		webserver_configure_.Load(values["webserver"]);
		ledger_configure_.Load(values["ledger"]);
		genesis_configure_.Load(values["genesis"]);
		wsserver_configure_.Load(values["wsserver"]);
		monitor_configure_.Load(values["monitor"]);
		return true;
	}
}