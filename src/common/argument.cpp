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

#include <utils/logger.h>
#include "general.h"
#include "private_key.h"
#include "key_store.h"
#include "argument.h"
#include <sstream>

namespace bumo {
	bool g_enable_ = true;
	bool g_ready_ = false;
	Argument::Argument() :
		help_modle_(false),
		drop_db_(false),
		peer_addr_(false),
		clear_peer_addresses_(false),
		clear_consensus_status_(false),
		log_dest_(-1),
		console_(false),
		create_hardfork_(false){}
	Argument::~Argument() {}

	bool Argument::Parse(int argc, char *argv[]) {
		if (argc > 1) {
			std::string s(argv[1]);
            if (s == "--console-with-cmd") {
                static std::set<std::string> support_cmd = {
                    "--sign-data",
                    "--sign-data-with-keystore",
                    "--check-address",
                    "--check-keystore",
                    "--check-signed-data",
                    "--get-address",
                    "--get-address-from-pubkey",
                    "--get-privatekey-from-keystore",
                    "--create-account",
                    "--create-keystore",
                    "--create-keystore-from-privatekey"
                };

                std::cout << "enter console command mode" << std::endl;

                do {
                    std::string input;
                    std::getline(std::cin, input);
                    utils::StringVector args_vec = utils::String::Strtok(input, ' ');
                    args_vec.insert(args_vec.begin(), utils::String::BinToHexString(argv[0]));
                    if (args_vec.size() < 1 || args_vec.size() > 9) {
                        std::cout << "error" << std::endl;
                        break;
                    }

                    char* tmp_argv[10];
                    for (size_t i = 0; i < args_vec.size(); i++) {
                        args_vec[i] = utils::String::HexStringToBin(args_vec[i]);
                        tmp_argv[i] = (char *)args_vec[i].c_str();
                    }

                    if (strcmp(tmp_argv[0], "exit") == 0) {
                        std::cout << "exit" << std::endl;
                        break;
                    }

                    //Search for the user input command in the command set.
                    if (support_cmd.find(tmp_argv[1]) == support_cmd.end()) {
                        std::cout << "error, command not found" << std::endl;
                        break;
                    }

                    Parse(args_vec.size(), tmp_argv);

                } while (true);
                return true;
            }
			else if (s == "--dropdb") {
				drop_db_ = true;
			}
			else if (s == "--console") {
				console_ = true;
			}
			else if (s == "--hardware-address") {
				ShowHardwareAddress();
				return true;
			}
			else if (s == "--clear-peer-addresses") {
				clear_peer_addresses_ = true;
			}
			else if (s == "--aes-crypto") {
				if (argc > 2) {
					printf("%s\n", (utils::Aes::CryptoHex(argv[2], bumo::GetDataSecuretKey())).c_str());
				}
				else {
					printf("missing parameter, need crypto value \n");
				}
				return true;
			}
			//else if (s == "--aes-decrypt"  && argc > 2) {
			//	printf("%s\n", utils::Aes::HexDecrypto(argv[2], bumo::GetDataSecuretKey()).c_str());
			//	return true;
			//}
			else if (s == "--sm3" ) {
				if (argc > 2) {
					printf("%s\n", utils::String::BinToHexString(utils::Sm3::Crypto(argv[2])).c_str());
				}
				else {
					printf("missing parameter, need crypto value \n");
				}
				return true;
			}
			else if (s == "--sm3-hex" ) {
				if (argc > 2) {
					printf("%s\n", utils::String::BinToHexString(utils::Sm3::Crypto(utils::String::HexStringToBin(argv[2]))).c_str());
				}
				else {
					printf("missing parameter, need crypto value with hex format \n");
				}
				return true;
			}
			else if (s == "--peer-address") {
				ShowNodeId(argc, argv);
				return true;
			}
			else if (s == "--clear-consensus-status") {
				clear_consensus_status_ = true;
			}
			else if (s == "--create-hardfork") {
				create_hardfork_ = true;
			}
			else if (s == "--version") {
				std::string sk_hash = utils::String::BinToHexString(bumo::HashWrapper::Crypto(bumo::GetDataSecuretKey())).substr(0, 2);
#ifdef SVNVERSION
				printf("%s;%u;secure:%s;git:" SVNVERSION "\n", General::BUMO_VERSION, General::LEDGER_VERSION, sk_hash.c_str());
#else
				printf("%s;%u;secure:%s\n", General::BUMO_VERSION, General::LEDGER_VERSION, sk_hash.c_str());
#endif 
				return true;
			}
			else if (s == "--help") {
				Usage();
				return true;
			}
			else if (s == "--check-signed-data" && argc > 4) {
				printf("%s\n", PublicKey::Verify(utils::String::HexStringToBin(argv[2]), utils::String::HexStringToBin(argv[3]), argv[4]) ? "true" : "false");
				return true;
			}
			else if (s == "--check-address" && argc > 2) {
				printf("%s\n", PublicKey::IsAddressValid(argv[2]) ? "ok" : "error");
				return true;
			}
			else if (s == "--sign-data" && argc > 3) {
				PrivateKey priv_key(argv[2]);
				std::string public_key = priv_key.GetEncPublicKey();
				std::string raw_data = utils::String::HexStringToBin(argv[3]);
				Json::Value result = Json::Value(Json::objectValue);
				
				result["data"] = argv[3];
				result["public_key"] = public_key;
				result["sign_data"] = utils::String::BinToHexString(priv_key.Sign(raw_data));
				printf("%s\n", result.toStyledString().c_str());
				return true;
			}
			else if (s == "--get-address" && argc > 2) {
				PrivateKey priv_key(argv[2]);
				std::string public_key = priv_key.GetEncPublicKey();
				std::string private_key = priv_key.GetEncPrivateKey();
				std::string public_address = priv_key.GetEncAddress();
				Json::Value result = Json::Value(Json::objectValue);

				result["public_key"] = public_key;
				result["private_key"] = private_key;
				result["private_key_aes"] = utils::Aes::CryptoHex(private_key, bumo::GetDataSecuretKey());
				result["address"] = public_address;
				result["public_key_raw"] = EncodePublicKey(priv_key.GetRawPublicKey());
				result["sign_type"] = GetSignTypeDesc(priv_key.GetSignType());
				printf("%s\n", result.toStyledString().c_str());
				return true;
			}

			else if (s == "--get-address-from-pubkey" && argc > 2) {
				bumo::PublicKey pub_key(argv[2]);
				std::string public_key_str = pub_key.GetEncPublicKey();
				std::string public_address = pub_key.GetEncAddress();
				Json::Value result = Json::Value(Json::objectValue);

				result["public_key"] = public_key_str;
				result["address"] = public_address;
				result["public_key_raw"] = EncodePublicKey(pub_key.GetRawPublicKey());
				result["sign_type"] = GetSignTypeDesc(pub_key.GetSignType());
				printf("%s\n", result.toStyledString().c_str());
				return true;
			}
			else if (s == "--create-account") {
				SignatureType type = SIGNTYPE_ED25519;
				if (argc > 2) {
					type = GetSignTypeByDesc(argv[2]);
					if (type == SIGNTYPE_NONE) {
						printf("parameter \"%s\" error, support ed25519 \n", argv[2]);
						return true;
					}
				}

				PrivateKey priv_key(type);
				if (!priv_key.IsValid()) {
					printf("Generate private key error");
					return true;
				}

				std::string public_key = priv_key.GetEncPublicKey();
				std::string private_key = priv_key.GetEncPrivateKey();
				std::string public_address = priv_key.GetEncAddress();

				LOG_TRACE("Creating account address:%s", public_address.c_str());
				Json::Value result = Json::Value(Json::objectValue);
				result["public_key"] = public_key;
				result["private_key"] = private_key;
				result["private_key_aes"] = utils::Aes::CryptoHex(private_key, bumo::GetDataSecuretKey());
				result["address"] = public_address;
				result["public_key_raw"] = EncodePublicKey(priv_key.GetRawPublicKey());
				result["sign_type"] = GetSignTypeDesc(priv_key.GetSignType());
				printf("%s\n", result.toStyledString().c_str());
				return true;
			}
			else if (s == "--create-keystore") {
				std::string password;
				if (argc <= 2) {
					password = utils::GetCinPassword("input the password:");
					std::cout << std::endl;
					if (password.empty()) {
						std::cout << "error, empty" << std::endl;
						return true;
					}
					std::string password1 = utils::GetCinPassword("input the password again:");
					std::cout << std::endl;
					if (password != password1) {
						std::cout << "error, not match" << std::endl;
						return true;
					}
				}
				else {
					password = argv[2];
				}

				KeyStore key_store;
				std::string new_priv_key;
				Json::Value key_store_json;
				bool ret = key_store.Generate(password, key_store_json, new_priv_key);
				if (ret) {
					printf("%s", key_store_json.toFastString().c_str());
				}
				else {
					printf("error");
				}
				return true;
			}

			else if (s == "--sign-data-with-keystore-list" && argc >4 ) {
				return SignDataWithKList(argc, argv);
			}
			else if (s == "--create-keystore-list") {
				std::string path = argc > 3 ? argv[2] : "";
				int nums = argc > 3 ? utils::String::Stoi(argv[3]) : 0;
				std::string password;

				if (4 == argc) {
					password = utils::GetCinPassword("input the password:");
					std::cout << std::endl;
					if (password.empty()) {
						std::cout << "error, empty" << std::endl;
						return true;
					}
					std::string password1 = utils::GetCinPassword("input the password again:");
					std::cout << std::endl;
					if (password != password1) {
						std::cout << "error, not match" << std::endl;
						return true;
					}
				}
				else if(5 == argc){
					password = argv[4];
				}

				if (path.empty() || nums <= 0 || password.empty()){
					Usage();
					return true;
				}

				if (!utils::File::IsExist(path) && !utils::File::CreateDir(path)){
					printf("create dir false, path:%s, make sure the path exists\n", path.c_str());
					return true;
				}

				utils::File file;
				std::string address_path = utils::File::RegularPath(utils::String::Format("%s/address_list", path.c_str()));
				if (!file.Open(address_path, utils::File::FILE_M_APPEND | utils::File::FILE_M_TEXT)){
					printf("open file failure, path:%s\n", address_path.c_str());
					return true;
				}
				long long a = nums;
				int count = 0;
				while (a != 0) {
					a /= 10;
					count++;
				}
				//std::string str_count = utils::String::Format("%d", count);

				for (int i = 0; i < nums; i++){
					KeyStore key_store;
					std::string new_priv_key;
					Json::Value key_store_json;
					bool ret = key_store.Generate(password, key_store_json, new_priv_key);
					if (ret) {
						std::string format_key = utils::String::Format("%%s/%%0%dd-%%s.wallet", count);
						std::string format_address = utils::String::Format("%%0%dd-%%s%%s", count);
						std::string key_path = utils::File::RegularPath(utils::String::Format(format_key.c_str(), path.c_str(), i + 1, key_store_json["address"].asString().c_str()));
						std::string key_store_result = key_store_json.toFastString();

						std::string address = utils::String::Format(format_address.c_str(), i + 1, key_store_json["address"].asString().c_str(),
#ifdef WIN32
							"\n"
#else
							"\r\n"
#endif // DEBUG 
							);

						if (address.size() != file.Write(address.c_str(), 1, address.size())){
							printf("write file failure, path:%s\n", path.c_str());
						}

						utils::File file_list;
						if (!file_list.Open(key_path, utils::File::FILE_M_WRITE | utils::File::FILE_M_TEXT)){
							printf("open file failure, path:%s\n", key_path.c_str());
							return true;
						}

						if (key_store_result.size() != file_list.Write(key_store_result.c_str(), 1, key_store_result.size())){
							printf("write file failure, path:%s\n", path.c_str());
						}

						file_list.Close();
					}
					else {
						printf("error");
					}
				}
				file.Close();
				return true;
			}
			else if (s == "--create-keystore-from-privatekey") {
				std::string password;
				if (argc <= 3) {
					password = utils::GetCinPassword("input the password:");
					std::cout << std::endl;
					if (password.empty()) {
						std::cout << "error, empty" << std::endl;
						return true;
					}
					std::string password1 = utils::GetCinPassword("input the password again:");
					std::cout << std::endl;
					if (password != password1) {
						std::cout << "error, not match" << std::endl;
						return true;
					}
				}
				else {
					password = argv[3];
				}

				PrivateKey key(argv[2]);
				if (!key.IsValid()) {
					printf("error, private key not valid");
					return true;
				} 

				KeyStore key_store;
				std::string new_priv_key = argv[2];
				Json::Value key_store_json;
				bool ret = key_store.Generate(password, key_store_json, new_priv_key);
				if (ret) {
					printf("%s", key_store_json.toFastString().c_str());
				}
				else {
					printf("error");
				}
				return true;
			}
			else if (s == "--check-keystore" && argc > 3) {
				KeyStore key_store;
				Json::Value key_store_json;
				key_store_json.fromString(argv[2]);
				std::string private_key;
				bool ret = key_store.From(key_store_json, argv[3], private_key);
				if (ret) {
					printf("ok");
				}
				else {
					printf("error");
				}
				return true;
			}
			else if (s == "--get-privatekey-from-keystore" && argc > 3) {
				KeyStore key_store;
				Json::Value key_store_json;
				key_store_json.fromString(argv[2]);
				std::string private_key;
				bool ret = key_store.From(key_store_json, argv[3], private_key);
				if (ret) {
					printf("%s", private_key.c_str());
				}
				else {
					printf("error");
				}
				return true;
			}
			else if (s == "--sign-data-with-keystore" && argc > 4) {
				KeyStore key_store;
				Json::Value key_store_json;
				key_store_json.fromString(argv[2]);
				std::string private_key;
				bool ret = key_store.From(key_store_json, argv[3], private_key);
				if (ret) {
					printf("%s", key_store_json.toFastString().c_str());
				}
				else {
					printf("error");
				}

				PrivateKey priv_key(private_key);
				std::string public_key = priv_key.GetEncPublicKey();
				std::string raw_data = utils::String::HexStringToBin(argv[4]);
				Json::Value result = Json::Value(Json::objectValue);

				result["data"] = argv[4];
				result["public_key"] = public_key;
				result["sign_data"] = utils::String::BinToHexString(priv_key.Sign(raw_data));
				printf("%s\n", result.toStyledString().c_str());
				return true;
			}
			else if (s == "--log-dest" && argc > 2) {
				utils::StringVector dests, levels;
				log_dest_ = utils::LOG_DEST_NONE;
				dests = utils::String::Strtok(argv[2], '+');

				for (auto &dest : dests) {
					std::string destitem = utils::String::ToUpper(dest);

					if (destitem == "ALL")         log_dest_ = utils::LOG_DEST_ALL;
					else if (destitem == "STDOUT") log_dest_ |= utils::LOG_DEST_OUT;
					else if (destitem == "STDERR") log_dest_ |= utils::LOG_DEST_ERR;
					else if (destitem == "FILE")   log_dest_ |= utils::LOG_DEST_FILE;
				}
				return false;
			}
			else if ( s == "--force-ledger-seq") {
				std::string ledger_db_path = utils::String::Format("%s/data/ledger.db", utils::File::GetBinHome().c_str());
				std::string account_db_path = utils::String::Format("%s/data/account.db", utils::File::GetBinHome().c_str());

				KeyValueDb* ledger_db = nullptr;
				KeyValueDb* account_db = nullptr;
#ifdef WIN32
				ledger_db = new LevelDbDriver();
				account_db = new LevelDbDriver();
#else
				ledger_db = new RocksDbDriver();
				account_db = new RocksDbDriver();
#endif
				if (!ledger_db->Open(ledger_db_path, -1)) {
					printf("%s", ledger_db->error_desc().c_str());
					return true;
				}

				if (!account_db->Open(account_db_path, -1)) {
					printf("%s", account_db->error_desc().c_str());
					return true;
				}

				std::string ledger_db_seq;
				std::string account_db_seq;
				if (!ledger_db->Get(General::KEY_LEDGER_SEQ, ledger_db_seq)) {
					printf("Failed to get ledger seq from ledger-db(%s)\n", ledger_db_path.c_str());
					return true;
				}

				if (!account_db->Get(General::KEY_LEDGER_SEQ, account_db_seq)) {
					printf("Failed to get ledger seq from account-db(%s)\n", account_db_path.c_str());
					return true;
				}

				int64_t int_ledger_db_seq = utils::String::Stoi64(ledger_db_seq);
				int64_t int_account_db_seq = utils::String::Stoi64(account_db_seq);
				
				if (int_account_db_seq != int_ledger_db_seq - 1) {
					printf("Error, ledger seq (%s) from ledger-db not equal with seq (%s) + 1 from account-db\n",
						ledger_db_seq.c_str(), account_db_seq.c_str());
					return true;
				}

				printf("Input y to continue(ledger db seq(" FMT_I64 "), account db seq(" FMT_I64 "):",
					int_ledger_db_seq, int_account_db_seq);
				char ch;
				std::cin >> ch;

				if (ch != 'y'){
					return true;
				}

				if (!ledger_db->Put(General::KEY_LEDGER_SEQ, account_db_seq)) {
					printf("Failed to get ledger seq from account-db\n");
					return true;
				}

				printf("Set ledger seq to " FMT_I64 " successfully", int_account_db_seq);
				return true;
			}
			else if (s == "--dbtool") {
				printf("input database path:\n");
				std::string path;
				std::cin >> path;
				KeyValueDb* ledger_db_ = nullptr;
#ifdef WIN32
				ledger_db_ = new LevelDbDriver();
#else
				ledger_db_ = new RocksDbDriver();
#endif
				if (!ledger_db_->Open(path, -1)) {
					return false;
				}

				printf("1:list all key and values\n");
				printf("2:query one key\n");
				char ch;
				std::cin >> ch;
				if (ch == '1'){
#ifdef WIN32
					auto it = (leveldb::Iterator*)ledger_db_->NewIterator();
#else
					auto it = (rocksdb::Iterator*)ledger_db_->NewIterator();
#endif
					for (it->SeekToFirst(); it->Valid(); it->Next()){
						printf("%s:%s\n", utils::String::BinToHexString(it->key().ToString()).c_str(),
							utils::String::BinToHexString(it->value().ToString()).c_str());
					}
				}
				else if (ch == '2')
					while (true){
						printf("\ninput key(hex):");
						std::string hexkey, buff;
						std::cin >> hexkey;
						auto binkey = utils::String::HexStringToBin(hexkey);
						if (ledger_db_->Get(binkey, buff)){
							printf("%s", utils::String::BinToHexString(buff).c_str());
						}
						else{
							printf("%s", ledger_db_->error_desc().c_str());
						}
					}
				return true;
			}
			else {
				Usage();
				return true;
			}
		}

		return false;
	}

