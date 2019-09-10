#ifndef TEST_API_GETTRANSACTIONBLOB
#define TEST_API_GETTRANSACTIONBLOB

#include <gtest/gtest.h>

#include "common/http_client.h"
#include "common/private_key.h"
#include "common/web_socket_server.h"

#include <utils/crypto.h>
#include <utils/strings.h>
#include <json/json.h>
class ApiTest :public testing::Test{
protected:
	virtual void SetUp(){
		http_ = new bubi::HttpClient();
		http_->Initialize("192.168.10.92:19333");
		//address:bubiV8i2MLZd5ahDGay6oAZHiMyYNUkJfSiTAmJy
		priv_key_ = new bubi::PrivateKey("privC1CCDoFh9kfbKRHf9PZMPzH1N5vkBZWdbJxnkiZji7UfZsSXvUxU");
	}
	virtual void TearDown(){
		delete priv_key_;
		delete http_;
		priv_key_ = NULL;
	}
public:
	static bubi::PrivateKey *priv_key_;
	static bubi::HttpClient *http_;
};

bubi::PrivateKey *ApiTest::priv_key_ = NULL;
bubi::HttpClient *ApiTest::http_ = NULL;
Json::Value GetTransactionJson(bubi::PrivateKey & prv){
	
	Json::Value transaction_json;
	transaction_json["source_address"] = prv.GetBase58Address();
	transaction_json["fee"] = 1000;

	Json::Value operation;
	operation["type"] = 2;
	operation["asset_type"] = 1;
	operation["asset_issuer"] = prv.GetBase58Address();
	operation["asset_code"] = "abcd";
	operation["asset_amount"] = 1;

	transaction_json["operations"].append(operation);
	return transaction_json;
}

Json::Value SubmintTransaction(bubi::PrivateKey &prv,std::string sign_data, std::string transaction_blob){
	Json::Value item;

	Json::Value signature;
	signature["sign_data"] = sign_data;
	signature["public_key"] =prv.GetBase58PublicKey();

	item["signatures"].append(signature);
	item["transaction_blob"] = transaction_blob;

	Json::Value items;
	items["items"].append(item);
	return items;
}

bool CheckTransaction(int64_t time_out,std::string hash,bubi::HttpClient& http){
	time_out = time_out * utils::MICRO_UNITS_PER_SEC;
	int64_t start_time = utils::Timestamp::HighResolution();

	while (utils::Timestamp::HighResolution() - start_time < time_out){

		bubi::HttpClient::RecvMessage rec = http.http_request(bubi::HttpClient::HTTP_GET, "getTransactionHistory?hash=" + hash, "");
		Json::Value rep;
		rep.fromCString(rec.context.c_str());

		if (rep.isMember("error_code") && rep["error_code"].asInt() == 0){
			if (rep.isMember("result") && rep["result"].isMember("transactions") && rep["result"]["transaction"].size() > 0
				&& rep["result"]["transactions"][Json::UInt(0)].isMember("error_code"))
			if (rep["result"]["transactions"][Json::UInt(0)]["error_code"].asInt() != 0){
				std::cout << rec.context;
				return false;
			}
			return true;
		}
		utils::Sleep(1000);
	}
	std::cout << " hash:" << hash << std::endl;
	return false;
}

static std::list<std::string >  hashs;
static utils::ReadWriteLock hashs_mutex;

class  ApiTestWorker :
	public utils::Runnable{
public:
	ApiTestWorker(){}
	~ApiTestWorker(){}
	ApiTestWorker(const ApiTestWorker& s) = delete;
	
	virtual void Run(utils::Thread *thread) override;
};

static int64_t  num = 0;
static utils::ReadWriteLock num_mutex;

