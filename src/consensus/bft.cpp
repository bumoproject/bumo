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

#include <utils/headers.h>
#include <common/pb2json.h>
#include "proto/cpp/chain.pb.h"
#include "bft.h"

namespace bumo {
	Pbft::Pbft() :view_number_(0),
		last_exe_seq_(1),
		fault_number_(0),
		view_active_(true),
		new_view_repond_timer_(0),
		last_check_time_(utils::Timestamp::HighResolution()) {
		name_ = "pbft";

		//Load from the configuration.
		ckp_interval_ = 10;
	}

	Pbft::~Pbft() {

	}

	bool Pbft::Initialize() {
		if (!Consensus::Initialize()) {
			return false;
		}

		LoadValues();

		//sequence_ = last_exe_seq_ + 1;
		// 0 : 1~5     5:6~10      10 : 11~15
		//low_water_mark_ = (sequence_ - 1) / ckp_count_ * ckp_count_;

		return true;
	}

	bool Pbft::Exit() {
		return true;
	}

	void Pbft::LoadValues() {
		int64_t active = 1;
		LoadValue(PbftDesc::VIEW_ACTIVE, active);
		view_active_ = active > 0;

		LoadValue(PbftDesc::VIEWNUMBER_NAME, view_number_);
		LoadVcInstance();
	}

	void Pbft::ClearStatus() {
		DelValue(PbftDesc::VIEW_ACTIVE);
		DelValue(PbftDesc::VIEWNUMBER_NAME);
		DelValue(PbftDesc::VIEW_CHANGE_NAME);
	}

	int32_t Pbft::LoadVcInstance() {
		do {
			std::string str_instance;
			int32_t ret = LoadValue(PbftDesc::VIEW_CHANGE_NAME, str_instance);
			if (ret <= 0) {
				LOG_INFO("Failed to load view change instances.");
				return ret;
			}
			else if (ret == 0) {
				return ret;
			}

			Json::Value json_instance;
			if (!json_instance.fromString(str_instance)) {
				LOG_ERROR("Failed to parse loaded instances, and string instances is: %s.", str_instance.c_str());
				return -1;
			}

			for (uint32_t i = 0; i < json_instance.size(); i++) {
				const Json::Value &item = json_instance[i];

				int64_t index = item["sequence"].asInt64();
				PbftVcInstance &instance = vc_instances_[index];

				instance.view_number_ = item["view_number"].asInt64();
				instance.view_change_round_ = item["view_change_round"].asUInt();
				instance.start_time_ = item["start_time"].asInt64();
				instance.end_time_ = item["end_time"].asInt64();
				instance.last_propose_time_ = item["last_propose_time"].asInt64();
				instance.last_newview_time_ = item["last_newview_time"].asInt64();
				instance.new_view_round_ = item["new_view_round"].asUInt();

				if (!instance.view_change_msg_.ParseFromString(utils::String::HexStringToBin(item["view_change_msg"].asString()))) {
					LOG_ERROR("Failed to parse view change message string when Consensus load instance.");
				}

				if (!instance.newview_.ParseFromString(utils::String::HexStringToBin(item["new_view"].asString()))) {
					LOG_ERROR("Failed to parse new view message string when Consensus load instance.");
				}

				//For message buffer
				const Json::Value &msg_buffer_json = item["msg_buffer"];
				for (uint32_t m = 0; m < msg_buffer_json.size(); m++) {
					PbftPhaseVector pv;
					const Json::Value &msg_item_json = msg_buffer_json[m];
					protocol::PbftEnv env;
					if (!env.ParseFromString(utils::String::HexStringToBin(msg_item_json.asString()))) {
						LOG_ERROR("Failed to parse message buffer string when Consensus load view-change.");
						continue;
					}
					instance.msg_buf_.push_back(env);
				}

				//For view changes message
				const Json::Value &view_changes_json = item["view_changes"];
				for (uint32_t m = 0; m < view_changes_json.size(); m++) {
					protocol::PbftViewChange env;
					if (!env.ParseFromString(utils::String::HexStringToBin(view_changes_json[m].asString()))) {
						LOG_ERROR("Failed to parse view-change string Consensus load view-change.");
						continue;

					}
					instance.viewchanges_.insert(std::make_pair(env.replica_id(), env));

				}
			}
		} while (false);

		return 1;
	}

	int32_t Pbft::LoadValidators() {
		do {
			std::string str_validators;
			int32_t ret = LoadValue(PbftDesc::VALIDATORS, str_validators);
			if (ret <= 0) {
				LOG_INFO("Failed to load validators.");
				return ret;
			}
			else if (ret == 0) {
				return ret;
			}

			Json::Value json_instance;
			if (!json_instance.fromString(str_validators)) {
				LOG_ERROR("Failed to parse loaded validators, and the validators string instances is %s.", str_validators.c_str());
				return -1;
			}

			int64_t counter = 0;
			protocol::ValidatorSet proto_validators;
			for (uint32_t i = 0; i < json_instance.size(); i++) {
				auto validator = proto_validators.add_validators();
				validator->set_address(json_instance[i].asString());
				validator->set_pledge_coin_amount(0);
			}

			Consensus::UpdateValidators(proto_validators);
		} while (false);

		return 1;
	}

	bool Pbft::SaveValidators(ValueSaver &saver) {
		Json::Value total = Json::Value(Json::arrayValue);
		std::vector<std::string> vec_validators;
		vec_validators.resize(validators_.size());
		for (auto iter = validators_.begin(); iter != validators_.end(); iter++) {
			vec_validators[(uint32_t)iter->second] = iter->first;
		}

		for (size_t i = 0; i < vec_validators.size(); i++) {
			total[total.size()] = vec_validators[i];
		}

		saver.SaveValue(PbftDesc::VALIDATORS, total.toFastString());
		return true;
	}

	bool Pbft::SaveViewChange(ValueSaver &saver) {
		Json::Value total = Json::Value(Json::arrayValue);
		for (PbftVcInstanceMap::const_iterator iter = vc_instances_.begin(); iter != vc_instances_.end(); iter++) {
			int64_t index = iter->first;
			const PbftVcInstance &instance = iter->second;
			Json::Value &item = total[total.size()];

			item["sequence"] = index;
			item["view_number"] = instance.view_number_;

			//For tags
			item["view_change_round"] = instance.view_change_round_;
			item["start_time"] = instance.start_time_;
			item["end_time"] = instance.end_time_;
			item["last_propose_time"] = instance.last_propose_time_;

			item["last_newview_time"] = instance.last_newview_time_;
			item["new_view_round"] = instance.new_view_round_;
			item["view_change_msg"] = utils::String::BinToHexString(instance.view_change_msg_.SerializeAsString());
			item["new_view"] = utils::String::BinToHexString(instance.newview_.SerializeAsString());

			//For message buffer
			Json::Value &msg_buffer_json = item["msg_buffer"];
			for (PbftPhaseVector::const_iterator iter_msg = instance.msg_buf_.begin(); iter_msg != instance.msg_buf_.end(); iter_msg++) {
				msg_buffer_json[msg_buffer_json.size()] = utils::String::BinToHexString(iter_msg->SerializeAsString());
			}

			//For view changes
			Json::Value &view_changes = item["view_changes"];
			for (PbftViewChangeMap::const_iterator iter1 = instance.viewchanges_.begin(); iter1 != instance.viewchanges_.end(); iter1++) {
				view_changes[view_changes.size()] = utils::String::BinToHexString(iter1->second.SerializeAsString());
			}
		}

		saver.SaveValue(PbftDesc::VIEW_CHANGE_NAME, total.toFastString());

		return true;
	}

	bool Pbft::SendMessage(const PbftEnvPointer &msg) {
		return Consensus::SendMessage(msg->SerializeAsString());
	}

	void Pbft::OnTimer(int64_t current_time) {
		utils::MutexGuard guard(lock_);

		PbftInstance *last_prepared_instance = NULL;
		const PbftInstanceIndex *index = NULL;
		PbftInstanceMap::iterator last_prepared_iter = instances_.end();
		for (PbftInstanceMap::iterator iter = instances_.begin(); iter != instances_.end(); iter++) {

			//Check if it times out
			if (iter->second.IsExpire(current_time) &&
				!iter->second.have_send_viewchange_
				) {
				LOG_INFO("Pbft instance timeout, view-number(" FMT_I64 "), sequence(" FMT_I64 "), phase(%d)",
					iter->first.view_number_, iter->first.sequence_, iter->second.phase_);
				OnTxTimeout();
				iter->second.have_send_viewchange_ = true;
			}

			//Check if we should send the prepare again
			if (iter->second.NeedSendAgain(current_time) &&
				view_active_ &&
				iter->second.pre_prepare_msg_.has_pbft()) {
				iter->second.SendPrepareAgain(this, current_time);
				LOG_INFO("Send pre-prepare message again actively: view-number(" FMT_I64 "), sequence(" FMT_I64 "), round number(%u)",
					iter->first.view_number_, iter->first.sequence_, iter->second.pre_prepare_round_);
			}

			if (iter->second.phase_ >= PBFT_PHASE_PREPARED) {
				last_prepared_instance = &iter->second;
				index = &iter->first;
			}
		}

		if (last_prepared_instance != NULL &&
			last_prepared_instance->check_value_result_ == Consensus::CHECK_VALUE_VALID &&
			last_prepared_instance->NeedSendCommitAgain(current_time)) { //For broadcast only
			PbftEnvPointer commit_msg = NewCommit(last_prepared_instance->prepares_.begin()->second, ++last_prepared_instance->commit_round_);
			SendMessage(commit_msg);
			last_prepared_instance->SetLastCommitSendTime(current_time);
			LOG_INFO("Send commit message again actively: view-number(" FMT_I64 "), sequence(" FMT_I64 "), round number(%u)",
				index->view_number_, index->sequence_, last_prepared_instance->commit_round_);
		}

		//Check if the 'view change' times out, and get the last 'new view' just sent.
		PbftVcInstance *lastvc_instance = NULL;
		for (PbftVcInstanceMap::iterator iter_vc = vc_instances_.begin(); iter_vc != vc_instances_.end(); iter_vc++) {
			if (iter_vc->second.NeedSendAgain(current_time) && iter_vc->second.view_change_msg_.has_pbft()) {
				PbftEnvPointer new_ptr = IncPeerMessageRound(iter_vc->second.view_change_msg_, ++iter_vc->second.view_change_round_);
				SendMessage(new_ptr);
				iter_vc->second.SetLastProposeTime(current_time);
				LOG_INFO("Send view-change message again actively: view-number(" FMT_I64 "), round number(%u)",
					iter_vc->first, iter_vc->second.view_change_round_);
			}

			if (iter_vc->second.NeedSendNewViewAgain(current_time) &&
				iter_vc->second.view_number_ % validators_.size() == replica_id_) {
				lastvc_instance = &iter_vc->second;
			}
		}

		if (lastvc_instance != NULL) {
			lastvc_instance->SendNewViewAgain(this, current_time);
			LOG_INFO("Send new view message again actively: view number(" FMT_I64 "), round number(%u)",
				lastvc_instance->view_number_, lastvc_instance->new_view_round_);
		}
	}

