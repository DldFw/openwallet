#include "syncer.h"
#include <glog/logging.h>
#include "db_mysql.h"
#include "tinyformat.h"
#include <gmp.h>
#include <gmpxx.h>

// BTC address
std::map<std::string, int> s_address_id_btc;
std::map<std::string, int> s_address_id_eth;
std::map<std::string, int> s_address_id_omni_usdt;

static std::map<std::string, int> s_ethcontranct_tokenid;
static std::map<std::string,std::string> s_txid_address;

static void SetTimeout(const std::string& name, int second)
{
    struct timeval timeout ;
    timeout.tv_sec = second;
    timeout.tv_usec = 0;
    evtimer_add(Job::map_name_event_[name], &timeout);
}

static void ScanBTC(int fd, short kind, void *ctx)
{
    LOG(INFO) << "scan block chain BTC";
    std::string sql = "SELECT address,account_id FROM account WHERE coin_id = (SELECT coin_id FROM	coin WHERE short_name = 'BTC');";
    std::map<int,DBMysql::DataType> map_col_type;
    map_col_type[0] = DBMysql::STRING;
    map_col_type[1] = DBMysql::INT;

    json json_data;
    g_db_mysql->getData(sql, map_col_type, json_data);

    for(int i = 0; i < json_data.size(); i++)
    {
        s_address_id_btc[json_data[i][0].get<std::string>()] = json_data[i][1].get<int>();
    }

    if ( s_address_id_btc.empty() )
        return ;

    Syncer::instance().scanBTC(); 
    SetTimeout("ScanBTC", 10);
}

static void ScanETH(int fd, short kind, void *ctx)
{
    LOG(INFO) << "scan block chain ETH";
    std::string sql = "SELECT address, account_id FROM account WHERE coin_id = (SELECT coin_id FROM	coin WHERE short_name = 'ETH');";
    std::map<int,DBMysql::DataType> map_col_type;
    map_col_type[0] = DBMysql::STRING;
    map_col_type[1] = DBMysql::INT;

    json json_data;
    g_db_mysql->getData(sql, map_col_type, json_data);

    std::string address;
    for(int i = 0; i < json_data.size(); i++)
    {
        address = json_data[i][0].get<std::string>();
        std::transform(address.begin(), address.end(), address.begin(), ::toupper);
        s_address_id_eth[address] = json_data[i][1].get<int>();
    }

    if ( s_address_id_eth.empty() )
        return ;
    
    Syncer::instance().scanETH(); 
    SetTimeout("ScanETH", 15);
}

static void ScanXRP(int fd, short kind, void *ctx)
{
    LOG(INFO) << "scan block chain XRP";
    Syncer::instance().scanXRP(); 
    SetTimeout("ScanXRP", 5);
}

static void ScanEOS(int fd, short kind, void *ctx)
{
    LOG(INFO) << "scan block chain EOS";
    Syncer::instance().scanEOS(); 
    SetTimeout("ScanEOS", 5);
}

static void ScanOMNI_USDT(int fd, short kind, void *ctx)
{
    LOG(INFO) << "scan block chain OMNI_USDT";

    std::string sql = "SELECT address,account_id FROM account WHERE coin_id = (SELECT coin_id FROM	coin WHERE short_name = 'USDT');";
    std::map<int,DBMysql::DataType> map_col_type;
    map_col_type[0] = DBMysql::STRING;
    map_col_type[1] = DBMysql::INT;

    json json_data;
    g_db_mysql->getData(sql, map_col_type, json_data);

    for(int i = 0; i < json_data.size(); i++)
    {
        s_address_id_omni_usdt[json_data[i][0].get<std::string>()] = json_data[i][1].get<int>();
    }

    if ( s_address_id_omni_usdt.empty() )
        return ;

    Syncer::instance().scanOMNI_USDT(); 
    SetTimeout("ScanOMNI_USDT", 10);
}


static void ReCallback(int fd, short kind, void* ctx)
{
    Syncer::instance().reCallback(); 
    SetTimeout("ReCallback", 3*60);
}

static void VerifyTransaction(int fd, short kind, void *ctx)
{
    LOG(INFO) << "vefify transaction";
    Syncer::instance().verifyTransaction(); 
    SetTimeout("VerifyTransaction", 5*60);
}

void Syncer::refreshDB()
{
    LOG(INFO) << "refresh DB begin" ;
    LOG(INFO) << "SQL size: " << vect_sql_.size() ;
    if (vect_sql_.size() > 0)
    {
        g_db_mysql->batchRefreshDB(vect_sql_);
        vect_sql_.clear();
    }	
    LOG(INFO) << "refresh DB end" ;
}

