// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2019 Bitcoin Association
// Distributed under the Open BSV software license, see the accompanying file LICENSE.

#include "rpc/client.h"
#include "rpc/protocol.h"
#include "util.h"
#include "support/events.h"
#include "utilstrencodings.h"
#include "db_mysql.h"
#include "ethapi.h"
#include <cstdint>
#include <set>
#include <rpc/server.h>
#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/algorithm/string/classification.hpp>
#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>
#include <boost/algorithm/string/split.hpp>
#include <libethereum/Common.h>
#include <leveldb/db.h>
#include <gmp.h>
#include <gmpxx.h>
#include <glog/logging.h>
#include <curl/curl.h>

class CRPCConvertParam {
public:
    std::string methodName; //!< method whose params want conversion
    int paramIdx;           //!< 0-based idx of param to convert
    std::string paramName;  //!< parameter name
};

static const std::string s_eos_wallet_name = "default";
static const std::string s_eos_wallet_password = "PW5KENye24Beqh4Mt2hVwFD7e7Hg1LJXQNxopkHwiXP29xfoTwGVK";

const char *http_errorstring(int code) 
{
    switch (code) 
    {
#if LIBEVENT_VERSION_NUMBER >= 0x02010300
        case EVREQ_HTTP_TIMEOUT:
            return "timeout reached";
        case EVREQ_HTTP_EOF:
            return "EOF reached";
        case EVREQ_HTTP_INVALID_HEADER:
            return "error while reading header, or invalid header";
        case EVREQ_HTTP_BUFFER_ERROR:
            return "error encountered while reading or writing";
        case EVREQ_HTTP_REQUEST_CANCEL:
            return "request was canceled";
        case EVREQ_HTTP_DATA_TOO_LONG:
            return "response body is larger than allowed";
#endif
        default:
            return "unknown";
    }
}

#if LIBEVENT_VERSION_NUMBER >= 0x02010300
static void http_error_cb(enum evhttp_request_error err, void *ctx) {
    HTTPReply *reply = static_cast<HTTPReply *>(ctx);
    reply->error = err;
}
#endif

void http_request_done(struct evhttp_request *req, void *ctx) 
{
    HTTPReply *reply = static_cast<HTTPReply *>(ctx);

    if (req == nullptr) 
    {
        /**
         * If req is nullptr, it means an error occurred while connecting: the
         * error code will have been passed to http_error_cb.
         */
        reply->status = 0;
        return;
    }

    reply->status = evhttp_request_get_response_code(req);

    struct evbuffer *buf = evhttp_request_get_input_buffer(req);
    if (buf) 
    {
        size_t size = evbuffer_get_length(buf);
        const char *data = (const char *)evbuffer_pullup(buf, size);
        if (data) reply->body = std::string(data, size);
        evbuffer_drain(buf, size);
    }
}

json CallRPC(const std::string &strMethod, const json &params) 
{
    std::string host = "127.0.0.1";
    // In preference order, we choose the following for the port:
    //     1. -rpcport
    //     2. port in -rpcconnect (ie following : in ipv4 or ]: in ipv6)
    //     3. default port for chain
    int port = 8332;

    // Obtain event base
    raii_event_base base = obtain_event_base();

    // Synchronously look up hostname
    raii_evhttp_connection evcon =
        obtain_evhttp_connection_base(base.get(), host, port);
    evhttp_connection_set_timeout(
        evcon.get(),
        DEFAULT_HTTP_CLIENT_TIMEOUT);

    HTTPReply response;
    raii_evhttp_request req =
        obtain_evhttp_request(http_request_done, (void *)&response);
    if (req == nullptr) throw std::runtime_error("create http request failed");
#if LIBEVENT_VERSION_NUMBER >= 0x02010300
    evhttp_request_set_error_cb(req.get(), http_error_cb);
#endif

    // Get credentials
    std::string strRPCUserColonPass;
  
    strRPCUserColonPass =  "dev:a" ;
    
    struct evkeyvalq *output_headers = evhttp_request_get_output_headers(req.get());
    assert(output_headers);
    evhttp_add_header(output_headers, "Host", host.c_str());
    evhttp_add_header(output_headers, "Connection", "close");
    evhttp_add_header( output_headers, "Authorization", (std::string("Basic ") + EncodeBase64(strRPCUserColonPass)).c_str());

    // Attach request data
    std::string strRequest = JSONRPCRequestObj(strMethod, params, 1).dump() + "\n";
    struct evbuffer *output_buffer = evhttp_request_get_output_buffer(req.get());
    assert(output_buffer);
    evbuffer_add(output_buffer, strRequest.data(), strRequest.size());

    // check if we should use a special wallet endpoint
    std::string endpoint = "/";
    int r = evhttp_make_request(evcon.get(), req.get(), EVHTTP_REQ_POST, endpoint.c_str());

    json json_error;
    
    // ownership moved to evcon in above call
    req.release();
    if (r != 0) 
    {
    
        json_error["code"] = 1;
        json_error["error"] = "send http request failed";
        return json_error;
        //throw CConnectionFailed("send http request failed");
    }

    event_base_dispatch(base.get());

    if (response.status == 0) 
    {

        std::string error = strprintf(
            "couldn't connect to server: %s (code %d)\n(make sure server is "
            "running and you are connecting to the correct RPC port)",
            http_errorstring(response.error), response.error);
        /*throw CConnectionFailed(strprintf(
            "couldn't connect to server: %s (code %d)\n(make sure server is "
            "running and you are connecting to the correct RPC port)",
            http_errorstring(response.error), response.error));*/

        json_error["code"] = 2;
        json_error["error"] = error;
        return json_error;

    }
    else if (response.status == HTTP_UNAUTHORIZED) 
    {
        json_error["code"] = 3;
        json_error["error"] = "incorrect rpcuser or rpcpassword (authorization failed)";
        return json_error;
        //throw std::runtime_error("incorrect rpcuser or rpcpassword (authorization failed)");
    } 
    else if (response.status >= 400 && response.status != HTTP_BAD_REQUEST &&
               response.status != HTTP_NOT_FOUND &&
               response.status != HTTP_INTERNAL_SERVER_ERROR) 
    {
        json_error["code"] = 4;
        json_error["error"] = "server returned HTTP error " + std::to_string(response.status);
        return json_error;
        
        //throw std::runtime_error(strprintf("server returned HTTP error %d", response.status));
    } 
    else if (response.body.empty()) 
    {
        json_error["code"] = 5;
        json_error["error"] = "server returned HTTP error " + std::to_string(response.status);
        return json_error;
        
       // throw std::runtime_error("no response from server");
    }

    // Parse reply
    json json_reply = json::parse(response.body);
    
    return json_reply;
}
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