	bool Pbft::InWaterMark(int64_t seq) {
		return seq >= last_exe_seq_  && seq <= last_exe_seq_ + ckp_interval_;
	}

	bool Pbft::Request(const std::string &value) {
		if (view_number_ % validators_.size() != replica_id_) {
			return false;
		}

		LOG_INFO("Start to request value(%s)", notify_->DescConsensusValue(value).c_str());

		if (!view_active_) {
			LOG_INFO("The view(view-number:" FMT_I64 ") is not active, so request failed.", view_number_);
			return false;
		}

		//Lock the instances
		utils::MutexGuard lock_guad(lock_);

		//Delete the last uncommitted logs
		for (PbftInstanceMap::iterator iter_inst = instances_.begin();
			iter_inst != instances_.end();
			) {
			if (iter_inst->first.sequence_ > last_exe_seq_ && iter_inst->second.phase_ < PBFT_PHASE_COMMITED) {
				LOG_INFO("Before request, erasing the uncommitted log, sequence(" FMT_I64 ")", iter_inst->first.sequence_);
				instances_.erase(iter_inst++);
			}
			else {
				iter_inst++;
			}
		}

		int64_t sequence = last_exe_seq_ + 1;
		PbftEnvPointer env = NewPrePrepare(value, sequence);

		//Check the index
		PbftInstanceIndex index(view_number_, sequence);

		//Insert the instance to map
		PbftInstance pinstance;
		pinstance.pre_prepare_msg_ = *env;
		pinstance.phase_ = PBFT_PHASE_PREPREPARED;
		pinstance.pre_prepare_ = env->pbft().pre_prepare();
		pinstance.msg_buf_[env->pbft().type()].push_back(*env);
		instances_[index] = pinstance;

		LOG_INFO("Send pre-prepare message: view number(" FMT_I64 "), sequence(" FMT_I64 "), consensus value(%s)", 
			view_number_, index.sequence_, notify_->DescConsensusValue(value).c_str());
		//Broadcast the message to other nodes
		return SendMessage(env);
	}

	protocol::PbftMessageType Pbft::GetMessageType(const protocol::PbftEnv &env) {
		const protocol::Pbft &pbft = env.pbft();
		return pbft.type();
	}

	bool Pbft::CheckMessageItem(const protocol::PbftEnv &env) {
		return CheckMessageItem(env, validators_);
	}

	bool Pbft::CheckMessageItem(const protocol::PbftEnv &env, const ValidatorMap &validators) {
		//This function should output the error log
		const protocol::Pbft &pbft = env.pbft();
		const protocol::Signature &sig = env.signature();

		if (pbft.chain_id() != General::GetSelfChainId()){
			LOG_TRACE("Failed to check same chain, node self id(" FMT_I64 ") is not eq (" FMT_I64 ")",
				General::GetSelfChainId(), pbft.chain_id());
			return false;
		}

		//Get the node address
		PublicKey public_key(sig.public_key());

		//Check the node id to see if it exists in the validator' list
		int64_t should_replica_id = GetValidatorIndex(public_key.GetEncAddress(), validators);
		if (should_replica_id < 0) {
            LOG_ERROR("Unable to find validator(%s) from list", public_key.GetEncAddress().c_str());
			return false;
		}

		//Check pbft type is no larger than max
		int64_t replica_id = -1;
		switch (pbft.type()) {
		case protocol::PBFT_TYPE_PREPREPARE:
		{
			if (!pbft.has_pre_prepare()) {
				LOG_ERROR("Pre-Prepare message has no instance.");
				return false;
			}
			replica_id = pbft.pre_prepare().replica_id();
			break;
		}
		case protocol::PBFT_TYPE_PREPARE:
		{
			if (!pbft.has_prepare()) {
				LOG_ERROR("Prepare message has no instance.");
				return false;
			}
			replica_id = pbft.prepare().replica_id();
			break;
		}
		case protocol::PBFT_TYPE_COMMIT:
		{
			if (!pbft.has_commit()) {
				LOG_ERROR("Commit message has no instance");
				return false;
			}
			replica_id = pbft.commit().replica_id();
			break;
		}
		case protocol::PBFT_TYPE_VIEWCHANGE:
		{
			if (!pbft.has_view_change()) {
				LOG_ERROR("View change message has no instance");
				return false;
			}

			replica_id = pbft.view_change().replica_id();
			break;
		}
		case protocol::PBFT_TYPE_VIEWCHANG_WITH_RAWVALUE:
		{
			if (!pbft.has_view_change_with_rawvalue()) {
				LOG_ERROR("View change with raw value message has no instance");
				return false;
			}

			if (!CheckViewChangeWithRawValue(pbft.view_change_with_rawvalue(), validators)) {
				return false;
			}

			const protocol::PbftEnv &view_change_raw = pbft.view_change_with_rawvalue().view_change_env();
			replica_id = view_change_raw.pbft().view_change().replica_id();
			break;
		}
		case protocol::PBFT_TYPE_NEWVIEW:
		{
			if (!pbft.has_new_view()) {
				LOG_ERROR("New view message has no instance");
				return false;
			}
			replica_id = pbft.new_view().replica_id();
			break;
		}
		default:
		{
			LOG_ERROR("Unable to parse the type(%d)", pbft.type());
			return false;
		}
		}
		//Check if replica_id is equal to the object id
		if (replica_id != should_replica_id) {
			LOG_ERROR("Failed to check the received message(type:%s), because the message replica id(" FMT_I64 ") is not equal to the signature id(" FMT_I64")",
				PbftDesc::GetPbft(pbft).c_str(), replica_id, should_replica_id);
			return false;
		}

		//Check the signature
		if (!PublicKey::Verify(pbft.SerializeAsString(), sig.sign_data(), sig.public_key())) {
			LOG_ERROR("Failed to check received message's signature, desc(%s)", PbftDesc::GetPbft(pbft).c_str());
			return false;
		}
		return true;
	}

	bool Pbft::CheckViewChangeWithRawValue(const protocol::PbftViewChangeWithRawValue &view_change_raw, const ValidatorMap &validators) {

		if (!view_change_raw.has_view_change_env()) {
			LOG_ERROR("Failed to check raw view-change, thers is no view change env, desc(%s)", PbftDesc::GetViewChangeRawValue(view_change_raw).c_str());
			return false;
		}

		if (GetMessageType(view_change_raw.view_change_env()) != protocol::PBFT_TYPE_VIEWCHANGE ||
			!CheckMessageItem(view_change_raw.view_change_env(), validators)) {
			LOG_ERROR("Failed to check raw view-change, desc(%s)", PbftDesc::GetViewChangeRawValue(view_change_raw).c_str());
			return false;
		}
		std::string p_value_digest = view_change_raw.view_change_env().pbft().view_change().prepred_value_digest();

		std::string value_digest;
		if (view_change_raw.has_prepared_set() ) {
			bool error_ret = false;
			//Check the prepared message
			const protocol::PbftPreparedSet &prepared_set = view_change_raw.prepared_set();
			//Check the pre-prepared message
			const protocol::PbftEnv &pre_prepare_env = prepared_set.pre_prepare();
			const protocol::PbftPrePrepare &pre_prepare = pre_prepare_env.pbft().pre_prepare();
			if (!CheckMessageItem(pre_prepare_env, validators)) {
				return false;
			}
			value_digest = pre_prepare.value_digest();

			std::set<int64_t> replica_ids;
			//Check the prepared message
			for (int32_t m = 0; m < prepared_set.prepare_size(); m++) {
				const protocol::PbftEnv &prepare_env = prepared_set.prepare(m);
				if (!CheckMessageItem(prepare_env, validators)) {
					LOG_ERROR("Failed to check view-change prepared set, desc(%s)", PbftDesc::GetViewChangeRawValue(view_change_raw).c_str());
					return false;
				}

				const protocol::PbftPrepare &prepare = prepare_env.pbft().prepare();

				if (prepare.view_number() != pre_prepare.view_number() ||
					prepare.sequence() != pre_prepare.sequence() ||
					CompareValue(prepare.value_digest(), pre_prepare.value_digest()) != 0) {
					LOG_ERROR("Failed to check view-change prepared set, desc(%s)", PbftDesc::GetViewChangeRawValue(view_change_raw).c_str());
					return false;
				}

				replica_ids.insert(prepare.replica_id());
			}

			if (replica_ids.size() < GetQuorumSize(validators.size())) {
				LOG_ERROR("The raw-view-change message's prepared message's replica number(" FMT_SIZE ") is less than quorom size(" FMT_SIZE")",
					 replica_ids.size(), GetQuorumSize(validators.size()) + 1);
				return false;
			}
		} 

		if (p_value_digest != value_digest) {
			LOG_ERROR("Failed to check view-change, because inner value digest is difference, desc(%s)", PbftDesc::GetViewChangeRawValue(view_change_raw).c_str());
			return false;
		} 

		return true;
	}