void Syncer::init()
{
    std::string sql = "SELECT coin_id, node.node_id, node_url, node_auth, wallet_url, wallet_auth FROM	coin, node WHERE node.node_id = coin.node_id;";
    std::map<int,DBMysql::DataType> map_col_type;
    map_col_type[0] = DBMysql::INT;
    map_col_type[1] = DBMysql::INT;
    map_col_type[2] = DBMysql::STRING;
    map_col_type[3] = DBMysql::STRING;
    map_col_type[4] = DBMysql::STRING;
    map_col_type[5] = DBMysql::STRING;

    json json_data;
    g_db_mysql->getData(sql, map_col_type, json_data);
    for (int i = 0; i < json_data.size(); i++)
    {
        NodeInfo node;
        node.node_id = json_data[i][1].get<int>();
        node.node_url = json_data[i][2].get<std::string>();
        node.node_auth = json_data[i][3].get<std::string>();
        node.wallet_url = json_data[i][4].get<std::string>();
        node.wallet_auth = json_data[i][5].get<std::string>();
        map_coinid_node_[json_data[i][0].get<int>()] = node;
    }

    // init xrp_base_address_
    sql = "SELECT address, MAX(account_id) as account_id FROM account GROUP BY coin_id, address HAVING coin_id = 3 AND MAX(account_id);";
    map_col_type.clear();
    json_data.clear();
    map_col_type[0] = DBMysql::STRING;
    map_col_type[1] = DBMysql::INT;
    g_db_mysql->getData(sql, map_col_type, json_data);
    std::string address = json_data[0][0].get<std::string>();
    int account_id  = json_data[0][1].get<int>();
    assert( account_id > 0 );
    xrp_base_address_ = address;
    assert( !xrp_base_address_.empty() );

    // init eos_base_address_
    sql = "SELECT address, MAX(account_id) as account_id FROM account GROUP BY coin_id, address HAVING coin_id = 4 AND MAX(account_id);";
    map_col_type.clear();
    json_data.clear();
    map_col_type[0] = DBMysql::STRING;
    map_col_type[1] = DBMysql::INT;
    g_db_mysql->getData(sql, map_col_type, json_data);
    address = json_data[0][0].get<std::string>();
    account_id  = json_data[0][1].get<int>();
    assert( account_id > 0 );
    eos_base_address_ = address;
    assert( !eos_base_address_.empty() );

    // s_ethcontranct_tokenid
    sql = "SELECT token_id, contract FROM token WHERE short_name = 'ERC20_USDT';";
    json_data.clear();
    map_col_type.clear();
    map_col_type[0] = DBMysql::INT;
    map_col_type[1] = DBMysql::STRING;
    g_db_mysql->getData(sql, map_col_type, json_data);
    std::string contract = json_data[0][1].get<std::string>();
    std::transform(contract.begin(), contract.end(), contract.begin(), ::toupper);
    s_ethcontranct_tokenid[contract] = json_data[0][0].get<int>();
    assert( !s_ethcontranct_tokenid.empty() );

    // get s_txid_address
    sql = "SELECT txid, address FROM txutxo;";
    json_data.clear();
    map_col_type.clear();
    map_col_type[0] = DBMysql::STRING;
    map_col_type[1] = DBMysql::STRING;
    g_db_mysql->getData(sql, map_col_type, json_data);
    for ( size_t i = 0; i < json_data.size(); ++i )
    {
        s_txid_address[json_data[i][0].get<std::string>()] = json_data[i][1].get<std::string>();
    }
   
    return ;
}

bool Syncer::getCurrentHeight(const std::string& coin, int& coin_id, uint64_t& height)
{
    std::string sql = "SELECT coin_id, MAX(height) FROM block WHERE coin_id = (SELECT coin_id FROM coin WHERE short_name = '" + coin + "') GROUP BY coin_id;";
    std::map<int,DBMysql::DataType> map_col_type;
    map_col_type[0] = DBMysql::INT;
    map_col_type[1] = DBMysql::INT;

    json json_data;
    g_db_mysql->getData(sql, map_col_type, json_data);
    if (json_data.size() > 0)
    {
        coin_id = json_data[0][0].get<int>();
        height = json_data[0][1].get<uint64_t>();
        return true;
    }

    return false;
}

static std::vector<json> s_json_callback;


void Syncer::reCallback()
{
	std::vector<json> json_callback;
	for (int i = 0; i < s_json_callback.size(); i++)
	{
		json json_response;	
		std::string  auth;
		Rpc::RpcError ret_type = rpc_.callRpc(callback_url_, auth, s_json_callback[i], json_response);
		if(ret_type != Rpc::RPC_OK)
		{
			LOG(ERROR) << s_json_callback[i].dump();
		}

		int code = json_response["code"].get<int>();

		if (code == 1)
		{
			json_callback.push_back(s_json_callback[i]);
		}
	}
	s_json_callback.clear();
	s_json_callback = json_callback;

}