static size_t ReplyCallback(void *ptr, size_t size, size_t nmemb, void *stream)
{
    std::string *str = (std::string*)stream;
    (*str).append((char*)ptr, size*nmemb);
    return size * nmemb;
}

bool CurlPostParams(const CurlParams &params, std::string &response)
{
    CURL *curl = curl_easy_init();
    struct curl_slist *headers = NULL;
    CURLcode res;
    response.clear();
    std::string error_str ;
    //error_str.clear();
    if (curl)
    {
        headers = curl_slist_append(headers, params.content_type.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_URL, params.url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)params.data.size());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, params.data.c_str());

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ReplyCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

        if(params.need_auth)
        {
            curl_easy_setopt(curl, CURLOPT_USERPWD, params.auth.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
        }

        curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_TRY);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 120);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120);
        res = curl_easy_perform(curl);
    }
	curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
    {
        error_str = curl_easy_strerror(res);
        LOG(ERROR) << "url : " << params.url;
		LOG(ERROR) << "curl error: " << error_str << std::endl;
        return false;
    }
    return true;

}

bool CallNodeRPC(const std::string &strMethod, const json &params, const std::string& url, const std::string& auth, json& json_response) 
{
    bool ret = false;

    json json_post;
    json_post["id"] = "curltest";
    json_post["jsonrpc"] = "1.0";
    json_post["params"] = params;
    json_post["method"] = strMethod;
    CurlParams rpc_post;
    rpc_post.url = url;
    rpc_post.auth = auth;
    rpc_post.data = json_post.dump();
    rpc_post.need_auth = true;
    std::string response;
    ret = CurlPostParams(rpc_post, response);
    if(ret)
        json_response = json::parse(response);
    return ret;
/*
    std::string host = "127.0.0.1";
    int port = 8332;

	std::vector<std::string> vect_url;  
	boost::split(vect_url, url, boost::is_any_of(":"));

	json json_error;
	if (vect_url.size() < 2)
	{
		json_error["error"] = "error url";
		return json_error;
	}
	host = vect_url[0];
	port = std::atoi(vect_url[1].c_str());

    // Obtain event base
    raii_event_base base = obtain_event_base();

    // Synchronously look up hostname
    raii_evhttp_connection evcon =
        obtain_evhttp_connection_base(base.get(), host, port);
    evhttp_connection_set_timeout(
        evcon.get(),
        DEFAULT_HTTP_CLIENT_TIMEOUT);

    HTTPReply response;
    raii_evhttp_request req =
        obtain_evhttp_request(http_request_done, (void *)&response);
    if (req == nullptr) throw std::runtime_error("create http request failed");
#if LIBEVENT_VERSION_NUMBER >= 0x02010300
    evhttp_request_set_error_cb(req.get(), http_error_cb);
#endif

    // Get credentials
    std::string strRPCUserColonPass;
    strRPCUserColonPass = auth ;
    struct evkeyvalq *output_headers = evhttp_request_get_output_headers(req.get());
    assert(output_headers);
    evhttp_add_header(output_headers, "Host", host.c_str());
    evhttp_add_header(output_headers, "Connection", "close");
	evhttp_add_header(output_headers, "Authorization", (std::string("Basic ") + EncodeBase64(strRPCUserColonPass)).c_str());
	// eth Need this head
    evhttp_add_header(output_headers, "Content-Type", "application/json");

    
    // Attach request data
    std::string strRequest = JSONRPCRequestObj(strMethod, params, 1).dump() + "\n";
    struct evbuffer *output_buffer = evhttp_request_get_output_buffer(req.get());
    assert(output_buffer);
    evbuffer_add(output_buffer, strRequest.data(), strRequest.size());

    // check if we should use a special wallet endpoint
    std::string endpoint = "/";
    int r = evhttp_make_request(evcon.get(), req.get(), EVHTTP_REQ_POST, endpoint.c_str());

    // ownership moved to evcon in above call
    req.release();
    if (r != 0) 
    {
        throw CConnectionFailed("send http request failed");
    }

    event_base_dispatch(base.get());

    if (response.status == 0) 
    {
        throw CConnectionFailed(strprintf(
            "couldn't connect to server: %s (code %d)\n(make sure server is "
            "running and you are connecting to the correct RPC port)",
            http_errorstring(response.error), response.error));
    }
    else if (response.status == HTTP_UNAUTHORIZED) 
    {
        throw std::runtime_error("incorrect rpcuser or rpcpassword (authorization failed)");
    } 
    else if (response.status >= 400 && response.status != HTTP_BAD_REQUEST &&
               response.status != HTTP_NOT_FOUND &&
               response.status != HTTP_INTERNAL_SERVER_ERROR) 
    {
        throw std::runtime_error(strprintf("server returned HTTP error %d", response.status));
    } 
    else if (response.body.empty()) 
    {
        throw std::runtime_error("no response from server");
    }

    // Parse reply
    json json_reply = json::parse(response.body);
    return json_reply;*/
}

static const char WALLET_DB[] = "wallet_db";
std::map<int, ClientRpc::NodeInfo> ClientRpc::s_coinid_NodeInfo_;
std::map<int, int> ClientRpc::s_tokenid_coinid_;
std::map<int, ClientRpc::TokenInfo> ClientRpc::s_tokenid_TokenInfo_;
std::map<std::string, int> ClientRpc::s_tokenName_tokenid_;
std::map<std::string, int> ClientRpc::s_coinName_coinid_;
std::string ClientRpc::xrp_base_address_;
std::string ClientRpc::eos_base_address_;
uint32_t ClientRpc::max_xrp_tag_;
uint32_t ClientRpc::max_eos_tag_;
std::string ClientRpc::fee_address_omni_;
std::string ClientRpc::eos_public_key_;