	bool Pbft::TraceOutPbftPrePrepare(const protocol::PbftEnv &env) {
		const protocol::Pbft &pbft = env.pbft();
		const protocol::PbftPrePrepare &pre_pre = pbft.pre_prepare();
		PbftInstanceIndex index(pre_pre.view_number(), pre_pre.sequence());
		PbftInstanceMap::iterator iter = out_pbft_instances_.find(index);

		PbftInstance &instance = out_pbft_instances_[index];
		//if (iter == out_pbft_instances_.end()){
		//	PbftInstance instance;
		*instance.pre_prepare_.mutable_value() = pre_pre.value();
		instance.pre_prepare_.set_value_digest(pre_pre.value_digest());
		//	out_pbft_instances_.insert(std::make_pair(index, instance));
		//}

		LOG_INFO("Received trace out pre-prepare message from replica(" FMT_I64 "):  view number(" FMT_I64 "), sequence(" FMT_I64 "), round number(%u)",
			pre_pre.replica_id(), pre_pre.view_number(), pre_pre.sequence(), pbft.round_number());
		TryDoTraceOut(index, instance);
		return true;
	}

	bool Pbft::TraceOutPbftCommit(const protocol::PbftEnv &env) {
		const protocol::Pbft &pbft = env.pbft();
		const protocol::PbftCommit &commit = pbft.commit();
		PbftInstanceIndex index(commit.view_number(), commit.sequence());
		//Check if it exists in normal object
		PbftInstanceMap::iterator iter_normal = instances_.find(index);
		if (iter_normal != instances_.end()) {
			LOG_INFO("Received trace out but normal commit message from replica(" FMT_I64 "): view number(" FMT_I64 "), sequence(" FMT_I64 "), round number(%u)",
				commit.replica_id(), commit.view_number(), commit.sequence(), pbft.round_number());
			return OnCommit(pbft, iter_normal->second);
		}

		PbftInstanceMap::iterator iter = out_pbft_instances_.find(index);
		if (iter == out_pbft_instances_.end()) {
			PbftInstance instance;
			instance.pre_prepare_.set_value_digest(commit.value_digest());
			out_pbft_instances_.insert(std::make_pair(index, instance));
		}

		PbftInstance &instance_exist = out_pbft_instances_[index];
		if (0 != CompareValue(instance_exist.pre_prepare_.value_digest(), commit.value_digest())) {
			LOG_ERROR("The commit message (view number:" FMT_I64 ", sequence:" FMT_I64 ") is not equal to pre-commit message",
				commit.view_number(), commit.sequence());
			return false;
		}

		LOG_INFO("Received trace out commit message from replica(" FMT_I64 "): view number(" FMT_I64 "), sequence(" FMT_I64 "), round number(%u)",
			commit.replica_id(), commit.view_number(), commit.sequence(), pbft.round_number());
		instance_exist.commits_.insert(std::make_pair(commit.replica_id(), commit));
		TryDoTraceOut(index, instance_exist);

		return true;
	}

	void Pbft::TryDoTraceOut(const PbftInstanceIndex &index, const PbftInstance &instance) {
		if (instance.commits_.size() >= GetQuorumSize() + 1 /*&& instance.pre_prepare_.has_value()*/) {
			LOG_INFO("commited trace out pbft, vn(" FMT_I64 "), seq(" FMT_I64 ")", index.view_number_, index.sequence_);
			if (index.sequence_ - last_exe_seq_ >= ckp_interval_) {
				LOG_INFO("The trace out pbft's sequence(" FMT_I64 ") is larger than the last execution sequence(" FMT_I64 ") for checkpoint interval(" FMT_I64 "),then try to move watermark.",
					index.sequence_, last_exe_seq_, ckp_interval_);

				//We should move to the new watermark
				view_active_ = true;
				view_number_ = index.view_number_;
				last_exe_seq_ = index.sequence_;

				ValueSaver saver;
			//	saver.SaveValue(PbftDesc::LOW_WATER_MRAK_NAME, low_water_mark_);
				saver.SaveValue(PbftDesc::VIEW_ACTIVE, view_active_? 1 : 0);

				//Clear the view change instance
				for (PbftVcInstanceMap::iterator iter_vc = vc_instances_.begin(); iter_vc != vc_instances_.end();) {
					if (iter_vc->second.view_change_msg_.has_pbft()) {
						vc_instances_.erase(iter_vc++);
					}
					else {
						iter_vc++;
					}
				}
				SaveViewChange(saver);

				//Delete instance
				for (PbftInstanceMap::iterator iter = instances_.begin(); iter != instances_.end();) {
					const PbftInstanceIndex &index = iter->first;
					if (index.sequence_ <= last_exe_seq_) instances_.erase(iter++);
					else iter++;
				}
				//SaveInstance(saver);

				//Clear the Out pbft instance
				out_pbft_instances_.clear();

				notify_->OnResetCloseTimer();
			}
		}
	}

	PbftInstance *Pbft::CreateInstanceIfNotExist(const protocol::PbftEnv &env) {
		const protocol::Pbft &pbft = env.pbft();
		int64_t view_number = 0;
		int64_t sequence = 0;
		switch (pbft.type()) {
		case protocol::PBFT_TYPE_PREPREPARE:
		{
			view_number = pbft.pre_prepare().view_number();
			sequence = pbft.pre_prepare().sequence();
			break;
		}
		case protocol::PBFT_TYPE_PREPARE:{

			view_number = pbft.prepare().view_number();
			sequence = pbft.prepare().sequence();
			break;
		}
		case protocol::PBFT_TYPE_COMMIT:{

			view_number = pbft.commit().view_number();
			sequence = pbft.commit().sequence();
			break;
		}
		default:{
			return NULL;
		}
		}

		bool in_water = InWaterMark(sequence);
		bool same_view = view_number_ == view_number;
		if (!in_water || !same_view) {

			if (!in_water) LOG_TRACE("The message(type: %s, sequence:" FMT_I64 ") is not in water mark(" FMT_I64 ", " FMT_I64"), desc(%s)", PbftDesc::GetMessageTypeDesc(pbft.type()),
				sequence, last_exe_seq_, last_exe_seq_ + ckp_interval_, PbftDesc::GetPbft(pbft).c_str());
			if (!same_view)	LOG_TRACE("The message(type:%s)'s view number(" FMT_I64 ") != this view number(" FMT_I64 "), desc(%s)", PbftDesc::GetMessageTypeDesc(pbft.type()),
				view_number, view_number_, PbftDesc::GetPbft(pbft).c_str());
			if (sequence > last_exe_seq_) {
				if (pbft.type() == protocol::PBFT_TYPE_COMMIT) {
					TraceOutPbftCommit(env);
				}
			}
			return NULL;
		}

		if (!view_active_) {
			LOG_INFO("The message(type:%s, sequence " FMT_I64 ") would not be processed when view is not active, desc(%s)", PbftDesc::GetMessageTypeDesc(pbft.type()),
				sequence, PbftDesc::GetPbft(pbft).c_str());
			return NULL;
		}


		if (sequence <= last_exe_seq_) {
			LOG_TRACE("Pbft current sequence(" FMT_I64 ") <= last sequence(" FMT_I64 "), then don't create instance.", sequence, last_exe_seq_);
			return NULL;
		}

		PbftInstanceIndex index(view_number, sequence);
		PbftInstanceMap::iterator iter_find = instances_.find(index);
		if (iter_find == instances_.end()) {
			PbftInstanceIndex index(view_number, sequence);
			LOG_INFO("Create pbft instance(view-number:" FMT_I64 ", sequence:" FMT_I64 ")", view_number, sequence);
			instances_.insert(std::make_pair(index, PbftInstance()));

			//Delete the seq which is not the same view
			for (PbftInstanceMap::iterator iter_inst = instances_.begin();
				iter_inst != instances_.end();
				) {
				if (iter_inst->first.view_number_ < view_number &&
					iter_inst->first.sequence_ == sequence) {
					iter_inst = instances_.erase(iter_inst);
				}
				else {
					iter_inst++;
				}
			}
		}

		PbftInstance &instace = instances_[index];
		instace.msg_buf_[pbft.type()].push_back(env);

		return &instace;
	}

	bool Pbft::OnRecv(const ConsensusMsg &message) {
		if (message.GetType() != name_) {
			LOG_ERROR("The received consensus message may be error, and its type is not pbft");
			return false;
		}
		protocol::PbftEnv env = message.GetPbft();
		const protocol::Pbft &pbft = env.pbft();
		const protocol::Signature &sig = env.signature();

		//Check the message item. 
		if (!CheckMessageItem(env)) {
			return false;
		}

		bool doret = false;
		switch (pbft.type()) {
		case protocol::PBFT_TYPE_PREPREPARE:
		case protocol::PBFT_TYPE_PREPARE:
		case protocol::PBFT_TYPE_COMMIT:{
			//The current view must be active.
			int32_t ret = Consensus::CHECK_VALUE_VALID;
			if (pbft.type() == protocol::PBFT_TYPE_PREPREPARE) {
				const protocol::PbftPrePrepare &pre_prepare = pbft.pre_prepare();
				ret = CheckValue(pre_prepare.value());
			}

			utils::MutexGuard lock_guad(lock_);
			PbftInstance *pinstance = CreateInstanceIfNotExist(env);
			if (pinstance) doret = pinstance->Go(env, this, ret);
			break;
		}
		case protocol::PBFT_TYPE_VIEWCHANG_WITH_RAWVALUE:{
			utils::MutexGuard lock_guad(lock_);
			doret = OnViewChangeWithRawValue(env);
			break;
		}
		case protocol::PBFT_TYPE_NEWVIEW:{
			utils::MutexGuard lock_guad(lock_);
			doret = OnNewView(env);
			break;
		}
		default: break;
		}
		return doret;
	}

