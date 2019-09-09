#ifndef HTTP_CLIENT_H_
#define HTTP_CLIENT_H_

#include <curl/curl.h>
#include <utils/headers.h>

//http client for get
std::string HttpGet(std::string url);

//http client for post
std::string HttpPost(const std::string &url_address, const std::string &post_data);

#endif
