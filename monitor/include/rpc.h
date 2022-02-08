#ifndef RPC_H_
#define RPC_H_
#include "common.h"
#include <iostream>

class Rpc
{
public:
    Rpc()
    {
    }

    ~Rpc()
    {
    }

	enum  RpcError
	{
		RPC_OK = 0,
		RPC_NODEDOWN = 1,
		RPC_INTERNAL = 2
	};

public:
   
    bool getBlockCount(uint64_t& height);

	bool getBlock(const uint64_t& height, json& json_block);

	bool getRawTransaction(const std::string& txid, json& json_tx);

	bool getRawMempool(json& json_mempool);

	RpcError callRpc(const std::string& url, const std::string& auth,const json& json_post, json& json_response);

public:

    bool structRpc(const std::string& method, const json& json_params, json& json_post);	

};
#endif // RPC_H