	bool Pbft::OnPrePrepare(const protocol::Pbft &pbft, PbftInstance &pinstance, int32_t check_value_ret) {
		//Continue if it has only one node.
		if (view_number_ % validators_.size() == replica_id_ && validators_.size() != 1) {
			return true;
		}

		const protocol::PbftPrePrepare &pre_prepare = pbft.pre_prepare();
		//Check the value digest
		if (pre_prepare.value_digest() != HashWrapper::Crypto(pre_prepare.value())) {
			LOG_ERROR("The value digest(%s) is not equal to (%s)'s digest, desc(%s)",
				utils::String::BinToHexString(pre_prepare.value_digest()).c_str(),
				notify_->DescConsensusValue(pre_prepare.value()).c_str(), PbftDesc::GetPbft(pbft).c_str());
			return false;
		}

		//Check the value
		if (check_value_ret == Consensus::CHECK_VALUE_INVALID) {
			LOG_ERROR("Failed to check the value(%s), desc(%s)", notify_->DescConsensusValue(pre_prepare.value()).c_str(), PbftDesc::GetPbft(pbft).c_str());
			return false;
		}
		if (pinstance.phase_ != PBFT_PHASE_NONE) {
			bool ret = false;
			do {
				if (CompareValue(pinstance.pre_prepare_.value(), pre_prepare.value()) != 0) {
					LOG_ERROR("The pre-prepare message value(%s) != this value(%s) , desc(%s)",
						notify_->DescConsensusValue(pre_prepare.value()).c_str(), notify_->DescConsensusValue(pinstance.pre_prepare_.value()).c_str(), PbftDesc::GetPbft(pbft).c_str());
					break;
				}

				LOG_INFO("The message value(%s) received is duplicated, desc(%s)",
					notify_->DescConsensusValue(pre_prepare.value()).c_str(), PbftDesc::GetPbft(pbft).c_str());

				if (check_value_ret != Consensus::CHECK_VALUE_VALID) {
					LOG_INFO("Failed to check the value(view number:" FMT_I64 ",sequence:" FMT_I64 ", round number:1), so don't send prepare message.", pre_prepare.view_number(), pre_prepare.sequence());
					ret = true;
					break;
				}

				LOG_INFO("Send prepare message again: view number(" FMT_I64 "), sequence(" FMT_I64 "), round number(%u)",
					pre_prepare.view_number(), pre_prepare.sequence(), pbft.round_number());
				PbftEnvPointer prepare_msg = NewPrepare(pre_prepare, pbft.round_number());
				if (!SendMessage(prepare_msg)) {
					break;
				}
				ret = true;
			} while (false);

			return ret;
		}
		LOG_INFO("Received pre-prepare message from replica id(" FMT_I64 "): view number(" FMT_I64 "), sequence(" FMT_I64 "), round number(" FMT_I64 "), value(%s)",
			pre_prepare.replica_id(), pre_prepare.view_number(), pre_prepare.sequence(), pbft.round_number(),
			notify_->DescConsensusValue(pre_prepare.value()).c_str());

		//Insert the instance to map
		pinstance.phase_ = PBFT_PHASE_PREPREPARED;
		pinstance.phase_item_ = 0;
		pinstance.pre_prepare_ = pre_prepare;
		pinstance.check_value_result_ = check_value_ret;

		if (pinstance.check_value_result_ != Consensus::CHECK_VALUE_VALID) {
			LOG_INFO("Failed to check the value(view number:" FMT_I64 ",sequence:" FMT_I64 ", round number:1, value: %s), so don't send prepare message",
				pre_prepare.view_number(), pre_prepare.sequence(), notify_->DescConsensusValue(pre_prepare.value()).c_str());
			return true;
		}

		LOG_INFO("Send prepare message: view number(" FMT_I64 "), replica id(" FMT_I64 "), sequence(" FMT_I64 "), round number(1), value(%s)",
			pre_prepare.view_number(), replica_id_, pre_prepare.sequence(), notify_->DescConsensusValue(pre_prepare.value()).c_str());
		PbftEnvPointer prepare_msg = NewPrepare(pre_prepare, 1);
		if (!SendMessage(prepare_msg)) {
			return false;
		}

		//Start timer
		return true;
	}

	bool Pbft::OnPrepare(const protocol::Pbft &pbft, PbftInstance &pinstance) {
		const protocol::PbftPrepare &prepare = pbft.prepare();
		if (CompareValue(pinstance.pre_prepare_.value_digest(), prepare.value_digest()) != 0) {
			LOG_ERROR("The message prepare value(%s) != this pre-prepare value(%s) , desc(%s)",
				utils::String::Bin4ToHexString(prepare.value_digest()).c_str(),
				utils::String::Bin4ToHexString(pinstance.pre_prepare_.value_digest()).c_str(), PbftDesc::GetPbft(pbft).c_str());
			return false;
		}

		LOG_INFO("Received prepare message from replica id(" FMT_I64 "): view number(" FMT_I64 "),sequence(" FMT_I64 "), round number(" FMT_I64 ")",
			prepare.replica_id(), prepare.view_number(), prepare.sequence(), pbft.round_number());

		bool exist = false;
		PbftPrepareMap::iterator iter_prepare = pinstance.prepares_.find(prepare.replica_id());
		if (iter_prepare != pinstance.prepares_.end()) {
			LOG_INFO("The prepare message(view number:" FMT_I64 ", sequence:" FMT_I64 ", round number: %u) has been received duplicated, desc(%s)",
				prepare.view_number(), prepare.sequence(), pbft.round_number(), PbftDesc::GetPbft(pbft).c_str());
			exist = true;
		}

		pinstance.prepares_.insert(std::make_pair(prepare.replica_id(), prepare));
		if (pinstance.prepares_.size() >= GetQuorumSize()) {

			if (pinstance.phase_ < PBFT_PHASE_PREPARED) {  //Detect and receive again
				pinstance.phase_ = PBFT_PHASE_PREPARED;
				pinstance.phase_item_ = 0;
			}

			//Send commit
			if (pinstance.check_value_result_ == Consensus::CHECK_VALUE_VALID) {
				LOG_INFO("Sending commit message(%s), view number(" FMT_I64 "),sequence(" FMT_I64 "), round number(%u)", exist ? " again" : "",
					pinstance.pre_prepare_.view_number(), pinstance.pre_prepare_.sequence(), pbft.round_number());
				PbftEnvPointer commit_msg = NewCommit(prepare, pbft.round_number());
				pinstance.SetLastCommitSendTime(utils::Timestamp::HighResolution());
				return SendMessage(commit_msg);
			}
			else {
				LOG_INFO("Don't send commit message(msg value: %s, view number:" FMT_I64 ",sequence" FMT_I64 ", round number: %u) because the check result is not valid.", exist ? " again" : "",
					pinstance.pre_prepare_.view_number(), pinstance.pre_prepare_.sequence(), pbft.round_number());
			}
		}

		return true;
	}

	bool Pbft::OnCommit(const protocol::Pbft &pbft, PbftInstance &pinstance) {
		const protocol::PbftCommit &commit = pbft.commit();
		if (CompareValue(pinstance.pre_prepare_.value_digest(), commit.value_digest()) != 0) {
			LOG_ERROR("The message commit value(%s) != this pre-prepare value(%s) , desc(%s)",
				utils::String::Bin4ToHexString(commit.value_digest()).c_str(), notify_->DescConsensusValue(pinstance.pre_prepare_.value()).c_str(), PbftDesc::GetPbft(pbft).c_str());
			return false;
		}

		PbftCommitMap::iterator iter_commit = pinstance.commits_.find(commit.replica_id());
		if (iter_commit != pinstance.commits_.end()) {
			LOG_INFO("The prepare message(view number:" FMT_I64 ", sequence:" FMT_I64 ") has been received and duplicated",
				commit.view_number(), commit.sequence());
			return true;
		}

		LOG_INFO("Received commit message from replica(" FMT_I64 "): view number(" FMT_I64 "), sequence(" FMT_I64 "), round number(%u)",
			commit.replica_id(), commit.view_number(), commit.sequence(), pbft.round_number());
		pinstance.commits_.insert(std::make_pair(commit.replica_id(), commit));
		if (pinstance.commits_.size() >= GetQuorumSize() + 1 && pinstance.phase_ < PBFT_PHASE_COMMITED) {
			pinstance.phase_ = PBFT_PHASE_COMMITED;
			pinstance.phase_item_ = 0;
			pinstance.end_time_ = utils::Timestamp::HighResolution();
			LOG_INFO("Request commited, view number(" FMT_I64 "), sequence(" FMT_I64 "), try to execute consensus value.", pinstance.pre_prepare_.view_number(), pinstance.pre_prepare_.sequence());

			// This consensus has achieved.
			return TryExecuteValue();
		}

		return true;
	}