void Syncer::callbackTransaction(const json& json_params)
{
//    json json_post;
//    json_post["params"] = json_params;

    //rpc_.structRpc("callbackrecharge", json_params, json_post);
    json json_response;	
    std::string  auth;
    Rpc::RpcError ret_type = rpc_.callRpc(callback_url_, auth, json_params, json_response);
    if(ret_type != Rpc::RPC_OK)
    {
        LOG(ERROR) << json_params.dump();
    }

	int code = json_response["code"].get<int>();

	if (code == 1)
	{
		s_json_callback.push_back(json_params);
	}

}


void Syncer::scanBTC()
{
    int coin_id = 1;
    uint64_t pre_height =0;
    bool ret  = getCurrentHeight("BTC", coin_id, pre_height);	
    if (!ret)
        return;

    NodeInfo node_btc;
    if (map_coinid_node_.find(coin_id) == map_coinid_node_.end())
        return ;

    node_btc = map_coinid_node_[coin_id];

    json json_post;
    json json_params = json::array();
    json_post["params"] = json_params;

    rpc_.structRpc("getblockcount", json_params, json_post);
    json json_response;	

    Rpc::RpcError ret_type = rpc_.callRpc(node_btc.node_url, node_btc.node_auth, json_post, json_response);
    if(ret_type != Rpc::RPC_OK)
        return ;	

    uint64_t height = json_response["result"].get<uint64_t>();
    for (uint64_t i = pre_height+1; i <= height; i++)
    {
        json_params.clear();
        json_post.clear();
        json_params.push_back(i);    

        rpc_.structRpc("getblockhash", json_params, json_post);
        ret_type = rpc_.callRpc(node_btc.node_url, node_btc.node_auth, json_post, json_response);
        if(ret_type != Rpc::RPC_OK)
            break;

        json_params.clear();
        json_post.clear();
        std::string block_hash = json_response["result"].get<std::string>();
        json_params.push_back(block_hash);	
        rpc_.structRpc("getblock", json_params, json_post);
        ret_type = rpc_.callRpc(node_btc.node_url, node_btc.node_auth, json_post, json_response);
        if(ret_type != Rpc::RPC_OK)
            break;
        
        std::string txid;
        bool ok = true;	
        json json_txs = json_response["result"]["tx"];
        for (int j = 0; j < json_txs.size(); j++)
        {
            txid = json_txs[j].get<std::string>();
            json_params.clear();
            json_post.clear();
            json_params.push_back(txid);
            json_params.push_back(true);	
            rpc_.structRpc("getrawtransaction", json_params, json_post);
            ret_type = rpc_.callRpc(node_btc.node_url, node_btc.node_auth, json_post, json_response);
            if(ret_type != Rpc::RPC_OK)
            {	
                ok = false;
                break;
            }

            json json_vins = json_response["result"]["vin"];
            json json_vouts = json_response["result"]["vout"];
            bool vin = false;
            for (int k = 0; k < json_vins.size(); k++)
            {
                if(json_vins[k].find("txid") != json_vins[k].end())
                {
                    std::string  pre_txid = json_vins[k]["txid"].get<std::string>();

                    if (s_txid_address.find(pre_txid) != s_txid_address.end())
                    {
                        vin = true;
                        break;
                    }
                }
            }

            bool vout = false;
            for (int k = 0; k < json_vouts.size(); k++)
            {
                json json_vout = json_vouts[k];
                if (json_vout["scriptPubKey"].find("addresses") != json_vout["scriptPubKey"].end())
                {
                    int number = json_vout["n"].get<int>();
                    std::string value = std::to_string(10000000 * json_vout["value"].get<double>());
                    //value = 10000000 * json_vout["value"].get<double>();
                    for ( size_t n = 0; n < json_vout["scriptPubKey"]["addresses"].size(); n++ )
                    {
                        std::string address = json_vout["scriptPubKey"]["addresses"][n].get<std::string>();
                        if (s_address_id_btc.find(address) != s_address_id_btc.end())
                        {
                            std::string sql = strprintf("INSERT INTO `txutxo` (`txid`, `n`, `address`, `amount`, `flag`) VALUES ('%s', '%d', '%s', '%s', '%d');",
                                    txid, n, address, value, 0);
                            if (vin)
                            {
                               sql = strprintf("INSERT INTO `txutxo` (`txid`, `n`, `address`, `amount`, `flag`) VALUES ('%s', '%d', '%s', '%s', '%d');",
                                    txid, n, address, value, 2);
                            }
                            else
                            {
                                s_txid_address[txid] = address;
                            }

                            vect_sql_.push_back(sql);
                            json json_callback;
							json_callback["balance"] = value;
							json_callback["coinCode"] = "BTC";
							json_callback["n"] = number;
							json_callback["status"] = 1;
							json_callback["address"] = address;
							json_callback["txid"] = txid;
							json_callback["type"] = false;
                            
                            callbackTransaction(json_callback); 
                        }
                        else if (vin)
                        {
                            std::string sql = strprintf("INSERT INTO `txutxo` (`txid`, `n`, `address`, `amount`, `flag`) VALUES ('%s', '%d', '%s', '%s', '%d');",
                                    txid, number, address, value, 1);
                            vect_sql_.push_back(sql);
                            json json_callback;
                           
							json_callback["balance"] = value;
							json_callback["coinCode"] = "BTC";
							json_callback["n"] = number;
							json_callback["status"] = 1;
							json_callback["address"] = address;
							json_callback["txid"] = txid;
							json_callback["type"] = false;;

                            callbackTransaction(json_callback); 
                        }
                    }
                }
            }
        }

        if( !ok )
        {
            break;
        }

        if (i % 100 == 0 || i == height)
        {
            std::string sql = strprintf("INSERT INTO `block` (`coin_id`, `hash`, `height`) VALUES ('%d', '%s', '%d');",
                    coin_id, block_hash, i);
            vect_sql_.push_back(sql);			
            refreshDB();
        }
    }
}