	void Argument::Usage() {
		printf(
			"Usage: bumo [OPTIONS]\n"
			"OPTIONS:\n"
			"  --dropdb                                                      clean up database\n"
			"  --peer-address <node-priv-key>                                get peer address from crypted node private key\n"
			"  --create-account [crypto]                                     create account, support ed25519\n"
			"  --get-address <node-priv-key>                                 get address from private key\n"
			"  --get-address-from-pubkey <public-key>                        get address from public key\n"
			"  --sign-data <node-priv-key> <blob data>                       sign blob data\n"
			"  --check-signed-data <blob data> <signed data> <public key>    check signed data\n"
			"  --check-address <address>                                     check address\n"
			"  --hardware-address                                            get local hardware address\n"
			"  --clear-consensus-status                                      delete consensus status\n"
			"  --sm3 <arg>                                                   generate sm3 hash \n"
			"  --sm3-hex <arg>                                               generate sm3 hash from hex format \n"
			"  --aes-crypto <value>                                          crypto value\n"
			"  --version                                                     display version information\n"
			"  --create-hardfork                                             create hard fork ledger\n"
			"  --clear-peer-addresses                                        clear peer list\n"
			"  --create-keystore <password>                                  create key store\n"
			"  --create-keystore-list <path> <nums> <password>               create a number of keystores into path with same password\n"
			"  --create-keystore-from-privatekey <private key> <password>    create key store from private key\n"
			"  --sign-data-with-keystore <keystore> <password> <blob data>   sign blob data with keystore\n"
			"  --sign-data-with-keystore-list <data-file-path> <keystore-dir-path> <start-id> <password>    sign blob data with keystore list\n"
			"  --check-keystore <keystore> <password>                        check password match the keystore\n"
			"  --get-privatekey-from-keystore <keystore> <password>          check password match the keystore\n"
			"  --force-ledger-seq                                            make the ledge-seq equal\n"
			"  --log-dest <dest>                                             set log dest, LIKE FILE+STDOUT+STDERR\n"
			"  --help                                                        display this help\n"
			);
	}

