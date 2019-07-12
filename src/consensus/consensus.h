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

#ifndef CONSENSUS_H_
#define CONSENSUS_H_

#include <utils/common.h>
#include <common/general.h>
#include <common/private_key.h>
#include <common/storage.h>
#include <proto/cpp/consensus.pb.h>
#include "consensus_msg.h"

namespace bumo {
	typedef std::function< void(bool check_result)> CheckValueFunction;
	class IConsensusNotify {
	public:
		IConsensusNotify() {};
		~IConsensusNotify() {};

		virtual std::string OnValueCommited(int64_t request_seq, const std::string &value, const std::string &proof, bool calculate_total) = 0;
		virtual void OnViewChanged(const std::string &last_consvalue) = 0;
		virtual int32_t CheckValue(const std::string &value) = 0;
		virtual void SendConsensusMessage(const std::string &message) = 0;
		virtual std::string FetchNullMsg() = 0;
		virtual void OnResetCloseTimer() = 0;
		virtual std::string DescConsensusValue(const std::string &request) = 0;
	};

	typedef std::map<std::string, int64_t> ValidatorMap;
	class Consensus {
	protected:
		std::string name_;

		bool is_validator_;
		PrivateKey private_key_;
		bool validation_random;
		int64_t replica_id_;
		std::map<std::string, int64_t> validators_;

		//Lock the instance
		utils::Mutex lock_;

		//Notify
		IConsensusNotify *notify_;

		int32_t CheckValue(const std::string &value);
		bool SendMessage(const std::string &message);
		std::string OnValueCommited(int64_t request_seq, const std::string &value, const std::string &proof, bool calculate_total);
		void OnViewChanged(const std::string &last_consvalue);
		
		//only called by derived class
		bool UpdateValidators(const protocol::ValidatorSet &validators);
	public:
		Consensus();
		~Consensus();

		enum CheckValueResult {
			CHECK_VALUE_VALID,
			CHECK_VALUE_MAYVALID,
			CHECK_VALUE_INVALID
		};

		virtual bool Initialize();
		virtual bool Exit();
		virtual bool Request(const std::string &value) { return true; };
		virtual bool OnRecv(const ConsensusMsg &message) { return true; };
		virtual size_t GetQuorumSize() { return 0; };

		virtual void OnTimer(int64_t current_time) {};
		virtual void OnSlowTimer(int64_t current_time) {};
		virtual void GetModuleStatus(Json::Value &data) {};
		virtual void OnTxTimeout() {};
		virtual bool CheckProof(const protocol::ValidatorSet &validators, const std::string &previous_value_hash, const std::string &proof) { return true; };
		virtual bool UpdateValidators(const protocol::ValidatorSet &validators, const std::string &proof) { return true; };

		static int32_t CompareValue(const std::string &value1, const std::string &value2);

		static bool SaveValue(const std::string &name, const std::string &value);
		static bool SaveValue(const std::string &name, int64_t value);
		static int32_t LoadValue(const std::string &name, std::string &value);
		static int32_t LoadValue(const std::string &name, int64_t &value);
		static bool DelValue(const std::string &name);
		void SetNotify(IConsensusNotify *notify);

		bool IsValidator();
		protocol::Signature SignData(const std::string &data);
		virtual int32_t IsLeader() { return -1; };
		std::string GetNodeAddress();
		int64_t GetValidatorIndex(const std::string &node_address) const;
		static int64_t GetValidatorIndex(const std::string &node_address, const ValidatorMap &validators);
		std::string DescRequest(const std::string &value);
		bool GetValidation(protocol::ValidatorSet &validators, size_t &quorum_size);
	};

	class ValueSaver {
	public:
		ValueSaver();
		~ValueSaver();

		size_t write_size;
		WRITE_BATCH writes;

		void SaveValue(const std::string &name, const std::string &value);
		void SaveValue(const std::string &name, int64_t value);
		void DelValue(const std::string &name);
		bool Commit();
	};

	class OneNode : public Consensus {
	public:
		OneNode();
		~OneNode();

		virtual bool Request(const std::string &value);
		virtual void GetModuleStatus(Json::Value &data);
	};

}

#endif