void Syncer::scanETH()
{
    int coin_id = 2;
    uint64_t pre_height =0;
    bool ret  = getCurrentHeight("ETH", coin_id, pre_height);	
    if (!ret)
        return;

    NodeInfo node_eth;
    if (map_coinid_node_.find(coin_id) == map_coinid_node_.end())
        return ;

    node_eth = map_coinid_node_[coin_id];

    uint64_t height = 0;
    json json_response;
    json json_post;
    json json_params = json::array();
    json_params.push_back("pending");
    json_params.push_back(true);

    rpc_.structRpc("eth_getBlockByNumber", json_params, json_post);
   
    Rpc::RpcError ret_type = rpc_.callRpc(node_eth.node_url, node_eth.node_auth, json_post, json_response);
    if ( ret_type != Rpc::RPC_OK )
        return ;
    
    if ( json_response.find("result") == json_response.end() )
        return ;

    std::string hex_number = json_response["result"]["number"].get<std::string>();
    std::stringstream ss;
    ss << hex_number;
    ss >> std::hex >> height;

    for (uint64_t i = pre_height + 1; i <= height; i++)
    {
        ss.clear();
        ss << std::hex << i;
        std::string hex_height;
        std::string prefix = "0x";
        ss >> hex_height;

        json_params.clear();
        json_post.clear();
        json_params.push_back(prefix + hex_height);
        json_params.push_back(true);

        rpc_.structRpc("eth_getBlockByNumber", json_params, json_post);
        ret_type = rpc_.callRpc(node_eth.node_url, node_eth.node_auth, json_post, json_response);
        if ( ret_type != Rpc::RPC_OK )
            break ;
        
        if ( json_response.find("result") == json_response.end() )
            continue ;

        json json_trans = json_response["result"]["transactions"];
        json json_tran;
        mpz_class amount;
        mpz_class fee;
        std::string from, to, contract, value, txid;

        for(int j = 0; j < json_trans.size(); j++)
        {
            json_tran = json_trans[j];

            if( json_tran["to"].is_null() || json_tran["from"].is_null() )
                continue ;

            value = json_tran["value"].get<std::string>();
            from = json_tran["from"].get<std::string>();
            txid = json_tran["hash"].get<std::string>();

            std::transform(from.begin(), from.end(), from.begin(), ::toupper);
            
            if (value == "0x0") // contract
            {
                contract = json_tran["to"].get<std::string>();
                std::transform(contract.begin(), contract.end(), contract.begin(), ::toupper);
                if ( s_ethcontranct_tokenid.find(contract) == s_ethcontranct_tokenid.end() )
                    continue ;
                  
                std::string gas = json_tran["gas"].get<std::string>();
                std::string gas_price = json_tran["gasPrice"].get<std::string>();
                mpz_class gas_m, gas_price_m;
                gas_m = gas;
                gas_price_m = gas_price;
                fee = gas_m * gas_price_m;
                
                std::string input = json_tran["input"].get<std::string>();
                std::string method  = input.substr(0,10);
                if (method != "0xa9059cbb" || input.size() < 135)
                    continue;
                
                to = "0x" + input.substr(34,40);
                std::transform(to.begin(), to.end(), to.begin(), ::toupper);

                if ( s_address_id_eth.find(from) == s_address_id_eth.end() && s_address_id_eth.find(to) == s_address_id_eth.end() )
                    continue ;
                
                size_t flag;
                std::string address;
                // flag = 2
                if ( s_address_id_eth.find(from) != s_address_id_eth.end() && s_address_id_eth.find(to) != s_address_id_eth.end())
                {
                    flag = 2;
                    address = from;
                }
                // flag = 0
                else if ( s_address_id_eth.find(to) != s_address_id_eth.end() )
                {
                    flag = 0;
                    address = to;
                }
                // flag = 1
                else if ( s_address_id_eth.find(from) != s_address_id_eth.end() )
                {
                    flag = 1;
                    address = from;
                }
        
                int token_id = s_ethcontranct_tokenid[contract];
                value ="0x" + input.substr(input.size() - 41,40);
                amount = value;
                value = amount.get_str(10);

                json json_callback;
                json_callback["balance"] = value;
                json_callback["coinCode"] = "ERC20_USDT";
                json_callback["n"] = 0;
                json_callback["status"] = 1;
                json_callback["address"] = address;
                json_callback["txid"] = txid;
                json_callback["type"] = true;

                callbackTransaction(json_callback); 

                std::string sql = strprintf("INSERT INTO `txaccount` (`coin_id`, `token_id`, `txid`, `vin`, `vout`, `amount`, `fee`) VALUES ('%d', '%d', '%s', '%s', '%s', '%s', '%s');",
                        coin_id, token_id, txid, from, to, value, fee.get_str(10));	
                vect_sql_.push_back(sql);

                sql = strprintf("INSERT INTO `txutxo` (`txid`, `n`, `address`, `amount`, `flag`) VALUES ('%s', '%d', '%s', '%s', '%d');",
                    txid, 0, address, value, flag);
                vect_sql_.push_back(sql);
            }
            else 
            {
                to = json_tran["to"].get<std::string>();
                std::transform(to.begin(), to.end(), to.begin(), ::toupper);

                if ( s_address_id_eth.find(from) == s_address_id_eth.end() && s_address_id_eth.find(to) == s_address_id_eth.end() )
                    continue ;
                
                size_t flag;
                std::string address;
                // flag = 2
                if ( s_address_id_eth.find(from) != s_address_id_eth.end() && s_address_id_eth.find(to) != s_address_id_eth.end() )
                {
                    flag = 2;
                    address = from;
                }
                // flag = 0
                if ( s_address_id_eth.find(to) != s_address_id_eth.end() )
                {
                    flag = 0;
                    address = to;
                }
                // flag = 1
                else if ( s_address_id_eth.find(from) != s_address_id_eth.end() )
                {
                    flag = 1;
                    address = from;
                }

                std::string gas = json_tran["gas"].get<std::string>();
                std::string gas_price = json_tran["gasPrice"].get<std::string>();
                mpz_class gas_m, gas_price_m;
                gas_m = gas;
                gas_price_m = gas_price;
                fee = gas_m * gas_price_m;
                amount = value;
                value = amount.get_str(10);

                json json_callback;
                json_callback["balance"] = value;
                json_callback["coinCode"] = "ETH";
                json_callback["n"] = 0;
                json_callback["status"] = 1;
                json_callback["address"] = address;
                json_callback["txid"] = txid;
                json_callback["type"] = false;

                callbackTransaction(json_callback);

                std::string sql = strprintf("INSERT INTO `txaccount` (`coin_id`, `token_id`, `txid`, `vin`, `vout`, `amount`, `fee`) VALUES ('%d', '%d', '%s', '%s', '%s', '%s', '%s');",
                    coin_id, 0, txid, from, to, value, fee.get_str());	
                vect_sql_.push_back(sql);

                sql = strprintf("INSERT INTO `txutxo` (`txid`, `n`, `address`, `amount`, `flag`) VALUES ('%s', '%d', '%s', '%s', '%d');",
                    txid, 0, address, value, flag);
                vect_sql_.push_back(sql);
            }
        }
        
        if (i % 500 ==0 && i == height)
        {
            std::string sql = strprintf("INSERT INTO `block` (`coin_id`, `hash`, `height`) VALUES ('%d', '%s', '%d');", coin_id, txid, i);
            vect_sql_.push_back(sql);
            refreshDB();
        }
    }
}