bool ClientRpc::init()
{
    DBMysql db_mysql;
    bool ret = false;
    if ( !db_mysql.openDB(g_json_config["otc_db"]) )
    {
        LOG(ERROR) << "open db fail";
        return ret;
    }

    if ( g_json_config["fee_address_omni"].is_null() )
    {
        LOG(ERROR) << "omni addess has not init";
        return ret;
    }
    // init fee_address_omni_
    fee_address_omni_ = g_json_config["fee_address_omni"].get<std::string>();
    
    if ( g_json_config["eos_public_key"].is_null() )
    {
        LOG(ERROR) << "EOS public key has not init";
        return ret;
    }
    // init eos_public_key_
    eos_public_key_ = g_json_config["eos_public_key"].get<std::string>();


    std::map<int,DBMysql::DataType> map_col_type;
    json json_datas;
    std::string sql;
    std::map<int, int> nodeid_coinid;

    // init s_coinName_coinid_ 
    map_col_type[0] = DBMysql::INT;
    map_col_type[1] = DBMysql::STRING;
    map_col_type[2] = DBMysql::STRING;
    map_col_type[3] = DBMysql::STRING;
    map_col_type[4] = DBMysql::STRING;
    map_col_type[5] = DBMysql::STRING;
    map_col_type[6] = DBMysql::INT;
    sql = strprintf("SELECT node.node_id, coin.short_name, node_url, node_auth, wallet_url, wallet_auth ,coin_id FROM node, coin WHERE node.node_id = coin.node_id;");
    
    db_mysql.getData(sql, map_col_type, json_datas);
    int node_id = 0, coin_id = 0;
    NodeInfo node;
    for ( size_t i = 0; i < json_datas.size(); ++i )
    {
        coin_id = json_datas[i][6].get<int>();
        node_id = json_datas[i][0].get<int>();
        s_coinName_coinid_[json_datas[i][1].get<std::string>()] = coin_id;
        node.node_id = node_id;
        node.node_url = json_datas[i][2].get<std::string>();
        node.node_auth = json_datas[i][3].get<std::string>();
        node.wallet_url = json_datas[i][4].get<std::string>();
        node.wallet_auth = json_datas[i][5].get<std::string>();
        s_coinid_NodeInfo_[coin_id] = node;     

    }
    // init s_tokenid_TokenInfo_ and s_tokenid_coinid_ and s_tokenName_tokenid_
    map_col_type.clear();
    map_col_type[0] = DBMysql::INT;
    map_col_type[1] = DBMysql::INT;
    map_col_type[2] = DBMysql::STRING;
    map_col_type[3] = DBMysql::STRING;
    map_col_type[4] = DBMysql::INT;
    
    sql = strprintf("SELECT coin_id, token_id, short_name, contract, `decimal` FROM token;");
    json_datas.clear();
    db_mysql.getData(sql, map_col_type, json_datas);
    int token_id = 0;
    for ( size_t i = 0; i < json_datas.size(); ++i )
    {
        TokenInfo token;
        token_id = json_datas[i][1].get<int>(); 
        coin_id = json_datas[i][0].get<int>();
        token.coin_id = coin_id;
        token.token_id = token_id;

        token.short_name = json_datas[i][2].get<std::string>();
        token.contract = json_datas[i][3].get<std::string>();
        token.decimal = json_datas[i][4].get<int>();

        s_tokenid_TokenInfo_[token_id] = token;
        s_tokenid_coinid_[token_id] = coin_id;
        s_tokenName_tokenid_[token.short_name] = token_id;
    }
    //LOG(INFO) << "INIT s_tokenid_coinid_: " << json(s_tokenid_coinid_); 
    //LOG(INFO) << "INIT s_tokenid_TokenInfo_: " << json(s_tokenid_TokenInfo_); 
    //LOG(INFO) << "INIT s_tokenName_tokenid_: " << json(s_tokenName_tokenid_); 

    // init max_xrp_tag_ and  xrp_base_address_
    if ( s_coinName_coinid_.find("XRP") != s_coinName_coinid_.end() )
    {
        map_col_type.clear();
        map_col_type[0] = DBMysql::STRING;
        map_col_type[1] = DBMysql::STRING;
        int coin_id = s_coinName_coinid_["XRP"];
        //sql = strprintf("SELECT tag, address FROM account a where a.account_id in (SELECT max(account_id) FROM account WHERE a.coin_id = %d);", coin_id);
        sql = strprintf("SELECT tag, address FROM account a where a.account_id = (SELECT max(account_id) FROM account b WHERE b.coin_id = %d) and a.coin_id = %d;", coin_id, coin_id);
        json_datas.clear();
        db_mysql.getData(sql, map_col_type, json_datas);
        if ( json_datas.size() > 0 )
        {   
            uint32_t tag = std::stoul(json_datas[0][0].get<std::string>());
            if ( tag > 0 )
            {
                max_xrp_tag_ = tag;
                xrp_base_address_ = json_datas[0][1].get<std::string>();
            }
            else
            {
                max_xrp_tag_ = 0;
                xrp_base_address_ = "";
            }
        }
        //LOG(INFO) << "INIT max_xrp_tag_: " << max_xrp_tag_; 
        //LOG(INFO) << "INIT xrp_base_address_: " << xrp_base_address_; 
    }

    ret = true;
    // init max_eos_tag_ and eos_base_address_
    if ( s_coinName_coinid_.find("EOS") != s_coinName_coinid_.end() )
    {
        map_col_type.clear();
        map_col_type[0] = DBMysql::STRING;
        map_col_type[1] = DBMysql::STRING;
        int coin_id = s_coinName_coinid_["EOS"];
        // sql = strprintf("SELECT tag, address FROM account a where a.account_id in (SELECT max(account_id) FROM account WHERE a.coin_id = %d);", coin_id);
        sql = strprintf("SELECT tag, address FROM account a where a.account_id = (SELECT max(account_id) FROM account b WHERE b.coin_id = %d) and a.coin_id = %d;", coin_id, coin_id);
        json_datas.clear();
        db_mysql.getData(sql, map_col_type, json_datas);
        if ( json_datas.size() > 0 )
        {   
            uint32_t tag = std::stoul(json_datas[0][0].get<std::string>());
            max_eos_tag_ = tag;
            eos_base_address_ = json_datas[0][1].get<std::string>();
        }
        else
        {
            LOG(ERROR) << "Must init eos address in db";
            ret = false;
        }
        //LOG(INFO) << "INIT max_eos_tag_: " << max_eos_tag_; 
        //LOG(INFO) << "INIT eos_base_address_: " << eos_base_address_; 
    }
    db_mysql.closeDB();
    return ret;
}

