// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2019 Bitcoin Association
// Distributed under the Open BSV software license, see the accompanying file LICENSE.

#ifndef BITCOIN_RPCCLIENT_H
#define BITCOIN_RPCCLIENT_H

#include "json.hpp"
#include <functional>
#include "db_mysql.h"

using json = nlohmann::json;
// Exit codes are EXIT_SUCCESS, EXIT_FAILURE, CONTINUE_EXECUTION 
static const int CONTINUE_EXECUTION = -1;  

json CallRPC(const std::string &strMethod, const json &params); 

//
// Exception thrown on connection error.  This error is used to determine when
// to wait if -rpcwait is given.
//
class CConnectionFailed : public std::runtime_error {
public:
    explicit inline CConnectionFailed(const std::string &msg)
        : std::runtime_error(msg) {}
};

struct HTTPReply 
{
    HTTPReply() : status(0), error(-1) {}

    int status;
    int error;
    std::string body;
};

static const char DEFAULT_RPCCONNECT[] = "127.0.0.1";
static const int DEFAULT_HTTP_CLIENT_TIMEOUT = 900;
static const bool DEFAULT_NAMED = false;

class ClientRpc
{
public:
	ClientRpc()
	{}
	~ClientRpc()
	{}

	enum Node
	{
		BTC = 1,
		ETH = 2,
		XRP = 3,
		EOS = 4,
		USDT = 5		
	};

	enum Token
	{
        OMNI_USDT = 1,
		ERC20_USDT = 2
	};

	struct NodeInfo
	{
		int node_id;
		std::string node_url;
		std::string node_auth;
		std::string wallet_url;
		std::string wallet_auth;
	};
	
    struct TokenInfo
    {
        int coin_id;
        int token_id;
        std::string short_name;
        std::string contract;
        int decimal;
    };

public:
    static bool init();

	bool transfer(const json& json_request, json& json_response);
 
	bool getNewAddress(const std::string& coin_name, const size_t& address_quantity);

protected:
	bool getNodeInfo(const std::string& coin_name, bool is_token, NodeInfo& node, DBMysql& db_mysql);

    bool createAddressLocalChain(const NodeInfo& node, std::string& address, std::string& prikey);

	bool createAddressOffline(const NodeInfo& node, std::string& address, std::string& privkey);

	bool setPrivateKey(const std::string& address, const std::string& private_key);

	bool getPrivateKey(const std::string& address, std::string& private_key); 

protected:

	bool transferBtc(const NodeInfo& node, const json& json_request, json& json_response);

	bool transferEos(const NodeInfo& node, const json& json_request, json& json_response);
	
    bool transferEth(const NodeInfo& node, const json& json_request, json& json_response);
	
    bool transferXrp(const NodeInfo& node, const json& json_request, json& json_response);
	
    bool transferErc20Usdt(const NodeInfo& node, const TokenInfo& tokenInfo, const json& json_request, json& json_response);
	
    bool transferOmniUsdt(const NodeInfo& node, const TokenInfo& tokenInfo, const json& json_request, json& json_response); 

protected:
	static std::map<int, NodeInfo> s_coinid_NodeInfo_;
	static std::map<int, int> s_tokenid_coinid_;
	static std::map<std::string, int> s_tokenName_tokenid_;
	static std::map<std::string, int> s_coinName_coinid_;
    static std::map<int, TokenInfo> s_tokenid_TokenInfo_;
    static std::string xrp_base_address_;
    static std::string eos_base_address_;
    static uint32_t max_xrp_tag_;
    static uint32_t max_eos_tag_;
	static std::string fee_address_omni_;
	static std::string eos_public_key_;
	
};

#endif // BITCOIN_RPCCLIENT_H