void Syncer::scanXRP()
{
    if ( xrp_base_address_.empty() )
        return ;

    int coin_id = 3;
    uint64_t pre_height =0;
    bool ret  = getCurrentHeight("XRP", coin_id, pre_height);	
    if (!ret)
        return;

    NodeInfo node_xrp;
    if (map_coinid_node_.find(coin_id) == map_coinid_node_.end())
        return ;

    node_xrp = map_coinid_node_[coin_id];

    // leger
    json json_response;	
    json json_post;
    json json_params = json::array();
    json json_param = json::object(); 
    json_param["ledger_index"] = "validated";
    json_params.push_back(json_param);

    rpc_.structRpc("ledger", json_params, json_post);
    Rpc::RpcError ret_type = rpc_.callRpc(node_xrp.node_url, node_xrp.node_auth, json_post, json_response);
    if(ret_type != Rpc::RPC_OK)
        return ;	

    std::string status = json_response["result"]["status"].get<std::string>();
    if ( status != "success" )
        return ; 

    if ( !json_response["result"]["validated"].get<bool>() )
        return ; 
    
    uint64_t height = json_response["result"]["ledger_index"].get<uint64_t>();

    for (uint64_t i = pre_height+1; i <= height; i++)
    {
        // ledger
        json_post.clear();
        json_params.clear();
        json_param.clear();

        json_param["ledger_index"] = i;
        json_param["transactions"] = true;
        json_param["binary"] = true;
        json_params.push_back(json_param);

        rpc_.structRpc("ledger", json_params, json_post);
        ret_type = rpc_.callRpc(node_xrp.node_url, node_xrp.node_auth, json_post, json_response);
        if( ret_type != Rpc::RPC_OK )
            break ;

        std::string status = json_response["result"]["status"].get<std::string>();
        if ( status != "success" )
            continue ;
        
        if ( !json_response["result"]["validated"].get<bool>() )
            continue ;

        json json_txs = json_response["result"]["ledger"]["transactions"];

        std::string hash;
        for ( int j = 0; j < json_txs.size(); ++j )
        {
            // tx
            std::string tx = json_txs[j].get<std::string>();
            json_post.clear();
            json_params.clear();
            json_param.clear();
            json_param["transaction"] = tx;
            json_param["binary"] = false;
            json_params.push_back(json_param);

            rpc_.structRpc("tx", json_params, json_post);
            ret_type = rpc_.callRpc(node_xrp.node_url, node_xrp.node_auth, json_post, json_response);
            if( ret_type != Rpc::RPC_OK )
                break ;

            std::string status = json_response["result"]["status"].get<std::string>();
            if ( status != "success" )
                continue ;

            if ( !json_response["result"]["validated"].get<bool>() )
                continue ;
            
            std::string transaction_type = json_response["result"]["TransactionType"].get<std::string>();
            if ( transaction_type != "Payment" )
                continue ;
            
            // !xrp payment
            json json_amount = json_response["result"]["Amount"];
            if ( json_amount.is_object() )
                continue ;
            // xrp payment    
            std::string amount =  json_amount.get<std::string>(); 
            std::string account = json_response["result"]["Account"].get<std::string>();
            std::string destination = json_response["result"]["Destination"].get<std::string>();

            hash = json_response["result"]["hash"].get<std::string>();
            
            if ( xrp_base_address_ != account && xrp_base_address_ != destination)
                continue ;
            
            size_t flag;
            // flag = 0
            if (xrp_base_address_ == destination)
            {
                flag = 0;
            }
            // flag = 1
            else if (xrp_base_address_ == account)
            {
                flag = 1;
            }

            std::string sql = strprintf("INSERT INTO `txutxo` (`txid`, `n`, `address`, `amount`, `flag`) VALUES ('%s', '%d', '%s', '%s', '%d');",
                    hash, 0, xrp_base_address_, amount, flag);
            vect_sql_.push_back(sql);

            json json_callback;
            json_callback["balance"] = amount;
            json_callback["coinCode"] = "XRP";
            json_callback["n"] = 0;
            json_callback["status"] = 1;
            json_callback["toAddress"] = xrp_base_address_;
            json_callback["txid"] = hash;
            json_callback["type"] = false;;

            callbackTransaction(json_callback); 
        }

        if (i % 100 == 0 || i == height)
        {
            std::string sql = strprintf("INSERT INTO `block` (`coin_id`, `hash`, `height`) VALUES ('%d', '%s', '%d');",
                    coin_id, hash, i);
            vect_sql_.push_back(sql);			
            refreshDB();
        }
    }
}

