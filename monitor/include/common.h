#ifndef __COMMON_H__
#define __COMMON_H__

#include <string>
#include <vector>
#include  <map>
#include "json.hpp"
using json = nlohmann::json;

struct CurlParams
{
    std::string url;
    std::string auth;
    std::string data;
    std::string content_type;
    bool need_auth;
    CurlParams()
    {
        need_auth = true;
        content_type = "content-type:application/json";
    }
};

struct NodeInfo
{
	int node_id;
	std::string node_url;
	std::string wallet_url;
	std::string node_auth;
	std::string wallet_auth;
};


bool CurlPostParams(const CurlParams& params, std::string& response);

#endif