	bool Argument::SignDataWithKList(int argc, char *argv[]) {
		std::string data_path = argc > 2 ? argv[2] : "";
		std::string keystore_dir_path = argc > 3 ? argv[3] : "";
		int start_id = argc > 4 ? utils::String::Stoi(argv[4]) : 0;
		std::string password;

		if (5 == argc) {
			password = utils::GetCinPassword("input the password:");
			std::cout << std::endl;
			if (password.empty()) {
				std::cout << "error, empty" << std::endl;
				return true;
			}
		}
		else if (6 == argc) {
			password = argv[5];
		}

		if (data_path.empty() || keystore_dir_path.empty() || password.empty()) {
			Usage();
			return true;
		}

		//open source file
		utils::File file_data;
		if (!file_data.Open(data_path, utils::File::FILE_M_READ | utils::File::FILE_M_BINARY)) {
			printf("Failed to open file, path:%s\n", data_path.c_str());
			return true;
		}
		std::string data_content;
		file_data.ReadData(data_content, 1024000);

		//check keystore file path
		if (!utils::File::IsExist(keystore_dir_path)) {
			printf("Keystore file not exist, path:%s, make sure the path exists\n", keystore_dir_path.c_str());
			return true;
		}

		//retrieve all keystore from directory
		std::map<int32_t, Json::Value> keystore_list;
		utils::FileAttributes nFiles;
		utils::File::GetFileList(keystore_dir_path, nFiles, true);
		for (utils::FileAttributes::iterator itr = nFiles.begin(); itr != nFiles.end(); itr++) {
			const std::string &strName = itr->first;
			utils::FileAttribute &nAttr = itr->second;

			if (nAttr.is_directory_) {
				continue;
			}

			size_t line_pos = strName.find("-");
			if (line_pos == std::string::npos &&  strName.find("wallet") == std::string::npos) {
				continue;
			}
			int32_t id = utils::String::Stoi(strName.substr(0, line_pos));
			std::string file_content;
			utils::File file_keystore;
			std::string dest_file_path = utils::String::Format("%s/%s", keystore_dir_path.c_str(), strName.c_str());
			if (!file_keystore.Open(dest_file_path, utils::File::FILE_M_READ | utils::File::FILE_M_BINARY)) {
				printf("Failed to open keystore file, path:%s\n", dest_file_path.c_str());
				return true;
			}
			file_keystore.ReadData(file_content, 10240);
			Json::Value json_file_content;
			json_file_content.fromString(file_content);
			keystore_list[id] = json_file_content;
		}


		//parse from source file
		Json::Value json_data = Json::Value(Json::objectValue);
		if (!json_data.fromString(data_content)) {
			printf("Failed to parse from file, path:%s\n", data_path.c_str());
			return true;
		}

		Json::Value &json_items = json_data["items"];
		for (size_t i = 0; i < json_items.size(); i++) {
			Json::Value &item = json_items[i];
			std::string blob_str = item["transaction_blob"].asString();
			KeyStore keystore;
			std::string str_priv_key;
			Json::Value temp_ks = keystore_list[start_id + i];
			if (!keystore.From(temp_ks, password, str_priv_key)) {
				printf("Failed to parse from key store file\n");
				return true;
			}
			bumo::PrivateKey priv_key(str_priv_key);
			std::string sign_data = priv_key.Sign(utils::String::HexStringToBin(blob_str));
			Json::Value &sign_objs = item["signatures"];
			Json::Value &sign_obj = sign_objs[sign_objs.size()];
			sign_obj["public_key"] = priv_key.GetEncPublicKey();
			sign_obj["sign_data"] = utils::String::BinToHexString(sign_data);
		}

		//create dest file
		utils::File file_dest;
		//compose dest file path
		size_t dot_pos = data_path.rfind(".");
		std::string data_dest_path = data_path.insert(dot_pos, "_1");
		if (!file_dest.Open(data_dest_path, utils::File::FILE_M_WRITE| utils::File::FILE_M_BINARY)) {
			printf("Failed to open dest file, path:%s\n", data_dest_path.c_str());
			return true;
		}
		std::string str_write = json_data.toFastString();
		file_dest.Write(str_write.c_str(), str_write.size(), 1);

		return true;
	}