void Syncer::scanEOS()
{
    if ( eos_base_address_.empty() )
        return ;
        
    int coin_id = 4;
    uint64_t pre_height =0;
    bool ret  = getCurrentHeight("EOS", coin_id, pre_height);	
    if (!ret)
        return;

    if (map_coinid_node_.find(coin_id) == map_coinid_node_.end())
        return ;

    NodeInfo node = map_coinid_node_[coin_id];
    std::string url_base_chain = "http://" + node.node_url + "/v1/chain/";
    std::string url_base_history = "http://" + node.node_url + "/v1/history/";

    json json_response;	
    json json_params = json::object();

    // get_info
    Rpc::RpcError ret_type = rpc_.callRpc(url_base_chain + "get_info", node.node_auth, json_params, json_response);
    if(ret_type != Rpc::RPC_OK)
        return ;	

    if ( json_response.find("last_irreversible_block_num") == json_response.end() )
        return ;

    uint64_t last_irreversible_block_num = json_response["last_irreversible_block_num"].get<uint64_t>();

    // for ( uint64_t i = pre_height + 1; i <= last_irreversible_block_num; ++i )
    for ( uint64_t i = 56264 + 1; i <= last_irreversible_block_num; ++i )
    {
        // get_block
        json_params.clear();
        json_params["block_num_or_id"] = i;
    
        ret_type = rpc_.callRpc(url_base_chain + "get_block", node.node_auth, json_params, json_response);
        if(ret_type != Rpc::RPC_OK)
            break ;	

        if ( json_response.find("error") != json_response.end() )
        {
            json json_error = json_response["error"];
            if ( !json_error.empty() )
                continue ;
        }
        
        if ( json_response.find("transactions") == json_response.end() )
            continue ;

        json json_transactions = json_response["transactions"];
        std::string trx;
        for ( size_t j = 0; j < json_transactions.size(); ++j )
        {
            json json_transaction = json_transactions[j];
            
            if ( json_transaction.find("trx") == json_transaction.end() )
                continue ;

            json json_trx = json_transaction["trx"];
            if ( json_trx.find("id") == json_trx.end() )
                continue ;

            trx = json_trx["id"].get<std::string>();

            // get_transaction
            json_params.clear();
            json_params["id"] = trx;
            ret_type = rpc_.callRpc(url_base_history + "get_transaction", node.node_auth, json_params, json_response);
            if(ret_type != Rpc::RPC_OK)
                break ;

            if ( json_response.find("error") != json_response.end() )
            {
                json json_error = json_response["error"];
                if ( !json_error.empty() )
                    continue ;
            }
            
            if ( json_response.find("trx") == json_response.end() )
                continue ;

            json json_his_trx = json_response["trx"];

            if ( json_his_trx.find("trx") == json_his_trx.end() )
                continue ;

            json json_his_trx_sub = json_his_trx["trx"];

            if ( json_his_trx_sub.find("actions") == json_his_trx_sub.end() )
                continue ;
            
            json json_actions = json_his_trx_sub["actions"];
            for ( size_t k = 0; k < json_actions.size(); ++k )
            {
                json json_action = json_actions[k];
            
                if ( json_action.find("data") == json_action.end() )
                    continue ;
                
                json json_data = json_action["data"];

                if ( (json_data.find("from") == json_data.end()) && (json_data.find("to") == json_data.end()))
                    continue ;

                std::string from = json_data["from"].get<std::string>();
                std::string to = json_data["to"].get<std::string>();
                std::string quantity = json_data["quantity"].get<std::string>();
                
                size_t flag;

                // flag = 0
                if (  eos_base_address_ == to )
                {
                     flag = 0;
                }  
                // flag = 1
                else if ( eos_base_address_ == from )
                {
                    flag = 1;
                }    
                
                if ( quantity.find(" EOS") == std::string::npos )
                    continue ;
                quantity = quantity.substr(0, quantity.find(" EOS"));

                std::string sql = strprintf("INSERT INTO `txutxo` (`txid`, `n`, `address`, `amount`, `flag`) VALUES ('%s', '%d', '%s', '%s', '%d');",
                        trx, 0, eos_base_address_, quantity, flag);
                vect_sql_.push_back(sql);

                json json_callback;
                json_callback["balance"] = quantity;
                json_callback["coinCode"] = "EOS";
                json_callback["n"] = 0;
                json_callback["status"] = 1;
                json_callback["toAddress"] = eos_base_address_;
                json_callback["txid"] = trx;
                json_callback["type"] = false;;

                callbackTransaction(json_callback);   
            }
        }
        if ( i % 100 == 0 || i == last_irreversible_block_num )
        {
            std::string sql = strprintf("INSERT INTO `block` (`coin_id`, `hash`, `height`) VALUES ('%d', '%s', '%d');",
                    coin_id, trx, i);
            vect_sql_.push_back(sql);			
            refreshDB();
        }
    }
}