	bool Pbft::OnViewChangeWithRawValue(const protocol::PbftEnv &pbft_env) {
		const protocol::Pbft &pbft = pbft_env.pbft();
		const protocol::PbftViewChangeWithRawValue &view_change_raw = pbft.view_change_with_rawvalue();
		const protocol::PbftEnv &inner_pbft_env = view_change_raw.view_change_env();
		const protocol::PbftViewChange &view_change = inner_pbft_env.pbft().view_change();

		LOG_INFO("Received view change message from replica id(" FMT_I64 "): view number(" FMT_I64 "),round number(%u)",
			view_change.replica_id(), view_change.view_number(), pbft.round_number());
		if (view_change.view_number() == view_number_) {
			LOG_INFO("The new view number(" FMT_I64 ") is equal to current view number, then do nothing", view_change.view_number());
			return true;
		}
		else if (view_change.view_number() < view_number_) {
			LOG_INFO("The new view number(" FMT_I64 ") is less than current view number(" FMT_I64 "), then do nothing",
				view_change.view_number(), view_number_);
			return false;
		}

		LOG_INFO("Received view change message from replica id(" FMT_I64 "): view number(" FMT_I64 "),sequence(" FMT_I64 "), round number(%u)",
			view_change.replica_id(), view_change.view_number(), view_change.sequence(), pbft.round_number());

		ValueSaver saver;
		PbftVcInstanceMap::iterator iter = vc_instances_.find(view_change.view_number());
		if (iter == vc_instances_.end()) {   //New instance
			vc_instances_.insert(std::make_pair(view_change.view_number(), PbftVcInstance(view_change.view_number())));
			vc_instances_[view_change.view_number()].seq = view_change.sequence();
		}

		PbftVcInstance &vc_instance = vc_instances_[view_change.view_number()];

		//Insert into the msg need to be sent again for timeout
		if (view_change.replica_id() == replica_id_ && !vc_instance.view_change_msg_.has_pbft()) {
			vc_instance.view_change_msg_ = pbft_env;
		}

		PbftViewChangeMap::iterator iter_v = vc_instance.viewchanges_.find(view_change.replica_id());
		if (iter_v == vc_instance.viewchanges_.end()) {
			vc_instance.msg_buf_.push_back(pbft_env);
			vc_instance.viewchanges_.insert(std::make_pair(view_change.replica_id(), view_change));
		}

		if (view_change_raw.has_prepared_set()) {
			const protocol::PbftEnv &pre_prepared_pbft = view_change_raw.prepared_set().pre_prepare();
			const protocol::PbftEnv &last_pre_prepared_pbft = vc_instance.pre_prepared_env_set.pre_prepare();
			
			int64_t msg_seq = pre_prepared_pbft.pbft().pre_prepare().sequence();
			int64_t last_seq = last_pre_prepared_pbft.pbft().pre_prepare().sequence();
			if (msg_seq > last_seq && msg_seq > last_exe_seq_ ) {

				LOG_INFO("Replacing the view-change instance's pre-prepared env, pbft desc(%s)",
					PbftDesc::GetPbft(pre_prepared_pbft.pbft()).c_str());
				vc_instance.pre_prepared_env_set = view_change_raw.prepared_set();
			} 
		}

		if (vc_instance.viewchanges_.size() > GetQuorumSize() && vc_instance.end_time_ == 0) { //for view change, quorum size is 2f
			//View changes have achieved
			bool ret = ProcessQuorumViewChange(vc_instance);

			SaveViewChange(saver);
			return ret;
		}

		return true;
	}

	bool Pbft::OnNewView(const protocol::PbftEnv &pbft_env) {
		const protocol::Pbft &pbft = pbft_env.pbft();
		const protocol::PbftNewView &new_view = pbft.new_view();

		LOG_INFO("Received new view message from replica id(" FMT_I64 "): view number(" FMT_I64 "),round number(%u)",
			new_view.replica_id(), new_view.view_number(), pbft.round_number());
		if (new_view.view_number() == view_number_) {
			LOG_INFO("The new view number(" FMT_I64 ") is equal to current view number, then do nothing", new_view.view_number());
			return true;
		}
		else if (new_view.view_number() < view_number_) {
			LOG_INFO("The new view number(" FMT_I64 ") is less than current view number(" FMT_I64 "), then do nothing",
				new_view.view_number(), view_number_);
			return false;
		}

		//Delete the response timer
		bool ret = utils::Timer::Instance().DelTimer(new_view_repond_timer_);
		LOG_INFO("Delete new view repond timer( id:" FMT_I64 ") %s", new_view_repond_timer_, ret ? "true" : "failed");

		if (new_view.view_number() % validators_.size() == replica_id_) {
			LOG_INFO("It's the new primary(replica_id:" FMT_I64 "), so do not process the new view message", replica_id_);
			return true;
		}

		//Check the view change message
		PbftVcInstance vc_instance_tmp;
		vc_instance_tmp.view_number_ = new_view.view_number();

		bool check_ret = true;
		std::set<int64_t> replica_set;
		for (int32_t i = 0; i < new_view.view_changes_size(); i++) {
			const protocol::PbftEnv &view_change_env = new_view.view_changes(i);
			vc_instance_tmp.msg_buf_.push_back(view_change_env);
			if (!CheckMessageItem(pbft_env)) {
				check_ret = false;
				break;
			}

			const protocol::PbftViewChange &view_change = view_change_env.pbft().view_change();
			vc_instance_tmp.viewchanges_.insert(std::make_pair(view_change.replica_id(), view_change));

			if (new_view.view_number() != view_change.view_number()) {
				LOG_ERROR("The new view message's view-number(" FMT_I64 ") is not equal to it's view-change number(" FMT_I64 ")",
					new_view.view_number(), view_change.view_number());
				check_ret = false;
				break;
			}

			replica_set.insert(view_change.replica_id());
		}

		if (!check_ret) {
			return false;
		}

		if (replica_set.size() <= GetQuorumSize()) {
			LOG_ERROR("The new view message(number:" FMT_I64 ")'s count(" FMT_SIZE ") is less than or equal to qurom size(" FMT_SIZE ")",
				new_view.view_number(), replica_set.size(), GetQuorumSize());
			return false;
		}

		//Delete the other log
		for (PbftInstanceMap::iterator iter_inst = instances_.begin();
			iter_inst != instances_.end();
			) {
			if (iter_inst->second.phase_ == PBFT_PHASE_COMMITED  || 
				(iter_inst->second.phase_ == PBFT_PHASE_PREPARED &&
				iter_inst->first.sequence_ > last_exe_seq_)) {
				iter_inst++;
			}
			else {
				instances_.erase(iter_inst++);
			}
		}

		LOG_INFO("Replica(id: " FMT_I64 ") enter the new view(number:" FMT_I64 ")", replica_id_, new_view.view_number());
		//Enter the new view
		ValueSaver saver;
		view_number_ = new_view.view_number();
		view_active_ = true;
		saver.SaveValue(PbftDesc::VIEWNUMBER_NAME, view_number_);
		saver.SaveValue(PbftDesc::VIEW_ACTIVE, 1);

		PbftVcInstanceMap::iterator iter = vc_instances_.find(view_number_);
		if (iter != vc_instances_.end()) {
			iter->second.ChangeComplete(utils::Timestamp::HighResolution());
		}

		ClearViewChanges();
		OnViewChanged("");
		return true;
	}

	void Pbft::ClearViewChanges() {

		ValueSaver saver;
		//Delete other incomplete view change instances
		for (PbftVcInstanceMap::iterator iter_vc = vc_instances_.begin(); iter_vc != vc_instances_.end();) {
			if (iter_vc->second.end_time_ == 0) {
				LOG_INFO("Delete the view change instance (vn:" FMT_I64 ") that is not completed", iter_vc->second.view_number_);
				vc_instances_.erase(iter_vc++);
			}
			else if (iter_vc->second.view_number_ < view_number_ - 5) {
				LOG_INFO("Delete the view change instance (vn:" FMT_I64 ") that has passed by 5 view.", iter_vc->second.view_number_);
				vc_instances_.erase(iter_vc++);
			}
			else {
				iter_vc++;
			}
		}
		SaveViewChange(saver);
		saver.Commit();
	}