bool ClientRpc::transfer(const json& json_request, json& json_response)
{
    LOG(INFO) << "tranfer " << json_request.dump(4);
    bool ret = false;
	DBMysql db_mysql;
	if ( !db_mysql.openDB(g_json_config["otc_db"]) )
    {
        LOG(ERROR) << "open db fail";
        return ret;
    }

	std::string coin_name = json_request[0].get<std::string>();
   	bool is_token = json_request[1].get<bool>();
	int coin_id;
    NodeInfo node;

	if ( !is_token )
	{
        if ( s_coinName_coinid_.find(coin_name) == s_coinName_coinid_.end() )
        {
            LOG(ERROR) << "coin not supported by the database: " << coin_name; 
            return ret;
        }
        coin_id = s_coinName_coinid_[coin_name];

        if ( s_coinid_NodeInfo_.find(coin_id) == s_coinid_NodeInfo_.end() )
        {
            LOG(ERROR) << "coin not supported by the database: " << coin_name; 
            return ret;
        }
        node = s_coinid_NodeInfo_[coin_id];

		if (node.node_id == BTC)
		{
            ret = transferBtc(node, json_request, json_response);
		}
		else if (node.node_id == ETH)
		{
            ret = transferEth(node, json_request, json_response);
		}
		else if ( node.node_id == XRP )
		{
            ret = transferXrp(node, json_request, json_response);
		}
		else if ( node.node_id == EOS )
		{
            ret = transferEos(node, json_request, json_response);
		}
	}
	else
	{
        if ( s_tokenName_tokenid_.find(coin_name) == s_tokenName_tokenid_.end() )
        {
            LOG(ERROR) << "coin not supported by the database: " << coin_name; 
            return ret;
        }
        int token_id = s_tokenName_tokenid_[coin_name];

        if ( s_tokenid_TokenInfo_.find(token_id) == s_tokenid_TokenInfo_.end() )
        {
            LOG(ERROR) << "coin not supported by the database: " << coin_name; 
            return ret;
        }
        TokenInfo tokenInfo = s_tokenid_TokenInfo_[token_id];

        if ( s_tokenid_coinid_.find(token_id) == s_tokenid_coinid_.end() )
        {
            LOG(ERROR) << "coin not supported by the database: " << coin_name; 
            return ret;
        }
        coin_id = s_tokenid_coinid_[token_id];

        if ( s_coinid_NodeInfo_.find(coin_id) == s_coinid_NodeInfo_.end() )
        {
            LOG(ERROR) << "coin not supported by the database: " << coin_name; 
            return ret;
        }
        node = s_coinid_NodeInfo_[coin_id];

		if ( tokenInfo.token_id == OMNI_USDT ) 
		{
            ret = transferOmniUsdt(node, tokenInfo, json_request, json_response);
		}
		else if ( tokenInfo.token_id == ERC20_USDT )
		{
            ret = transferErc20Usdt(node, tokenInfo, json_request, json_response);
		}
	}

    db_mysql.closeDB();
    return ret;
}

bool ClientRpc::getNewAddress(const std::string& coin_name, const size_t& address_quantity)
{
    LOG(INFO) << "getNewAddress " << coin_name << " quantity " << address_quantity;
    DBMysql db_mysql;
	bool ret = db_mysql.openDB(g_json_config["otc_db"]);
	if (!ret)
    {
        LOG(ERROR) << "open db fail";
		return ret;
    }

    if ( s_coinName_coinid_.find(coin_name) == s_coinName_coinid_.end() )
    {
        LOG(ERROR) << "coin not supported by the database: " << coin_name; 
        return false;
    }

	int coin_id = s_coinName_coinid_[coin_name];
    if ( s_coinid_NodeInfo_.find(coin_id) == s_coinid_NodeInfo_.end() )
    {
        LOG(ERROR) << "coin not supported by the database: " << coin_name; 
        return false;
    }

    NodeInfo node = s_coinid_NodeInfo_[coin_id];
	std::map<int,DBMysql::DataType> map_col_type;
	map_col_type[0] = DBMysql::INT;
	json json_data;
	std::string sql = strprintf("SELECT COUNT(*) FROM user WHERE coin_id = %d UNION ALL SELECT COUNT(*) FROM account WHERE coin_id = %d UNION ALL SELECT MAX(account_id) FROM account WHERE coin_id = %d;", coin_id, coin_id, coin_id);
	db_mysql.getData(sql, map_col_type, json_data);
	if (json_data.size() <= 2)
	{
        LOG(ERROR) << "SQL ERROR: " << sql;
		db_mysql.closeDB();
		return false;
	}

	size_t count_user = json_data[0][0].get<size_t>();
	size_t count_account = json_data[1][0].get<size_t>();
	size_t max_account_id = json_data[2][0].get<size_t>();
	
	if ( count_account - count_user >= address_quantity )
	{
		db_mysql.closeDB();
		return true;
	}

    std::vector<std::string> vect_sql;
    std::string address;
    std::string privkey;
    std::string tag; 
    if ( node.node_id == BTC )
    {
        tag = "BTC";
		for (size_t i = 0; i < address_quantity; i++)
		{
            if( createAddressLocalChain(node, address, privkey) )
            {
                int account_id = max_account_id + i + 1;
			    std::string sql_insert = strprintf("INSERT INTO `account` (`account_id`, `coin_id`, `address`, `tag`) VALUES ('%d', '%d', '%s', '%s');",
									account_id, coin_id, address, tag);
			    vect_sql.push_back(sql_insert);	
            }
		}
		db_mysql.batchRefreshDB(vect_sql);	
	}
	else if ( node.node_id == ETH )
	{
		tag = "ETH";
		for (size_t i = 0; i < address_quantity; i++)
		{
            if( createAddressOffline(node, address, privkey) )
            {
               
                if( !setPrivateKey(address, privkey) )
                {
                    continue;
                }
                int account_id = max_account_id + i + 1;
			    std::string sql_insert = strprintf("INSERT INTO `account` (`account_id`, `coin_id`, `address`, `tag`) VALUES ('%d', '%d', '%s', '%s');",
									account_id, coin_id, address, tag);
			    vect_sql.push_back(sql_insert);	
            }
		}
		db_mysql.batchRefreshDB(vect_sql);	
	}
	else if (node.node_id == XRP)
	{
		//std::vector<std::string> vect_sql;
		for (size_t i = 0; i < address_quantity; i++)
		{
            if( max_xrp_tag_ <= 0 )
            {
                if( !createAddressOffline(node, address, privkey) )
                {
                    break;
                }

                if(!setPrivateKey(address,privkey))
                {
                    break;
                }
                xrp_base_address_ = address;
                max_xrp_tag_ ++;
                tag = std::to_string(max_xrp_tag_);
                int account_id = max_account_id + i + 1;
			    std::string sql_insert = strprintf("INSERT INTO `account` (`account_id`, `coin_id`, `address`, `tag`) VALUES ('%d', '%d', '%s', '%s');",
									account_id, coin_id, xrp_base_address_, tag);
			    vect_sql.push_back(sql_insert);	         
            }
            else
            {
                if ( xrp_base_address_.empty() )
                    break;
                max_xrp_tag_ ++;
                tag = std::to_string(max_xrp_tag_);
                int account_id = max_account_id + i + 1;
                std::string sql_insert = strprintf("INSERT INTO `account` (`account_id`, `coin_id`, `address`, `tag`) VALUES ('%d', '%d', '%s', '%s');",
                        account_id, coin_id, xrp_base_address_, tag);
                vect_sql.push_back(sql_insert);	              
            }
        }
		db_mysql.batchRefreshDB(vect_sql);
	}
	else if (node.node_id == EOS)
	{
        for (size_t i = 0; i < address_quantity; i++)
		{
            if ( eos_base_address_.empty() )
                break;

            max_eos_tag_ ++;
            tag = std::to_string(max_eos_tag_);
            int account_id = max_account_id + i + 1;
            std::string sql_insert = strprintf("INSERT INTO `account` (`account_id`, `coin_id`, `address`, `tag`) VALUES ('%d', '%d', '%s', '%s');",
                    account_id, coin_id, eos_base_address_, tag);
            vect_sql.push_back(sql_insert);	              
            
        }
		db_mysql.batchRefreshDB(vect_sql);
	}
    else if ( node.node_id == USDT )
    {
        tag = "USDT";
		for (size_t i = 0; i < address_quantity; i++)
		{
            if( createAddressLocalChain(node, address, privkey) )
            {
                int account_id = max_account_id + i + 1;
			    std::string sql_insert = strprintf("INSERT INTO `account` (`account_id`, `coin_id`, `address`, `tag`) VALUES ('%d', '%d', '%s', '%s');",
									account_id, coin_id, address, tag);
			    vect_sql.push_back(sql_insert);	
            }
		}
		db_mysql.batchRefreshDB(vect_sql);	
    }

	db_mysql.closeDB();
	return true;
}