void Syncer::scanOMNI_USDT()
{
    int coin_id = 5;
    uint64_t pre_height =0;
    bool ret  = getCurrentHeight("USDT", coin_id, pre_height);	
    if (!ret)
        return;

    NodeInfo node_btc;
    if (map_coinid_node_.find(coin_id) == map_coinid_node_.end())
        return ;

    node_btc = map_coinid_node_[coin_id];
    json json_response;	
    json json_post;
    json json_params = json::array();

    rpc_.structRpc("getblockcount", json_params, json_post);
    
    Rpc::RpcError ret_type = rpc_.callRpc(node_btc.node_url, node_btc.node_auth, json_post, json_response);
    if(ret_type != Rpc::RPC_OK)
        return ;	
    
    if ( json_response["result"].empty() )
        return ;

    std::string txid;
    uint64_t height = json_response["result"].get<uint64_t>();
    for (uint64_t i = pre_height+1; i <= height; i++)
    {
        // omni_listblocktransactions
        json_params.clear();
        json_post.clear();
        json_params.push_back(i);    
        rpc_.structRpc("omni_listblocktransactions", json_params, json_post);
        ret_type = rpc_.callRpc(node_btc.node_url, node_btc.node_auth, json_post, json_response);
        if(ret_type != Rpc::RPC_OK)
            break ;
        
        if ( json_response["result"].empty() )
            continue ;
        
        json json_txs = json_response["result"];
        
        for ( size_t j = 0; j < json_txs.size(); ++j )
        {
            txid = json_txs[j].get<std::string>();

            // omni_gettransaction
            json_params.clear();
            json_post.clear();
            json_params.push_back(txid);
            rpc_.structRpc("omni_gettransaction", json_params, json_post);
            ret_type = rpc_.callRpc(node_btc.node_url, node_btc.node_auth, json_post, json_response);
            if(ret_type != Rpc::RPC_OK)
                break ;

            if ( json_response["result"].empty() )
                continue ;
            
            json json_txinfo = json_response["result"];
            if ( !json_txinfo["valid"].get<bool>() ) 
                continue ;
             
            std::string sendingaddress = json_txinfo["sendingaddress"].get<std::string>();
            std::string referenceaddress = json_txinfo["referenceaddress"].get<std::string>();
            std::string amount = json_txinfo["amount"].get<std::string>();

            if ( s_address_id_omni_usdt.find(sendingaddress) == s_address_id_omni_usdt.end() && s_address_id_omni_usdt.find(referenceaddress) == s_address_id_omni_usdt.end() )
                continue ;
            
            std::string address;
            int flag; 
            // flag = 2
            if ( s_address_id_omni_usdt.find(sendingaddress) != s_address_id_omni_usdt.end() && s_address_id_omni_usdt.find(referenceaddress) != s_address_id_omni_usdt.end() )
            {
                flag = 2;
                address = sendingaddress;
            }
            // flag = 0
            else if ( s_address_id_omni_usdt.find(referenceaddress) != s_address_id_omni_usdt.end() )
            {
                flag = 0;
                address = referenceaddress;
            }
            // flag = 1
            else if ( s_address_id_omni_usdt.find(sendingaddress) != s_address_id_omni_usdt.end() )
            {
                flag = 1;
                address = sendingaddress;
            }

            int n = 0;
            std::string sql = strprintf("INSERT INTO `txutxo` (`txid`, `n`, `address`, `amount`, `flag`) VALUES ('%s', '%d', '%s', '%s', '%d');",
                txid, n, address, amount, flag);

            vect_sql_.push_back(sql);
            json json_callback;
            json_callback["balance"] = amount;
            json_callback["coinCode"] = "OMNI_USDT";
            json_callback["n"] = n;
            json_callback["status"] = 1;
            json_callback["address"] = address;
            json_callback["txid"] = txid;
            json_callback["type"] = true;;

            std::cout << "json_callback: " << json_callback.dump(4) << std::endl;

            callbackTransaction(json_callback);  
        }

        if (i % 100 == 0 || i == height)
        {
            std::string sql = strprintf("INSERT INTO `block` (`coin_id`, `hash`, `height`) VALUES ('%d', '%s', '%d');",
                    coin_id, txid, i);
            vect_sql_.push_back(sql);			
            refreshDB();
        }
    }
}