void ApiTestWorker::Run(utils::Thread *thread){
	bubi::PrivateKey priv_key("privC1CCDoFh9kfbKRHf9PZMPzH1N5vkBZWdbJxnkiZji7UfZsSXvUxU");
	bubi::HttpClient http;

	Json::Value resq_tx = GetTransactionJson(priv_key);
	http.Initialize("192.168.10.92:19333");

	while (thread->enabled()){
		int64_t start_time = utils::Timestamp::HighResolution();
		{
			utils::WriteLockGuard guardw(num_mutex);
			num++;
			resq_tx["operations"][Json::UInt(0)]["asset_amount"] = num;
		}
		bubi::HttpClient::RecvMessage rec = http.http_request(bubi::HttpClient::HTTP_POST, "getTransactionBlob",
			resq_tx.toStyledString());

		Json::Value rep;
		rep.fromCString(rec.context.c_str());
		std::string blob = rep["result"]["transaction_blob"].asString();
		std::string blobde;
		utils::decode_b16(blob, blobde);

		bubi::HttpClient::RecvMessage rec1 = http.http_request(bubi::HttpClient::HTTP_POST, "submitTransaction",
			SubmintTransaction(priv_key, utils::encode_b16(priv_key.Sign(blobde)), blob).toStyledString());

		Json::Value rep2;
		std::string hash;
		rep2.fromCString(rec1.context.c_str());
		if(rep2["results"][Json::UInt(0)]["error_code"].asInt64() == 0)
		 hash = rep2["results"][Json::UInt(0)]["hash"].asString();
		else{
			std::cout << rep2;
		}

		utils::WriteLockGuard guardw(hashs_mutex);
		hashs.push_back(hash);

		int64_t end_time = utils::Timestamp::HighResolution();

		//std::cout << "Sub One Tx Spend time:" << (end_time - start_time)  << std::endl;
	}
}

class  CheckTxWorker :
	public utils::Runnable{
public:
	CheckTxWorker(int64_t tx_start_count){  tx_start_count_ = tx_start_count; count = 0; }
	~CheckTxWorker(){}
	CheckTxWorker(const CheckTxWorker& s) = delete;
	
	virtual void Run(utils::Thread *thread) override;

private:
	ApiTestWorker *api_test_worker_;
	int64_t count;
	int64_t tx_start_count_;
	std::vector<utils::Thread *> threads_;
};

void CheckTxWorker::Run(utils::Thread *thread){
	int64_t start_time = utils::Timestamp::HighResolution();
	int64_t time_out = 10 * utils::MICRO_UNITS_PER_SEC;

	bubi::HttpClient http;
	http.Initialize("192.168.10.92:19333");

	while (thread->enabled()){
			std::string hash;
			{
				utils::WriteLockGuard guardw(hashs_mutex);
				if (hashs.size() > 0){
					hash = hashs.front();
					hashs.pop_front();
					count++;
				}
			}

			if (!hash.empty()){
				CheckTransaction(20, hash, http);
				//std::cout << "hash:" << hash << std::endl;
				if (utils::Timestamp::HighResolution() - start_time > time_out){

					bubi::HttpClient::RecvMessage rec = http.http_request(bubi::HttpClient::HTTP_GET, "getModulesStatus", "");
					Json::Value rep;
					rep.fromCString(rec.context.c_str());
					
					std::cout << "success tx:" << rep["ledger manager"]["ledger_manager.tx_count"].asInt64() - tx_start_count_ << ";  txcount:" << count << std::endl;
					start_time = utils::Timestamp::HighResolution();
				}
			}
			
	}
}

TEST_F(ApiTest, ApiGetTransactionBlob){
	hashs.clear();
	std::vector<utils::Thread *> threads;

	bubi::HttpClient http;
	http.Initialize("192.168.10.92:19333");

	int64_t tx_start_count;

	bubi::HttpClient::RecvMessage rec = http.http_request(bubi::HttpClient::HTTP_GET, "getModulesStatus", "");
	Json::Value rep;
	rep.fromCString(rec.context.c_str());

	tx_start_count = rep["ledger manager"]["ledger_manager.tx_count"].asInt64();

	utils::Thread* worker_thread = new utils::Thread(new CheckTxWorker(tx_start_count));
	worker_thread->Start(utils::String::Format("checkTx"));
	threads.push_back(worker_thread);

	for (int32_t i = 0; i < 1;i++){
		utils::Thread* worker_thread = new utils::Thread(new ApiTestWorker());
		std::string work_name = "-work-" + utils::String::ToString(i);
		if (!worker_thread->Start(utils::String::Format(work_name.c_str())))
		{
		}
		threads.push_back(worker_thread);
	}

	while (true){
	  utils::Sleep(1);
	}
};
#endif