bool ClientRpc::setPrivateKey(const std::string& address, const std::string& private_key) 
{
	using namespace leveldb;
	std::string dbname = g_json_config[WALLET_DB].get<std::string>();

	DB *db;
	Options options;
	options.create_if_missing = true;
	Status s = DB::Open(options, dbname, &db);
	if(!s.ok())
	{
        LOG(ERROR) << "open wallet fail" ;
		delete db;
		return false;
	}
	s = db->Put(WriteOptions(), address, private_key);
	if(!s.ok())
	{
        LOG(ERROR) << "open wallet fail" ;
		delete db;
		return false;
	}
	delete db;
	return true;
}

bool ClientRpc::getPrivateKey(const std::string& address, std::string& private_key) 
{
	using namespace leveldb;
	std::string dbname = g_json_config[WALLET_DB].get<std::string>();
	DB *db;
	Options options;
	options.create_if_missing = true;
	Status s = DB::Open(options, dbname, &db);
	if(!s.ok())
	{
        LOG(ERROR) << "open wallet fail" ;
		delete db;
		return false;
	}
	
	s = db->Get(ReadOptions(), address, &private_key);

	if(!s.ok())
	{
        LOG(ERROR) << "open wallet fail" ;
		delete db;
		return false;
	}
	delete db;
	return true;
}

bool ClientRpc::transferBtc(const NodeInfo& node, const json&json_request, json& json_response)
{
  	std::string from = json_request[2].get<std::string>();
	std::string to = json_request[3].get<std::string>();
	double balance = json_request[4].get<double>();
	double fee = json_request[5].get<double>();
    bool ret = false;
    json json_params;
    json json_addresses;
    json_addresses.push_back(from);
    json_params.push_back(1);
    json_params.push_back(9999999);
    json_params.push_back(json_addresses);

    json json_reply ;
    ret = CallNodeRPC("listunspent", json_params, node.node_url, node.node_auth, json_reply);
    if(!ret)
    {
        LOG(ERROR) << "rpc call fail";
        return ret;
    }

    json json_unspent = json_reply["result"];
    json json_vins;
    double total = 0.0;
    double recharge = 0.0;
    json json_vouts;
    int n = 0;
    for(size_t i = 0; i < json_unspent.size(); i++)
    {
        json json_vin;
        json_vin["txid"] = json_unspent[i]["txid"].get<std::string>();
        json_vin["vout"] = json_unspent[i]["vout"].get<int>();
        json_vins.push_back(json_vin);
        total += json_unspent[i]["amount"].get<double>();
        recharge = total - balance - fee;
        if(recharge >= 0.0000005)
        {
            json json_vout;
            json_vout[to] = balance;
            json_vouts.push_back(json_vout);
            json_vout.clear();
            json_vout[from] = recharge;
            json_vouts.push_back(json_vout);

            json_params.clear();
            json_params.push_back(json_vins);
            json_params.push_back(json_vouts);
            //if (to < from)
            //	n = 1;
            break;
        }		
    }

    json_reply.clear();
    ret = CallNodeRPC("createrawtransaction", json_params, node.node_url, node.node_auth, json_reply);
    if ( !ret )
    {
        LOG(ERROR) << "rpc call fail";
        return ret;
    }

    std::string unsign_txhex = json_reply["result"].get<std::string>();
    json_params.clear();
    json_params.push_back(unsign_txhex);

    json_reply.clear();
    ret = CallNodeRPC("signrawtransactionwithwallet", json_params, node.node_url, node.node_auth, json_reply);
    if (!ret)
    {
        LOG(ERROR) << "rpc call fail";
        return ret;
    }

    std::string sign_txhex = json_reply["result"]["hex"].get<std::string>();
    json_params.clear();
    json_params.push_back(sign_txhex);

    json_reply.clear();
    ret = CallNodeRPC("sendrawtransaction", json_params, node.node_url, node.node_auth, json_reply);
    if ( !ret )
    {
        LOG(ERROR) << "rpc call fail";
        return ret;
    }

    json_response["txid"] = json_reply["result"].get<std::string>();
    json_response["n"] = n;
    ret = true;
    return ret;

}

