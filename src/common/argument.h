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

#ifndef ARGUMENT_H_
#define ARGUMENT_H_

#include "storage.h"

namespace bumo {
	class Argument {
	public:
		Argument();
		~Argument();

		bool help_modle_;
		bool drop_db_;
		int32_t log_dest_;
		bool console_;

		bool peer_addr_;
		bool clear_peer_addresses_;
		bool clear_consensus_status_;
		bool create_hardfork_;

		bool Parse(int argc, char *argv[]);
		void Usage();
		void ShowHardwareAddress();
		void ShowNodeId(int argc, char *argv[]);
		void RequestCert(int argc, char *argv[]);
		void ShowRequest(int argc, char *argv[]);
		bool SignDataWithKList(int argc, char *argv[]);
	};

	extern bool g_enable_;
	extern bool g_ready_;

	void InstallSignal();
}
#endif