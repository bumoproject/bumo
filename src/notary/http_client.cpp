#include <notary/http_client.h>

size_t writeFunction(void *ptr, size_t size, size_t nmemb, std::string* data) {
	data->append((char*)ptr, size * nmemb);
	return size * nmemb;
}

//http client for get
std::string HttpGet(std::string url){
	std::string response_string;
	CURL *curl = curl_easy_init();
	if (!curl) {
		fprintf(stderr, "curl_easy_init() failed\n");
		return "";
	}

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunction);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15);

	// Execute
	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		LOG_ERROR("Url:%s, curl_easy_perform() failed: %s\n", url.c_str(), curl_easy_strerror(res));
	}

	curl_easy_cleanup(curl);
	return response_string;
}

struct PostData {
	const char *ptr;
	size_t size;
};

//http client for post
std::string HttpPost(const std::string &url, const std::string &postData){

	std::string response_string;

	// Get a curl object
	CURL *curl = curl_easy_init();
	if (!curl) {
		fprintf(stderr, "curl_easy_init() failed\n");
		return "";
	}

	// Set url
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

	// Set HTTP method to POST
	curl_easy_setopt(curl, CURLOPT_POST, 1L);

	// Set callback on sending data
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());

	// Set callback on receiving data
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunction);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15);

	// Execute
	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		LOG_ERROR("Url:%s, curl_easy_perform() failed: %s\n", url.c_str(), curl_easy_strerror(res));
	}
	curl_easy_cleanup(curl);

	return response_string;
}