	bool Pbft::ProcessQuorumViewChange(PbftVcInstance &vc_instance) {
		LOG_INFO("Process quorum view-change, new view (number:" FMT_I64 ")", vc_instance.view_number_);
		
		std::string abnormal_node;
		int32_t vsize = validators_.size();
		for (std::map<std::string, int64_t>::iterator iter = validators_.begin();
			iter != validators_.end();
			iter++) {
			if (iter->second == view_number_ % vsize) {
				abnormal_node = iter->first;
				std::unordered_map<std::string, int64_t>::iterator it = abnormal_records_.find(abnormal_node);
				if (it != abnormal_records_.end())
				{
					it->second++;
				}
				else {
					abnormal_records_.insert(std::make_pair(abnormal_node, 1));
				}
				break;
			}
		}

		if (vc_instance.view_number_ % validators_.size() != replica_id_) { // we must be the leader

			protocol::PbftPreparedSet temp_set = vc_instance.pre_prepared_env_set;
			new_view_repond_timer_ = utils::Timer::Instance().AddTimer(30 * utils::MICRO_UNITS_PER_SEC, vc_instance.view_number_, [this, temp_set](int64_t data) {
				if (view_active_) {
					LOG_INFO("The current view(" FMT_I64 ") is active, so do not send new view(vn:" FMT_I64 ") ", view_number_
						, data + 1);
				}
				else {
					LOG_INFO("The new view(vn: " FMT_I64 ")'s primary was not respond,  then negotiates next view(vn: " FMT_I64 ")", data
						, data + 1);

					//SEND NEW VIEW
					LOG_INFO("Sending view change message, new view number(" FMT_I64 ")", data + 1);
					PbftEnvPointer msg = NewViewChangeRawValue(data + 1, temp_set);
					SendMessage(msg);
				}
			});

			LOG_INFO("It's not the new primary(replica_id:" FMT_I64 "), so don't process the quorum view message, waiting for new view message 30s, timer id(" FMT_I64")",
				vc_instance.view_number_ % validators_.size(), new_view_repond_timer_);
			return true;
		}

		//New view message
		PbftEnvPointer msg = NewNewView(vc_instance);

		LOG_INFO("Sending new view message, new view number(" FMT_I64 ")", vc_instance.view_number_);
		//Send new view message
		vc_instance.SendNewView(this, utils::Timestamp::HighResolution(), msg);

		//Get last prepared consensus value
		std::string last_cons_value;
		if (vc_instance.pre_prepared_env_set.has_pre_prepare()) {
			const protocol::Pbft &pbft = vc_instance.pre_prepared_env_set.pre_prepare().pbft();
			last_cons_value = pbft.pre_prepare().value();
			LOG_INFO("View-change instance got the prepared value, desc(%s)", PbftDesc::GetPbft(pbft).c_str());
		}

		//Delete uncommitted instances
		for (PbftInstanceMap::iterator iter_inst = instances_.begin();
			iter_inst != instances_.end();
			) {
			if (iter_inst->second.phase_ == PBFT_PHASE_COMMITED ||
				(iter_inst->second.phase_ == PBFT_PHASE_PREPARED &&
				iter_inst->first.sequence_ > last_exe_seq_)) {
				iter_inst++;
			}
			else {
				instances_.erase(iter_inst++);
			}
		}

		ValueSaver saver;
		//Enter the new view

		view_number_ = vc_instance.view_number_;
		view_active_ = true;
		saver.SaveValue(PbftDesc::VIEW_ACTIVE, view_active_ ? 1 : 0);

		LOG_INFO("Primary enter the new view(number:" FMT_I64 ")", view_number_);
		vc_instance.ChangeComplete(utils::Timestamp::HighResolution());
		saver.SaveValue(PbftDesc::VIEWNUMBER_NAME, view_number_);

		ClearViewChanges();

		notify_->OnResetCloseTimer();
		OnViewChanged(last_cons_value);

		return true;
	}

	bool Pbft::TryExecuteValue() {
		for (PbftInstanceMap::iterator iter = instances_.begin(); iter != instances_.end(); iter++) {
			PbftInstance &instance = iter->second;
			const PbftInstanceIndex &index = iter->first;
			if (index.sequence_ <= last_exe_seq_) {
				continue;
			}

			if (index.sequence_ == last_exe_seq_ + 1 && instance.phase_ >= PBFT_PHASE_COMMITED) {
				last_exe_seq_++;
			}
			else {
				break;
			}

			//Get 'commit' env from buf
			protocol::PbftProof proof;
			const PbftPhaseVector &vec = instance.msg_buf_[protocol::PBFT_TYPE_COMMIT];
			std::set<std::string> commit_node;
			for (size_t i = 0; i < vec.size(); i++) {
				const protocol::PbftEnv &env = vec[i];
				const protocol::Signature &sign = env.signature();
				if (commit_node.find(sign.public_key()) == commit_node.end()) {
					*proof.add_commits() = env;
					commit_node.insert(sign.public_key());
				}
			}

			std::string state_digest = OnValueCommited(
				index.sequence_,
				instance.pre_prepare_.value(), 
				proof.SerializeAsString(),true);

			//Delete the old check point
			for (PbftInstanceMap::iterator iter = instances_.begin(); iter != instances_.end();) {
				if (iter->first.sequence_ <= index.sequence_ - ckp_interval_ / 2) {
					instances_.erase(iter++);
				} 
				else {
					iter++;
				}
			}
		}
		return true;
	}

	PbftEnvPointer Pbft::NewPrePrepare(const std::string &value, int64_t sequence) {
		PbftEnvPointer env = std::make_shared<protocol::PbftEnv>();

		protocol::Pbft *pbft = env->mutable_pbft();
		pbft->set_round_number(1);
		pbft->set_type(protocol::PBFT_TYPE_PREPREPARE);
		pbft->set_chain_id(General::GetSelfChainId());

		protocol::PbftPrePrepare *preprepare = pbft->mutable_pre_prepare();
		preprepare->set_view_number(view_number_);
		preprepare->set_replica_id(replica_id_);
		preprepare->set_sequence(sequence);
		*preprepare->mutable_value() = value;
		preprepare->set_value_digest(HashWrapper::Crypto(value));

		protocol::Signature *sig = env->mutable_signature();
        sig->set_public_key(private_key_.GetEncPublicKey());
		sig->set_sign_data(private_key_.Sign(pbft->SerializeAsString()));
		return env;
	}

	protocol::PbftEnv Pbft::NewPrePrepare(const protocol::PbftPrePrepare &pre_prepare) {
		protocol::PbftEnv env;
		protocol::Pbft *pbft = env.mutable_pbft();
		pbft->set_round_number(1);
		pbft->set_type(protocol::PBFT_TYPE_PREPREPARE);
		pbft->set_chain_id(General::GetSelfChainId());

		*pbft->mutable_pre_prepare() = pre_prepare;

		protocol::Signature *sig = env.mutable_signature();
        sig->set_public_key(private_key_.GetEncPublicKey());
		sig->set_sign_data(private_key_.Sign(pbft->SerializeAsString()));
		return env;
	}

	PbftEnvPointer Pbft::NewPrepare(const protocol::PbftPrePrepare &pre_prepare, int64_t round_number) {
		PbftEnvPointer env = std::make_shared<protocol::PbftEnv>();

		protocol::Pbft *pbft = env->mutable_pbft();
		pbft->set_round_number(round_number);
		pbft->set_type(protocol::PBFT_TYPE_PREPARE);
		pbft->set_chain_id(General::GetSelfChainId());

		protocol::PbftPrepare *prepare = pbft->mutable_prepare();
		prepare->set_view_number(pre_prepare.view_number());
		prepare->set_replica_id(replica_id_);
		prepare->set_sequence(pre_prepare.sequence());
		prepare->set_value_digest(pre_prepare.value_digest());

		protocol::Signature *sig = env->mutable_signature();
        sig->set_public_key(private_key_.GetEncPublicKey());
		sig->set_sign_data(private_key_.Sign(pbft->SerializeAsString()));
		return env;
	}

	PbftEnvPointer Pbft::NewCommit(const protocol::PbftPrepare &prepare, int64_t round_number) {
		PbftEnvPointer env = std::make_shared<protocol::PbftEnv>();

		protocol::Pbft *pbft = env->mutable_pbft();
		pbft->set_round_number(round_number);
		pbft->set_type(protocol::PBFT_TYPE_COMMIT);
		pbft->set_chain_id(General::GetSelfChainId());

		protocol::PbftCommit *preprepare = pbft->mutable_commit();
		preprepare->set_view_number(prepare.view_number());
		preprepare->set_replica_id(replica_id_);
		preprepare->set_sequence(prepare.sequence());
		preprepare->set_value_digest(prepare.value_digest());

		protocol::Signature *sig = env->mutable_signature();
        sig->set_public_key(private_key_.GetEncPublicKey());
		sig->set_sign_data(private_key_.Sign(pbft->SerializeAsString()));
		return env;
	}

	PbftEnvPointer Pbft::NewViewChangeRawValue(int64_t view_number, const protocol::PbftPreparedSet &prepared_set) {
		PbftEnvPointer env = std::make_shared<protocol::PbftEnv>();

		protocol::Pbft *pbft = env->mutable_pbft();
		pbft->set_round_number(0);
		pbft->set_type(protocol::PBFT_TYPE_VIEWCHANG_WITH_RAWVALUE);
		pbft->set_chain_id(General::GetSelfChainId());
		
		protocol::PbftViewChangeWithRawValue *vc_raw = pbft->mutable_view_change_with_rawvalue();

		//Add view change
		protocol::PbftEnv *pbft_env_inner = vc_raw->mutable_view_change_env();
		protocol::Pbft *pbft_inner = pbft_env_inner->mutable_pbft();
		pbft_inner->set_round_number(0);
		pbft_inner->set_type(protocol::PBFT_TYPE_VIEWCHANGE);
		pbft_inner->set_chain_id(General::GetSelfChainId());

		protocol::PbftViewChange *pviewchange = pbft_inner->mutable_view_change();
		pviewchange->set_view_number(view_number);
		pviewchange->set_sequence(last_exe_seq_);
		pviewchange->set_replica_id(replica_id_);

		if (prepared_set.has_pre_prepare()) {
			*vc_raw->mutable_prepared_set() = prepared_set;
			LOG_INFO("Got prepared value again, desc(%s)", PbftDesc::GetPbft(prepared_set.pre_prepare().pbft()).c_str());
		} else{
			//Add prepared set
			for (PbftInstanceMap::reverse_iterator iter_instance = instances_.rbegin();
				iter_instance != instances_.rend();
				iter_instance++) {
				const PbftInstance &instance = iter_instance->second;
				const PbftInstanceIndex &index = iter_instance->first;
				if (index.sequence_ > pviewchange->sequence() && instance.phase_ == PBFT_PHASE_PREPARED) {
					protocol::PbftPreparedSet *prepared_set_inner = vc_raw->mutable_prepared_set();

					//Add prepared message and pre-prepared message
					*prepared_set_inner->mutable_pre_prepare() = instance.msg_buf_[0][0];//Add prepreared message

					//Add prepared message
					for (size_t i = 0; i < instance.msg_buf_[1].size(); i++) {
						*prepared_set_inner->add_prepare() = instance.msg_buf_[1][i];
					}

					LOG_INFO("Got prepared value, desc(%s)", PbftDesc::GetPbft(prepared_set_inner->pre_prepare().pbft()).c_str());
					break;
				}
			}
		}
	

		//Add 'view change' value digest
		if (vc_raw->has_prepared_set()) {
			const protocol::PbftEnv &pp_pbft_env = vc_raw->prepared_set().pre_prepare();
			const protocol::PbftPrePrepare &pp_pbft = pp_pbft_env.pbft().pre_prepare();
			pviewchange->set_prepred_value_digest(pp_pbft.value_digest());
		}


		//Add 'view change' signature
		protocol::Signature *sig = pbft_env_inner->mutable_signature();
		sig->set_public_key(private_key_.GetEncPublicKey());
		sig->set_sign_data(private_key_.Sign(pbft_inner->SerializeAsString()));

		//Add 'view change' raw value signature
		protocol::Signature *sig_out = env->mutable_signature();
		sig_out->set_public_key(private_key_.GetEncPublicKey());
		sig_out->set_sign_data(private_key_.Sign(pbft->SerializeAsString()));

		return env;
	}