bool ClientRpc::transferEth(const NodeInfo& node, const json&json_request, json& json_response)
{
    std::string from = json_request[2].get<std::string>();
	std::string to = json_request[3].get<std::string>();
	double balance = json_request[4].get<double>();
	double fee = json_request[5].get<double>();
    bool ret = false;
    std::string privkey;
    if ( !getPrivateKey( from, privkey) )
    {
        LOG(ERROR) << "ETH address " << from << "can't get private key" ;
        return ret;
    }
    
    ETHAPI &instance = ETHAPI::getInstance();
    instance.Init(node.node_url);
    std::string response;
    if ( !instance.offlineSign(privkey, to, balance, fee, response) )
    {
        LOG(ERROR) << "ETH address " << from << " offline sign fail" ;
        return ret;
    }

    json_response["txid"] = response;
    json_response["n"] = 0;
    ret = true;
    return ret;

}

bool ClientRpc::transferXrp(const NodeInfo& node, const json&json_request, json& json_response)
{
    std::string from = json_request[2].get<std::string>();
    std::string to = json_request[3].get<std::string>();
    double balance = json_request[4].get<double>();
    double fee = json_request[5].get<double>();
    bool ret = false;
    std::string privkey;
    if ( !getPrivateKey(from, privkey) )
    {
        LOG(ERROR) << "XRP address " << from << "can't get private key" ;
        return ret;
    }

    json json_params = json::array();
    json json_params_submit;

    json_params_submit["offline"] = false;
    json_params_submit["secret"] = privkey;
    json_params_submit["fail_hard"] = true;

    mpz_class mzp_blance(balance * 1e6);
    mpz_class mzp_fee(fee * 1e6);

    json json_params_payment;
    json_params_payment["TransactionType"] = "Payment";
    json_params_payment["Account"] = from;
    json_params_payment["Destination"] = to;
    json_params_payment["Amount"] = mzp_blance.get_str(10);
    json_params_payment["Fee"] = mzp_fee.get_str(10);
    json_params_payment["Flags"] = 2147483648;

    json_params_submit["tx_json"] = json_params_payment;

    json_params.push_back(json_params_submit);

    json json_reply ;
    ret = CallNodeRPC("submit", json_params, node.node_url, node.node_auth, json_reply);
    if (!ret)
    {
        LOG(ERROR) << "RPC call fail";
        return ret;
    }

    if(json_reply["result"]["status"].get<std::string>() == "error")
    {
        
        LOG(ERROR) << "XRP submit " << json_reply.dump(4) << "fail" ;
        return ret;
    }

    if(json_reply["result"]["engine_result"].get<std::string>() != "tesSUCCESS")
    {
        LOG(ERROR) << "XRP submit " << json_reply.dump(4) << "fail" ;
        return ret;
    }

    json_response["txid"] = json_reply["result"]["tx_json"]["hash"].get<std::string>();
    json_response["n"] = 0;

    ret = true;
    return ret;
}

