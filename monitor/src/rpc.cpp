#include "rpc.h"
#include <glog/logging.h>
#include <iostream>
#include <chrono>
#include <fstream>

static Rpc::RpcError CurlPost(const std::string& url, const json &json_post, const std::string& auth, json& json_response)
{
    CurlParams curl_params;
    curl_params.auth = auth;
    curl_params.url = url;
 // curl_params.content_type = "content-type:text/plain";
    curl_params.data = json_post.dump();
    std::string response;
    bool ret = CurlPostParams(curl_params,response);
	if (!ret)
		return Rpc::RPC_NODEDOWN;
	LOG(INFO) << response; 
    json_response = json::parse(response);
	if (!json_response["error"].is_null())
	{
		LOG(ERROR) << response;
		LOG(ERROR) << curl_params.data;
		return Rpc::RPC_INTERNAL;
	}
	
    return Rpc::RPC_OK;
}

Rpc::RpcError Rpc::callRpc(const std::string& url, const std::string& auth,const json& json_post, json& json_response)
{
	return CurlPost(url, json_post, auth, json_response);
}

bool Rpc::structRpc(const std::string& method, const json& json_params, json& json_post)
{
    json_post["jsonrpc"] = "1.0";
    json_post["id"] = "curltest";
    json_post["method"] = method;
    json_post["params"] = json_params;
	
    return true;
}





