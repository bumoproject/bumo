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

#include <utils/crypto.h>
#include <utils/logger.h>
#include <common/storage.h>
#include <main/configure.h>
#include "consensus.h"

namespace bumo {
	Consensus::Consensus() : name_("consensus"),
		notify_(NULL),
		is_validator_(false),
		replica_id_(-1),
		private_key_(Configure::Instance().ledger_configure_.validation_privatekey_),
		validation_random(Configure::Instance().ledger_configure_.validation_random) {}

	Consensus::~Consensus() {}

	bool Consensus::Initialize() {
		if (!private_key_.IsValid()) {
			LOG_ERROR("The format of consensus private key is error.");
			return false;
		}

		return true;

// 		const ValidationConfigure &config = Configure::Instance().validation_configure_;
// 		int64_t counter = 0;
// 		protocol::ValidatorSet proto_validators;
// 		for (auto const &iter : config.validators_) {
// 			proto_validators.add_validator(iter);
// 		}
// 
// 		return UpdateValidators(proto_validators);
	}

	bool Consensus::Exit() {
		return true;
	}

	bool Consensus::UpdateValidators(const protocol::ValidatorSet &validators) {
		validators_.clear();

		is_validator_ = false;
        std::string node_address = private_key_.GetEncAddress();
		int64_t counter = 0;
		for (int32_t i = 0; i < validators.validators_size(); i++) {
			validators_.insert(std::make_pair(validators.validators(i).address(), counter++));
			if (node_address == validators.validators(i).address()) {
				is_validator_ = true;
			}
		}

		if (is_validator_) {
			std::map<std::string, int64_t>::const_iterator iter = validators_.find(node_address);
			replica_id_ = iter->second;
		}
		else {
			replica_id_ = -1;
		}

		return true;
	}

	bool Consensus::GetValidation(protocol::ValidatorSet &validators, size_t &quorum_size) {
		std::vector<std::string> vec_validators;
		vec_validators.resize(validators_.size());
		for (std::map<std::string, int64_t>::iterator iter = validators_.begin();
			iter != validators_.end();
			iter++) {
			vec_validators[(uint32_t)iter->second] = iter->first;
		}

		for (size_t i = 0; i < vec_validators.size(); i++) {
			auto validator = validators.add_validators();
			validator->set_address(vec_validators[i]);
			validator->set_pledge_coin_amount(0);
		}

		quorum_size = GetQuorumSize();
		return true;
	}

	protocol::Signature Consensus::SignData(const std::string &data) {
		protocol::Signature sig;
		sig.set_sign_data(private_key_.Sign(data));
        sig.set_public_key(private_key_.GetEncPublicKey());
		return sig;
	}

	bool Consensus::SendMessage(const std::string &message) {
		if (!is_validator_) {
			return true;
		}

		notify_->SendConsensusMessage(message);
		return true;
	}

	int64_t Consensus::GetValidatorIndex(const std::string &node_address) const {
		return GetValidatorIndex(node_address, validators_);
	}

	int64_t Consensus::GetValidatorIndex(const std::string &node_address, const ValidatorMap &validators) {
		std::map<std::string, int64_t>::const_iterator iter = validators.find(node_address);
		if (iter != validators.end()) {
			return iter->second;
		}

		return -1;
	}

	std::string Consensus::DescRequest(const std::string &value) {
		return notify_->DescConsensusValue(value);
	}

	std::string Consensus::OnValueCommited(int64_t request_seq, const std::string &value, const std::string &proof, bool calculate_total) {
		return notify_->OnValueCommited(request_seq, value, proof, calculate_total);
	}

	void Consensus::OnViewChanged(const std::string &last_consvalue) {
		notify_->OnViewChanged(last_consvalue);
	}

	int32_t Consensus::CheckValue(const std::string &value) {
		return notify_->CheckValue(value);
	}

	int32_t Consensus::CompareValue(const std::string &value1, const std::string &value2) {
		return value1.compare(value2);
	}

	bool Consensus::IsValidator() {
		return is_validator_;
	}

	std::string Consensus::GetNodeAddress() {
        return private_key_.GetEncAddress();
	}

	bool Consensus::SaveValue(const std::string &name, const std::string &value) {
		KeyValueDb *db = Storage::Instance().keyvalue_db();
		return db->Put(utils::String::Format("%s_%s", bumo::General::CONSENSUS_PREFIX, name.c_str()), value);
	}

	bool Consensus::SaveValue(const std::string &name, int64_t value) {
		LOG_INFO("Set %s, value = " FMT_I64 ".", name.c_str(), value);
		return SaveValue(name, utils::String::ToString(value));
	}

	int32_t Consensus::LoadValue(const std::string &name, std::string &value) {
		KeyValueDb *db = Storage::Instance().keyvalue_db();
		return db->Get(utils::String::Format("%s_%s", bumo::General::CONSENSUS_PREFIX, name.c_str()), value) ? 1 : 0;
	}

	bool Consensus::DelValue(const std::string &name) {
		KeyValueDb *db = Storage::Instance().keyvalue_db();
		return db->Delete(utils::String::Format("%s_%s", bumo::General::CONSENSUS_PREFIX, name.c_str())) ? 1 : 0;
	}

	int32_t Consensus::LoadValue(const std::string &name, int64_t &value) {
		std::string strvalue;
		int32_t ret = LoadValue(name, strvalue);
		if (ret > 0) value = utils::String::Stoi64(strvalue);
		return ret;
	}

	void Consensus::SetNotify(IConsensusNotify *notify) {
		notify_ = notify;
	}

	OneNode::OneNode() {
		name_ = "one_node";
	}

	OneNode::~OneNode() {}

	bool OneNode::Request(const std::string &value) {
		OnValueCommited(0, value, "", true);
		return true;
	}

	void OneNode::GetModuleStatus(Json::Value &data) {
		data["type"] = name_;
	}

	ValueSaver::ValueSaver() :write_size(0) {};
	ValueSaver::~ValueSaver() {
		Commit();
	};

	void ValueSaver::SaveValue(const std::string &name, const std::string &value) {
		writes.Put(utils::String::Format("%s_%s", bumo::General::CONSENSUS_PREFIX, name.c_str()), value);
		write_size++;
		LOG_TRACE("Set %s, value size =" FMT_SIZE ".", name.c_str(), value.size());
	}

	void ValueSaver::SaveValue(const std::string &name, int64_t value) {
		LOG_TRACE("Set %s, value = " FMT_I64 ".", name.c_str(), value);
		SaveValue(name, utils::String::ToString(value));
	}

	void ValueSaver::DelValue(const std::string &name) {
		writes.Delete(name);
		write_size++;
	}

	bool ValueSaver::Commit() {
		KeyValueDb *db = Storage::Instance().keyvalue_db();
		bool ret = true;
		if (write_size > 0) {
			ret = db->WriteBatch(writes);
			write_size = 0;
		}

		return true;
	}
}