bool ClientRpc::transferEos(const NodeInfo& node, const json&json_request, json& json_response)
{
    std::string from = json_request[2].get<std::string>();
    std::string to = json_request[3].get<std::string>();
    double balance = json_request[4].get<double>();

    std::string quantity = std::to_string(balance);
    if ( quantity.find(".") == std::string::npos )
    {
        quantity += ".0000 EOS";
    }
    else 
    {
        auto len = quantity.find(".") + 1;
        std::string quantity_sub = quantity.substr(len);
        auto len_sub = quantity_sub.size();
        if ( len_sub >= 4)
        {
            quantity = quantity.substr(0, len + 4) + " EOS";
        }
        else
        {
            if ( len_sub == 0 )
            {
                quantity += "0000 EOS";
            }
            else if (len_sub == 1)
            {
                quantity += "000 EOS";
            }
            else if (len_sub == 2)
            {
                quantity += "00 EOS";
            }
            else if (len_sub == 3)
            {
                quantity += "0 EOS";
            }
        }
    }

    bool ret = false;

    json json_params;
    json json_reply;
    json json_args;

    CurlParams curl_params;
    std::string curl_base_chain = "http://" + node.node_url + "/v1/chain/";
    std::string curl_base_wallet = "http://" + node.wallet_url + "/v1/wallet/";
    std::string response; 

    // abi_json_to_bin
    std::string binargs;
    json_params["code"] = "eosio.token";
    json_params["action"] = "transfer";
    json_args["from"] = from;
    json_args["to"] = to;
    json_args["quantity"] = quantity;
    json_args["memo"] = "";
    json_params["args"] = json_args;

    curl_params.url = curl_base_chain + "abi_json_to_bin";
    curl_params.data = json_params.dump();
    
    if ( !CurlPostParams(curl_params, response) )
        return ret;

    json_reply = json::parse(response);
    if ( json_reply.find("error") != json_reply.end() )
        return ret;
    
    binargs = json_reply["binargs"].get<std::string>();
    
    // get_info
    std::string chain_id;
    long long head_block_num;
    json_params.clear();
    curl_params.url = curl_base_chain + "get_info";
    curl_params.data = json_params.dump();

    response.clear();
    if ( !CurlPostParams(curl_params, response) )
    {
        LOG(ERROR) << "rpc call fail";
        return ret;
    }

    json_reply = json::parse(response);
    if ( json_reply.find("error") != json_reply.end() )
    {
        LOG(ERROR) << "EOS " << json_reply.dump(4) << "fail";
        return ret;
    }

    chain_id = json_reply["chain_id"].get<std::string>();
    head_block_num = json_reply["head_block_num"].get<long long>();

    // get_block
    std::string timestamp;
    long long ref_block_prefix;
    json_params.clear();
    json_params["block_num_or_id"] = std::to_string(head_block_num);

    curl_params.url = curl_base_chain + "get_block";
    curl_params.data = json_params.dump();

    response.clear();
    if ( !CurlPostParams(curl_params, response) )
    {

        LOG(ERROR) << "rpc call fail";
        return ret;
    }
    json_reply = json::parse(response);
    if ( json_reply.find("error") != json_reply.end() )
        return ret;

    timestamp = json_reply["timestamp"].get<std::string>();

    //struct tm* tmp_time = (struct tm*)malloc(sizeof(struct tm));
    struct tm *tmp_time = new struct tm;
    strptime(timestamp.c_str(),"%Y-%m-%dT%H:%M:%S",tmp_time);
    time_t t = mktime(tmp_time);  
    t += 600;
    char buff[70];
    strftime(buff, sizeof buff, "%Y-%m-%dT%H:%M:%S.000", gmtime(&t));
    delete tmp_time;
    //free(tmp_time);  

    timestamp = buff;
    
    ref_block_prefix = json_reply["ref_block_prefix"].get<long long>();

    // open wallet 
    json json_wallet = json::array();
    json_wallet.push_back(s_eos_wallet_name);
    json_wallet.push_back(s_eos_wallet_password);

    curl_params.url = curl_base_wallet + "unlock";
    curl_params.data = json_wallet.dump();

    response.clear();
    if ( !CurlPostParams(curl_params, response) )
        return ret;

    json_reply = json::parse(response);
    if ( !json_reply.empty() )
    {
        if ( json_reply.find("error") != json_reply.end() )
        {
            json json_what = json_reply["error"];
            if ( json_what.find("what") == json_what.end() )
            {
                LOG(ERROR) << json_reply.dump(4);
                return ret;
            }
            if ( json_what["what"].get<std::string>() != std::string("Already unlocked") )
            {
                LOG(ERROR) << json_reply.dump(4);
                return ret;
            }
        }
    }

    // sign_transaction
    json josn_param_arrays = json::array();
    json json_action_arrays = json::array();
    json json_action;
    json json_authorization_arrays = json::array();
    json json_authorization;
    json json_signature_arrays;

    json_params.clear();
    json_params["ref_block_num"] = head_block_num;
    json_params["ref_block_prefix"] = ref_block_prefix; 
    json_params["expiration"] = timestamp;

    json_action["account"] = "eosio.token";
    json_action["name"] = "transfer";

    json_authorization["actor"] = from;
    json_authorization["permission"] = "active";
    json_authorization_arrays.push_back(json_authorization);
    
    json_action["authorization"] = json_authorization_arrays;
    json_action["data"] = binargs;

    json_action_arrays.push_back(json_action);

    json_params["actions"] = json_action_arrays;
    json_params["signatures"] = json::array();

    josn_param_arrays.push_back(json_params);

    // get public key
    json json_publics = json::array();
    json_publics.push_back(eos_public_key_);
    josn_param_arrays.push_back(json_publics);

    // get chain id
    josn_param_arrays.push_back(chain_id);
    
    curl_params.url = curl_base_wallet + "sign_transaction";
    curl_params.data = josn_param_arrays.dump();

    response.clear();
    if ( !CurlPostParams(curl_params, response) )
    {

        LOG(ERROR) << "rpc call fail";
        return ret;
    }

    json_reply = json::parse(response);
    if ( json_reply.find("error") != json_reply.end() )
    {
        LOG(ERROR) << json_reply.dump(4);
        return ret;
    }

    json_signature_arrays = json_reply["signatures"];

    // push_transaction
    json_params.clear();

    json_action_arrays.clear();
    json_action.clear();
    json json_transaction;
    json_transaction["expiration"] = timestamp;
    json_transaction["ref_block_num"] = head_block_num;
    json_transaction["ref_block_prefix"] = ref_block_prefix;
    json_transaction["context_free_actions"] = json::array();

    json_action["account"] = "eosio.token";
    json_action["name"] = "transfer";

    json_authorization_arrays.clear();
    json_authorization.clear();
    json_authorization["actor"] = from;
    json_authorization["permission"] = "active";
    json_authorization_arrays.push_back(json_authorization);

    json_action["authorization"] = json_authorization_arrays;
    json_action["data"] = binargs;
    
    json_action_arrays.push_back(json_action);

    json_transaction["actions"] = json_action_arrays;
    json_transaction["transaction_extensions"] = json::array();

    json_params["compression"] = "none";
    json_params["transaction"] = json_transaction;
    json_params["signatures"] = json_signature_arrays;

    curl_params.url = curl_base_chain + "push_transaction";
    curl_params.data = json_params.dump();

    response.clear();
    if ( !CurlPostParams(curl_params, response) )
    {
        LOG(ERROR) << "rpc call fail";
        return ret;
    }

    json_reply = json::parse(response);
    if ( json_reply.find("error") != json_reply.end() )
    {
        LOG(ERROR) << json_reply.dump(4) ;
        return ret;
    }

    ret = true;
    json_response["txid"] = json_reply["transaction_id"].get<std::string>();
    json_response["n"] = 0;

    return ret;
}

bool ClientRpc::transferErc20Usdt(const NodeInfo& node, const TokenInfo& tokenInfo, const json& json_request, json& json_response)
{

    std::string from = json_request[2].get<std::string>();
    std::string to = json_request[3].get<std::string>();
    double balance = json_request[4].get<double>();
    double fee = json_request[5].get<double>();
    bool ret = false;
    std::string privkey;
    if ( !getPrivateKey(from, privkey) )
    {
        LOG(ERROR) << "ETH USDT " << from << " can't get private key";
        return ret;
    }

    ETHAPI &instance = ETHAPI::getInstance();
    instance.Init(node.node_url);
    std::string response;
    if ( !instance.offlineSignERC20(privkey, to, tokenInfo.contract, balance, fee, tokenInfo.decimal, response) )
    {
        LOG(ERROR) << "ETH USDT offline sign fail" << json_request.dump(4); 
        return ret;
    }
    json_response["txid"] = response;
    json_response["n"] = 0;

    ret = true;    
    return ret;
}

