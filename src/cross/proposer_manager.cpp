#include "proposer_manager.h"

namespace bumo {
	ProposerManager::ProposerManager() :
		enabled_(false),
		last_uptate_validate_address_time_(0),
		thread_ptr_(NULL){
		
	}

	ProposerManager::~ProposerManager(){
		if (thread_ptr_){
			delete thread_ptr_;
			thread_ptr_ = NULL;
		}
	}

	bool ProposerManager::Initialize(){
		enabled_ = true;
		thread_ptr_ = new utils::Thread(this);
		if (!thread_ptr_->Start("ProposerManager")) {
			return false;
		}
		bumo::MessageChannel::GetInstance()->RegisterMessageChannelConsumer(this, protocol::MESSAGE_CHANNEL_SUBMIT_HEAD);
		return true;
	}

	bool ProposerManager::Exit(){
		enabled_ = false;
		if (thread_ptr_) {
			thread_ptr_->JoinWithStop();
		}
		bumo::MessageChannel::GetInstance()->UnregisterMessageChannelConsumer(this, protocol::MESSAGE_CHANNEL_SUBMIT_HEAD);
		return true;
	}

	bool ProposerManager::CommitChildChainBlock(){
		///Push to a block map

		return true;
	}

	void ProposerManager::Run(utils::Thread *thread) {
		int64_t current_time = utils::Timestamp::HighResolution();
		while (enabled_){
			//Handel block map//
			HandleChildChainBlock();
			if (current_time > 2 * 60 * utils::MICRO_UNITS_PER_SEC + last_uptate_validate_address_time_)
			{
				UpdateValidateAddressList();
				last_uptate_validate_address_time_ = current_time;
			}
		}
	}

	void ProposerManager::HandleChildChainBlock(){
		//TODO: handel child chain block, and call MessageChannel to send main chain proc 

		if (CheckChildBlockExsit()){
			return;
		}

		CommitTransaction();
	}

	void ProposerManager::HandleMessageChannelConsumer(const protocol::MessageChannel &message_channel){
		if (message_channel.msg_type() != protocol::MESSAGE_CHANNEL_SUBMIT_HEAD){
			return;
		}


	}

	void ProposerManager::UpdateValidateAddressList(){
		int64_t chain_id = General::GetSelfChainId();
	}

	bool ProposerManager::AddressIsValidate(const std::string &address){
		utils::StringList::const_iterator itor;
		bool flag = false;
		utils::MutexGuard guard(validate_address_lock_);
		itor = std::find(validate_address_.begin(), validate_address_.end(), address.c_str());
		if (itor != validate_address_.end()){
			flag = true;
		}
		else{
			flag = false;
		}
		return flag;
	}

	bool ProposerManager::CheckChildBlockExsit(){
		//TODO: Check for child chain block in CMC

		return true;
	}

	bool ProposerManager::CommitTransaction(){
		//TODO: create a mainchain transaction with private key to CMC

		return true;
	}
}