void Syncer::verifyTransaction()
{
    std::string sql_eth = "SELECT txid FROM ethtran WHERE status = -1;";
    std::string sql_usdt = "SELECT txid FROM tokentran WHERE status = -1;";
    std::map<int,DBMysql::DataType> map_col_type;
    map_col_type[0] = DBMysql::STRING;

    json json_data;
    g_db_mysql->getData(sql_eth, map_col_type, json_data);
    std::string txid;
    json json_transaction;
    std::string status;
    std::string sql;
    bool ret = false;

    for(int i = 0; i < json_data.size(); i++)
    {
        /*    txid = json_data[i][0].get<std::string>();
              ret = rpc_.getRawTransaction(txid, json_transaction);
              if (g_node_dump)
              {
              g_node_dump = false;
              break;
              }

              if (!ret)
              {
              continue;
              }

              status = json_transaction["result"]["status"].get<std::string>().substr(2,1);
              sql = "UPDATE ethtran set status = '" + status + "' WHERE txid = '" + txid + "';";
              vect_sql_.push_back(sql);*/
    }

    refreshDB();

}

Syncer Syncer::single_;
void Syncer::registerTask(map_event_t& name_events, map_job_t& name_tasks)
{
    REFLEX_TASK(ScanBTC);
    REFLEX_TASK(ScanETH);
    REFLEX_TASK(ScanXRP);
    REFLEX_TASK(ScanEOS);
    REFLEX_TASK(ScanOMNI_USDT);
    
    REFLEX_TASK(ReCallback);
}


