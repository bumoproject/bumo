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

#ifndef PEER_NETWORK_H_
#define PEER_NETWORK_H_

#include <utils/singleton.h>
#include <utils/net.h>
#include <common/general.h>
#include <common/private_key.h>
#include <common/network.h>
#include "peer.h"
#include "broadcast.h"

namespace bumo {

	class PeerNetwork :
		public Network,
		public TimerNotify,
		public IBroadcastDriver {
	public:
		PeerNetwork(const SslParameter &ssl_parameter_);
		~PeerNetwork();

	private:
		asio::ssl::context context_;

		bool dns_seed_inited_;
		bool cert_is_valid_;

		//Peer cache list
		protocol::Peers db_peer_cache_;

		//Peers infomation received
		utils::Mutex peer_lock_;
		std::list<utils::StringMap> received_peer_list_;

		Broadcast broadcast_;

		// cert is enable or unable
		bool cert_enabled_;

		std::string peer_node_address_;
		std::string node_rand_;
		int64_t network_id_;
		int32_t total_peers_count_;

		std::error_code last_ec_;
		int64_t last_update_peercache_time_;

		void Clean();

 		bool ResolveSeeds(const utils::StringList &address_list, int32_t rank);
		bool ConnectToPeers(size_t max);
		void CleanNotActivePeers();
		bool LoadSeed();
		bool LoadHardcode();

		bool ResetPeerInActive();
		bool CreatePeerIfNotExist(const utils::InetAddress &address);
		bool GetActivePeers(int32_t max);

		bool OnMethodHello(protocol::WsMessage &message, int64_t conn_id);
		bool OnMethodPeers(protocol::WsMessage &message, int64_t conn_id);
		bool OnMethodTransaction(protocol::WsMessage &message, int64_t conn_id);
		bool OnMethodGetLedgers(protocol::WsMessage &message, int64_t conn_id);
		bool OnMethodLedgers(protocol::WsMessage &message, int64_t conn_id);
		bool OnMethodPbft(protocol::WsMessage &message, int64_t conn_id);
		bool OnMethodLedgerUpNotify(protocol::WsMessage &message, int64_t conn_id);
		bool OnMethodHelloResponse(protocol::WsMessage &message, int64_t conn_id);

		//Operate the ip list
		int32_t QueryItem(const utils::InetAddress &address, protocol::Peers &records);
		bool UpdateItem(const utils::InetAddress &address, protocol::Peer &record);
		bool CheckPeerPoolValid(protocol::Peers &all);
		int32_t QueryTopItem(bool active, int64_t limit, int64_t next_attempt_time, protocol::Peers &records);
		bool UpdateItemDisconnect(const utils::InetAddress &address, int64_t conn_id);

		virtual void OnDisconnect(Connection *conn);
		virtual bool OnConnectOpen(Connection *conn);
		virtual Connection *CreateConnectObject(server *server_h, client *client_,
			tls_server *tls_server_h, tls_client *tls_client_h,
			connection_hdl con, const std::string &uri, int64_t id);
		virtual bool OnVerifyCallback(bool preverified, asio::ssl::verify_context& ctx);
		virtual bool OnValidate(websocketpp::connection_hdl hdl);

	public:
		bool Initialize(const std::string &node_address);
		bool Exit();

		void AddReceivedPeers(const utils::StringMap &item);
		void BroadcastMsg(int64_t type, const std::string &data);
		bool ReceiveBroadcastMsg(int64_t type, const std::string &data, int64_t peer_id);

		void GetPeers(Json::Value &peers);

		virtual void OnTimer(int64_t current_time) override;
		virtual void OnSlowTimer(int64_t current_time) override {};
		void GetModuleStatus(Json::Value &data);

		virtual bool SendMsgToPeer(int64_t peer_id, WsMessagePointer msg);
		virtual bool SendRequest(int64_t peer_id, int64_t type, const std::string &data);
		virtual std::set<int64_t> GetActivePeerIds();

		bool NodeExist(std::string node_address, int64_t peer_id);
	};
}

#endif