	PbftEnvPointer Pbft::NewNewView(PbftVcInstance &vc_instance) {
		PbftEnvPointer env = std::make_shared<protocol::PbftEnv>();

		protocol::Pbft *pbft = env->mutable_pbft();
		pbft->set_round_number(0);
		pbft->set_type(protocol::PBFT_TYPE_NEWVIEW);
		pbft->set_chain_id(General::GetSelfChainId());

		protocol::PbftNewView *pnewview = pbft->mutable_new_view();
		pnewview->set_view_number(vc_instance.view_number_);
		pnewview->set_replica_id(replica_id_);
		pnewview->set_sequence(vc_instance.seq);

		for (PbftPhaseVector::iterator iter = vc_instance.msg_buf_.begin(); iter != vc_instance.msg_buf_.end(); iter++) {
			const protocol::PbftEnv &env_out = *iter;

			//Get the inner view change env
			*pnewview->add_view_changes() = env_out.pbft().view_change_with_rawvalue().view_change_env();
		}

		if (vc_instance.pre_prepared_env_set.has_pre_prepare()) {
			*pnewview->mutable_pre_prepare() = vc_instance.pre_prepared_env_set.pre_prepare();
		}

		protocol::Signature *sig = env->mutable_signature();
		sig->set_public_key(private_key_.GetEncPublicKey());
		sig->set_sign_data(private_key_.Sign(pbft->SerializeAsString()));
		return env;
	}

	PbftEnvPointer Pbft::IncPeerMessageRound(const protocol::PbftEnv &message, uint32_t round_number) {
		PbftEnvPointer env = std::make_shared<protocol::PbftEnv>(message);

		protocol::Pbft *pbft = env->mutable_pbft();
		pbft->set_round_number(round_number);
		pbft->set_chain_id(General::GetSelfChainId());

		protocol::Signature *sig = env->mutable_signature();
        sig->set_public_key(private_key_.GetEncPublicKey());
		sig->set_sign_data(private_key_.Sign(pbft->SerializeAsString()));

		return env;
	}

	size_t Pbft::GetQuorumSize() {
		return GetQuorumSize(validators_.size());
	}

	size_t Pbft::GetQuorumSize(size_t size) {
		// N       1   2   3   4   5   6   7   8   9
		// quorum  0   1   1   2   3   3   4   5   5
		// q +1    1   2   2   3   4   4   5   6   6

		if (size == 1) { // for debug
			return 0;
		}

		if (size == 2 || size == 3) { // for debug
			return 1;
		}

		size_t fault_number = (size - 1) / 3;
		size_t qsize = size;
		if (size == 3 * fault_number + 1) {
			qsize = 2 * fault_number;
		}
		else if (size == 3 * fault_number + 2) {
			qsize = 2 * fault_number + 1;
		}
		else if (size == 3 * fault_number + 3) {
			qsize = 2 * fault_number + 1;
		}

		return qsize;
	}

	void Pbft::OnTxTimeout() {
		if (!is_validator_) {
			return;
		}

		utils::MutexGuard lock_guad(lock_);

		LOG_INFO("Sending view change message, new view number(" FMT_I64 ")", view_number_ + 1);
		view_active_ = false;
		ValueSaver saver;
		saver.SaveValue(PbftDesc::VIEW_ACTIVE, view_active_ ? 1 : 0);
		protocol::PbftPreparedSet null_set;
		PbftEnvPointer msg = NewViewChangeRawValue(view_number_ + 1, null_set);
		SendMessage(msg);
	}

	std::string Pbft::GetNodeAddress(const protocol::PbftEnv &pbft_env) {
		PublicKey public_key(pbft_env.signature().public_key());
        return public_key.GetEncAddress();
	}

	int64_t Pbft::GetSeq(const protocol::PbftEnv &pbft_env) {
		const protocol::Pbft &pbft = pbft_env.pbft();
		int64_t sequence = 0;
		switch (pbft.type()) {
		case protocol::PBFT_TYPE_PREPREPARE:{
			if (pbft.has_pre_prepare()) sequence = pbft.pre_prepare().sequence();
			break;
		}
		case protocol::PBFT_TYPE_PREPARE:{
			if (pbft.has_prepare()) sequence = pbft.prepare().sequence();
			break;
		}
		case protocol::PBFT_TYPE_COMMIT:{
			if (pbft.has_commit()) sequence = pbft.commit().sequence();
			break;
		}
		case protocol::PBFT_TYPE_VIEWCHANGE:{
			if (pbft.has_view_change()) sequence = pbft.view_change().sequence();
			break;
		}
		case protocol::PBFT_TYPE_NEWVIEW:{
			if (pbft.has_new_view()) sequence = pbft.new_view().sequence();
			break;
		}
		default:{
			break;
		}
		}

		return sequence;
	}

	std::vector<std::string> Pbft::GetValue(const protocol::PbftEnv &pbft_env) {
		const protocol::Pbft &pbft = pbft_env.pbft();
		std::vector<std::string> values;
		switch (pbft.type()) {
		case protocol::PBFT_TYPE_PREPREPARE:{
			if (pbft.has_pre_prepare()) values.push_back(pbft.pre_prepare().value());
			break;
		}
											//case protocol::PBFT_TYPE_PREPARE:{
											//	if (pbft.has_prepare()) values.push_back(pbft.prepare().value());
											//	break;
											//}
											//case protocol::PBFT_TYPE_COMMIT:{
											//	if (pbft.has_commit()) values.push_back(pbft.commit().value());
											//	break;
											//}
		case protocol::PBFT_TYPE_VIEWCHANG_WITH_RAWVALUE:{
			if (pbft.has_view_change_with_rawvalue()) {
				const protocol::PbftViewChangeWithRawValue &view_change = pbft_env.pbft().view_change_with_rawvalue();
				const protocol::PbftPreparedSet &prepare_set = view_change.prepared_set();
				const protocol::PbftEnv &pre_prepare = prepare_set.pre_prepare();
				values.push_back(pre_prepare.pbft().pre_prepare().value());
			}
			break;
		}
		case protocol::PBFT_TYPE_NEWVIEW:{
			if (pbft.has_new_view()) {
				const protocol::PbftNewView &new_view = pbft_env.pbft().new_view();
				const protocol::PbftEnv &pre_prepare = new_view.pre_prepare();
				values.push_back(pre_prepare.pbft().pre_prepare().value());
			}
			break;
		}
		default:{
			break;
		}
		}

		return values;
	}

	void Pbft::GetModuleStatus(Json::Value &data) {
		utils::MutexGuard lock_guad(lock_);
		data["type"] = name_;
		data["replica_id"] = replica_id_;
		data["view_number"] = view_number_;
		data["ckp_interval"] = ckp_interval_;
		data["last_exe_seq"] = last_exe_seq_;
		data["fault_number"] = (Json::Int64)fault_number_;
		data["view_active"] = view_active_;
		data["is_leader"] = (replica_id_ == view_number_ % validators_.size());
		data["validator_address"] = private_key_.GetEncAddress();
		data["validator_address_random"] = validation_random;
		data["leader"] = CurrentLeader();
		Json::Value &instances = data["instances"];
		for (PbftInstanceMap::const_iterator iter = instances_.begin(); iter != instances_.end(); iter++) {
			const PbftInstance &instance = iter->second;
			const PbftInstanceIndex &index = iter->first;
			Json::Value &item = instances[instances.size()];
			item["vn"] = index.view_number_;
			item["seq"] = index.sequence_;
			item["phase"] = GetPhaseDesc(instance.phase_);
			item["phase_item"] = (Json::UInt64)instance.phase_item_;
			item["pre_prepare"] = PbftDesc::GetPrePrepare(instance.pre_prepare_);

			Json::Value &prepares = item["prepares"];
			for (PbftPrepareMap::const_iterator iter_pre = instance.prepares_.begin();
				iter_pre != instance.prepares_.end();
				iter_pre++) {
				Json::Value &prepares_item = prepares[prepares.size()];
				prepares_item = PbftDesc::GetPrepare(iter_pre->second);
			}

			Json::Value &commits = item["commits"];
			for (PbftCommitMap::const_iterator iter_commit = instance.commits_.begin();
				iter_commit != instance.commits_.end();
				iter_commit++) {
				Json::Value &commits_item = commits[commits.size()];
				commits_item = PbftDesc::GetCommit(iter_commit->second);
			}
			item["start_time"] = utils::Timestamp(instance.start_time_).ToFormatString(true);
			item["end_time"] = utils::Timestamp(instance.end_time_).ToFormatString(true);
			item["last_propose_time"] = utils::Timestamp(instance.last_propose_time_).ToFormatString(true);
			item["have_send_viewchange"] = instance.have_send_viewchange_;
			item["pre_prepare_round"] = instance.pre_prepare_round_;
		}

		Json::Value &viewchanges = data["viewchanges"];
		for (PbftVcInstanceMap::const_iterator iter = vc_instances_.begin(); iter != vc_instances_.end(); iter++) {
			const PbftVcInstance &vc_instance = iter->second;
			Json::Value &item = viewchanges[viewchanges.size()];
			item["view_number"] = vc_instance.view_number_;
			item["start_time"] = utils::Timestamp(vc_instance.start_time_).ToFormatString(true);
			item["last_propose_time"] = utils::Timestamp(vc_instance.last_propose_time_).ToFormatString(true);
			item["end_time"] = utils::Timestamp(vc_instance.end_time_).ToFormatString(true);
			item["newview_init"] = vc_instance.newview_.has_pbft();
			item["prepared_pre_env"] = vc_instance.pre_prepared_env_set.has_pre_prepare() ?
				PbftDesc::GetPbft(vc_instance.pre_prepared_env_set.pre_prepare().pbft()) : "";

			Json::Value &vc = item["viewchange"];
			for (PbftViewChangeMap::const_iterator iter_vc = vc_instance.viewchanges_.begin(); iter_vc != vc_instance.viewchanges_.end(); iter_vc++) {
				Json::Value &vc_item = vc[vc.size()];
				vc_item = PbftDesc::GetViewChange(iter_vc->second);
			}
		}

		Json::Value &validators = data["validators"];
		protocol::ValidatorSet set;
		size_t quorum_size;
		GetValidation(set, quorum_size);
		for (int32_t i = 0; i < set.validators_size(); i++) {
			validators[validators.size()] = set.validators(i).address();
		}
		data["quorum_size"] = (Json::UInt64)quorum_size;

		Json::Value records_json;
		std::unordered_map<std::string, int64_t>::iterator it = abnormal_records_.begin();
		for (; it != abnormal_records_.end(); it++)
		{
			records_json["address"] = it->first;
			records_json["count"] = it->second;
		}
		data["abnormal_records"] = records_json;
	}