bool ClientRpc::transferOmniUsdt(const NodeInfo& node, const TokenInfo& tokenInfo, const json& json_request, json& json_response)
{
    bool ret = false;
    std::string from = json_request[2].get<std::string>();
    std::string to = json_request[3].get<std::string>();
    double balance = json_request[4].get<double>();
    double fee = json_request[5].get<double>();
    
    json json_params;
    json json_addresses = json::array();
    json_addresses.push_back(fee_address_omni_);
    json_params.push_back(1);
    json_params.push_back(9999999);
    json_params.push_back(json_addresses);

    json json_reply ;
    ret = CallNodeRPC("listunspent", json_params, node.node_url, node.node_auth, json_reply);
    if (!ret)
    {
        LOG(ERROR) << "RPC call fail";
        return ret;
    }

    if ( json_reply["result"].empty() ) 
    {
        LOG(ERROR) << from  <<"list unspent has no money";
        return ret;
    }
    json json_unspent = json_reply["result"];
    json json_vins;
    double total = 0.0;
    double recharge = 0.0;
    json json_vouts;
    for( size_t i = 0; i < json_unspent.size(); i++ )
    {
        // spendable
        if ( !json_unspent[i]["spendable"].get<bool>() )
            continue ;
        
        total += json_unspent[i]["amount"].get<double>();
        recharge = total - fee;
        if(recharge >= 0.0000005)
        {
            json_params.clear();
            json_params.push_back(from);
            json_params.push_back(to);
            json_params.push_back(std::stoul(tokenInfo.contract));
            json_params.push_back(std::to_string(balance));
            json_params.push_back(fee_address_omni_);
            json_reply.clear();

            ret = CallNodeRPC("omni_funded_send", json_params, node.node_url, node.node_auth, json_reply);
            
            if (!ret)
            {
               LOG(ERROR) << "RPC call fail";
               break;
            }
            if ( json_reply["result"].empty() )
            {   
                LOG(ERROR) << json_reply.dump(4);
                break;
            }

            json_response["txid"] = json_reply["result"].get<std::string>();
            json_response["n"] = 0;
            ret = true;
            break;
        }		
    }

    return ret;
}

bool ClientRpc::createAddressOffline(const NodeInfo& node, std::string& address,std::string& privkey)
{
    bool ret = false;
    if (node.node_id == ETH)
    {
        // eth::KeyPair p = eth::KeyPair::create();
        // privkey  = eth::asHex(p.secret().asArray());
        // address =  eth::asHex(p.address().asArray());

        ETHAPI &instance = ETHAPI::getInstance();
        instance.Init(node.node_url);
        Account_t account;
        if ( !instance.createAccount(account) )
        {
            ret = false;
        }
    
        address = account.address;
        //std::transform(address.begin(), address.end(), address.begin(), ::toupper);
        privkey = account.privateKey;
        ret = true;
    }
    else if(node.node_id == XRP)
    {
        json json_params = json::array();
        json json_reply ;
        ret = CallNodeRPC("wallet_propose", json_params, node.node_url, node.node_auth, json_reply);
        if (!ret)
        {
            LOG(ERROR) << "Rpc call fail";
            return ret;
        }
        address = json_reply["result"]["account_id"].get<std::string>();
        privkey = json_reply["result"]["master_seed"].get<std::string>();
        ret = true;
    }

	return ret;
}

bool ClientRpc::createAddressLocalChain(const NodeInfo& node,  std::string& address, std::string& privkey)
{
    bool ret = false;
    json json_reply ; 
    json json_params = json::array();
    switch(node.node_id)
    {
        case BTC:
        case USDT:
            json_params.push_back("");
            json_params.push_back("legacy");
            ret = CallNodeRPC("getnewaddress", json_params, node.node_url, node.node_auth, json_reply);
            if (!ret)
            {
                LOG(ERROR) << "rpc call fail";
                break;
            }
            address = json_reply["result"].get<std::string>();
            ret = true;
            break;
        default:
            break;

    }
	return ret;
}

bool ClientRpc::getNodeInfo(const std::string& coin_name, bool is_token, NodeInfo& node, DBMysql& db_mysql)
{
	int coin_id = 0;
    LOG(INFO) << "get node info"; 
    bool ret = false;
	if (s_coinName_coinid_.find(coin_name) != s_coinName_coinid_.end())
	{	
		if(is_token)
		{	
			coin_id = s_tokenid_coinid_[s_coinName_coinid_[coin_name]];
		}
		else
		{
			coin_id = s_coinName_coinid_[coin_name]; 
		}
		
		node = s_coinid_NodeInfo_[coin_id];
        ret = true;
	}	
	else
	{
		std::string sql;
		std::string sql_coin;
		int token_id = 0;
		std::map<int,DBMysql::DataType> map_col_type;
		map_col_type[0] = DBMysql::INT;
		json json_data;
        bool find  = false;
		if (!is_token)
		{
			sql =  strprintf("SELECT node_id, node_url, node_auth, wallet_url, wallet_auth FROM node \
					WHERE node_id = (SELECT node_id FROM coin WHERE short_name = '%s') ;", coin_name);
			sql_coin = strprintf("SELECT coin_id FROM coin WHERE short_name = '%s';", coin_name);
			
			db_mysql.getData(sql_coin, map_col_type, json_data);
			if (json_data.size() > 0)
			{
			    coin_id = json_data[0][0].get<int>();
			    s_coinName_coinid_[coin_name] = coin_id;
                find = true;
            }

		}
		else
		{
			sql = strprintf("SELECT node_id, node_url, node_auth, wallet_url, wallet_auth FROM node \
					WHERE node_id = (SELECT node_id FROM coin WHERE \
						coin_id = (SELECT coin_id FROM token WHERE token.short_name = '%s'));", coin_name);

			sql_coin  = strprintf("SELECT coin_id, token_id FROM token WHERE short_name = '%s';", coin_name);

			map_col_type[1] = DBMysql::INT;
			db_mysql.getData(sql_coin, map_col_type, json_data);
			if (json_data.size() > 0)
			{
			    coin_id = json_data[0][0].get<int>();
			    token_id = json_data[0][1].get<int>();
			    s_coinName_coinid_[coin_name] = token_id;
			    s_tokenid_coinid_[token_id] = coin_id;
                find = true;
            }
		}

        if (find)
        {
            map_col_type[1] = DBMysql::STRING;
            map_col_type[2] = DBMysql::STRING;
            map_col_type[3] = DBMysql::STRING;
            map_col_type[4] = DBMysql::STRING;
            json_data.clear();
            db_mysql.getData(sql, map_col_type, json_data);

            if (json_data.size() > 0)
            {

                NodeInfo node_info;
                node_info.node_id = json_data[0][0].get<int>();
                node_info.node_url = json_data[0][1].get<std::string>();
                node_info.node_auth = json_data[0][2].get<std::string>();
                node_info.wallet_url = json_data[0][3].get<std::string>();
                node_info.wallet_auth = json_data[0][4].get<std::string>();

                s_coinid_NodeInfo_[coin_id] = node_info;
                node = node_info;
                ret = true;
            } 
        }
	}


	return ret;
}


