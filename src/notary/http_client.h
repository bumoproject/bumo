#ifndef HTTP_CLIENT_H_
#define HTTP_CLIENT_H_

#include <curl/curl.h>
#include <utils/headers.h>

std::string HttpGet(std::string url);

std::string HttpPost(const std::string &url, const std::string &post_data);

#endif