	int32_t Pbft::IsLeader() {
		if (IsValidator() && view_number_ % validators_.size() == replica_id_) {
			return 1;
		}

		return 0;
	}

	void Pbft::ClearNotCommitedInstance() {
		ValueSaver saver;
		//Discard other logs
		for (PbftInstanceMap::iterator iter_inst = instances_.begin();
			iter_inst != instances_.end();
			) {
			if (iter_inst->second.phase_ < PBFT_PHASE_COMMITED) {
				instances_.erase(iter_inst++);
			}
			else {
				iter_inst++;
			}
		}
	}

	std::string Pbft::CurrentLeader(){
		std::string leader;
		int64_t leaderId = view_number_ % validators_.size();

		for (auto validator : validators_){
			if (validator.second == leaderId){
				leader = validator.first;
			}
		}

		return leader;
	}

	//update
	bool Pbft::UpdateValidators(const protocol::ValidatorSet &validators, const std::string &proof) {
		utils::MutexGuard guard_(lock_);

		int64_t new_view_number = -1;
		int64_t new_seq = -1;
		if (proof.empty()) {
			new_view_number = 0;
		}
		else {
			protocol::PbftProof pbft_proof;
			if (!pbft_proof.ParseFromString(proof)) {
				LOG_ERROR("Failed to parse proof string");
				return false;
			}

			//Compare view number
			if (pbft_proof.commits_size() > 0) {
				const protocol::PbftEnv &env = pbft_proof.commits(0);
				const protocol::Pbft &pbft = env.pbft();
				const protocol::PbftCommit &commit = pbft.commit();
				if (commit.view_number() >= view_number_) {
					new_view_number = commit.view_number() + 1;
				}
				if (commit.sequence() > last_exe_seq_){
					new_seq = commit.sequence();
				}
			}
		}

		//Compare the validators
		bool validator_changed = (validators.validators_size() != validators_.size());
		int32_t validator_index = 0;
		for (int32_t i = 0; i < validators.validators_size(); i++) {
			std::map<std::string, int64_t>::iterator iter = validators_.find(validators.validators(i).address());
			if (iter == validators_.end()){
				validator_changed = true;
				break;
			}
			else if (iter->second != validator_index++) {
				validator_changed = true;
				break;
			}
		}

		ValueSaver saver;
		if (validator_changed ){
			//Update the validators
			Consensus::UpdateValidators(validators);

			if (validators_.size() < 4) {
				LOG_WARN("Pbft couldn't tolerate fault node when validator size =" FMT_SIZE ".", validators_.size());
			}

			fault_number_ = (validators_.size() - 1) / 3;

			SaveValidators(saver);

			LOG_INFO("When validator size = " FMT_SIZE ", pbft can tolerate " FMT_SIZE " fault nodes. Current node's replica_id = " FMT_I64 ", so it %s a leader",
				validators_.size(), fault_number_, replica_id_, view_number_ % validators_.size() == replica_id_ ? "is" : "isnot");
			
			ClearNotCommitedInstance();
			notify_->OnResetCloseTimer();

			//Clear abnormal records
			abnormal_records_.clear();
		} 
		
		if (new_seq > 0) {
			
			last_exe_seq_ = new_seq;

			LOG_INFO("Setting the last execution sequence(" FMT_I64 ")", last_exe_seq_);
			saver.SaveValue(PbftDesc::LAST_EXE_SEQUENCE_NAME, last_exe_seq_);
		}
		
		if ( new_view_number > 0 || new_seq > 0 ){
			ClearNotCommitedInstance();

			//Enter to new view
			view_number_ = new_view_number > 0 ? new_view_number : view_number_;
			view_active_ = true;
			saver.SaveValue(PbftDesc::VIEW_ACTIVE, true);

			LOG_INFO("%s enter the new view(number:" FMT_I64 ")", replica_id_ >= 0 ? (IsLeader() ? "Primary" : "replica") : "SynNode", view_number_);
			saver.SaveValue(PbftDesc::VIEWNUMBER_NAME, view_number_);

			//Delete other incomplete view change instances or other view change instances whose sequence is less than 5.
			for (PbftVcInstanceMap::iterator iter_vc = vc_instances_.begin(); iter_vc != vc_instances_.end();) {
				if (iter_vc->second.end_time_ == 0) {
					LOG_INFO("Delete the view change instance (vn:" FMT_I64 ") that is not completed", iter_vc->second.view_number_);
					vc_instances_.erase(iter_vc++);
				}
				else if (iter_vc->second.view_number_ < view_number_ - 5) {
					LOG_INFO("Delete the view change instance (vn:" FMT_I64 ") that has passed by 5 view.", iter_vc->second.view_number_);
					vc_instances_.erase(iter_vc++);
				}
				else {
					iter_vc++;
				}
			}

			notify_->OnResetCloseTimer();
			saver.Commit();
		}

		//Check the view number and validators.
		return true;
	}

	bool Pbft::CheckProof(const protocol::ValidatorSet &validators, const std::string &previous_value_hash, const std::string &proof) {
		ValidatorMap temp_vs;
		int64_t counter = 0;
		for (int32_t i = 0; i < validators.validators_size(); i++) {
			temp_vs.insert(std::make_pair(validators.validators(i).address(), counter++));
		}
		size_t total_size = temp_vs.size();
		size_t qsize = GetQuorumSize(temp_vs.size()) + 1;
		
		//Check proof
		protocol::PbftProof pbft_evidence;
		if (!pbft_evidence.ParseFromString(proof)) {
			LOG_ERROR("Failed to parse proof string");
			return false;
		}

		for (int32_t i = 0; i < pbft_evidence.commits_size(); i++) {
			const protocol::PbftEnv &env = pbft_evidence.commits(i);
			const protocol::Pbft &pbft = env.pbft();
			if (!CheckMessageItem(env, temp_vs)) {
				LOG_ERROR("Failed to check proof message item: validators:(%s), hash(%s), proof(%s), total_size(" FMT_SIZE "), qsize(" FMT_SIZE "), counter(" FMT_I64 ")", 
					Proto2Json(validators).toFastString().c_str(), utils::String::BinToHexString(previous_value_hash).c_str(), 
					Proto2Json(pbft_evidence).toFastString().c_str(),
					total_size, qsize,
					counter);
				return false;
			}

			if (pbft.type() != protocol::PBFT_TYPE_COMMIT || !pbft.has_commit()) {
				LOG_ERROR("Failed to check proof message item: type(%s) is not valid.", PbftDesc::GetMessageTypeDesc(pbft.type()));
				return false;
			}

			const protocol::PbftCommit &commit = pbft.commit();
			if (commit.value_digest() != previous_value_hash) {
				LOG_ERROR("Failed to check proof message item, because message value hash(%s) is not equal to previous value hash(%s)",
					utils::String::BinToHexString(commit.value_digest()).c_str(), utils::String::BinToHexString(previous_value_hash).c_str());
				return false;
			}

			const protocol::Signature &sign = env.signature();
			PublicKey pub_key(sign.public_key());
            std::string address = pub_key.GetEncAddress();
			if (temp_vs.find(address) == temp_vs.end()) {
				LOG_ERROR("Failed to check proof, because address(%s) is not found or duplicated", address.c_str());
				return false;
			}

			temp_vs.erase(address);
		}

		if (total_size - temp_vs.size() >= qsize) {
			return true;
		}
		else {
			LOG_ERROR("Failed to check proof, because message quorum size(" FMT_SIZE ") < quorum size( " FMT_SIZE ") ", 
				total_size - temp_vs.size(), qsize);
			return false;
		}
	}

	const char *Pbft::GetPhaseDesc(PbftInstancePhase phase) {
		switch (phase) {
		case PBFT_PHASE_NONE: return "PHASE_NONE";
		case PBFT_PHASE_PREPREPARED: return "PHASE_PREPREPARE";
		case PBFT_PHASE_PREPARED: return "PHASE_PREPARED";
		case PBFT_PHASE_COMMITED: return "PHASE_COMMITED";
		default: break;
		}
		return "UNDEFINE";
	}
}