	void Argument::ShowNodeId(int argc, char *argv[]) {
		if (argc < 3) {
			printf("missing parameter, need 1 parameter (the aes_crypto code of private key)\n");
			return;
		}

		if (!utils::String::IsHexString(argv[2])) {
			printf("the node_id of inputting is invalid, please check it!\n");
			return;
		}

		bumo::PrivateKey private_key(utils::Aes::HexDecrypto(argv[2], bumo::GetDataSecuretKey()));

        printf("local peer address (%s)\n", private_key.GetEncAddress().c_str());
	}

	void Argument::ShowHardwareAddress() {
		std::string hard_address = "";
		utils::System system;
		char out_msg[256] = { 0 };
		if (system.GetHardwareAddress(hard_address, out_msg))
			printf("local hardware address (%s)\n", hard_address.c_str());
		else
			printf("%s\n", out_msg);
	}

	void SignalFunc(int32_t code) {
		fprintf(stderr, "Get quit signal(%d)\n", code);
		g_enable_ = false;
	}

	void InstallSignal() {
		signal(SIGHUP, SignalFunc);
		signal(SIGQUIT, SignalFunc);
		signal(SIGINT, SignalFunc);
		signal(SIGTERM, SignalFunc);
#ifndef WIN32
		signal(SIGPIPE, SIG_IGN);
#endif
	}

}
