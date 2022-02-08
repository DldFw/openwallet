#include <rpc/server.h>
#include <utilstrencodings.h>
#include <rpc/register.h>
#include "db_mysql.h"
#include <utiltime.h>
#include <map>
#include <tinyformat.h>
#include <boost/algorithm/string/split.hpp>
#include <rpc/client.h>

struct AccountInfo
{
	int account_id;
	std::string address;
	std::string tag;
};

static std::map<std::string, AccountInfo> s_usercoin_accountinfo;

json getrechargeaddress(const JSONRPCRequest& request)
{
	if (request.fHelp || request.params.size() != 3) {
		throw std::runtime_error(
				"\nArguments:\n"
				"1. \"name\" (string)  The name of coin or token\n"
				"2. \"type\" (bool) the type of coin ,maybe it is coin or token"
				"3. \"user_id\" (string) the transaction from"
				);
	}
	bool ret = g_db_mysql->openDB(g_json_config["otc_db"]);
	if (!ret)
	{
		throw std::runtime_error("db server is down!");
	}

	json json_result;
	std::string coin_name = request.params[0].get<std::string>();
	bool is_token  = request.params[1].get<bool>();
	int user_id  = request.params[2].get<int>();
	std::string sql;
	if (!is_token)
		sql =  strprintf("SELECT coin_id FROM coin WHERE short_name = '%s';", coin_name);
	else
		sql = strprintf("SELECT coin_id FROM coin WHERE coin_id = (SELECT coin_id FROM token WHERE token.short_name = '%s');", coin_name);
	
	std::map<int,DBMysql::DataType> map_col_type;
	map_col_type[0] = DBMysql::INT;

	json json_data;
	g_db_mysql->getData(sql, map_col_type, json_data);
	if (json_data.size() <= 0)
	{
		std::string error_info = strprintf("The %s don't have support", coin_name); 
		g_db_mysql->closeDB();
		throw std::runtime_error(error_info);
	}

	int coin_id = json_data[0][0].get<int>();

	std::string encode_user = std::to_string(user_id) + std::to_string(coin_id) ;
	if (s_usercoin_accountinfo.find(encode_user) != s_usercoin_accountinfo.end())
	{
		json_result["user_id"] = user_id;
		json_result["coin"] = coin_name;
		json_result["address"] = s_usercoin_accountinfo[encode_user].address;
		json_result["tag"] = s_usercoin_accountinfo[encode_user].tag;

		g_db_mysql->closeDB();
		return json_result;
	}

	sql = strprintf("SELECT account_id, address, tag FROM account WHERE coin_id = %d AND account_id = (SELECT account_id FROM user WHERE coin_id = %d AND user_id = %d);" , coin_id, coin_id, user_id);
	map_col_type[0] = DBMysql::INT;
	map_col_type[1] = DBMysql::STRING;
	map_col_type[2] = DBMysql::STRING;

	AccountInfo account_info;
	json_data.clear();
	g_db_mysql->getData(sql, map_col_type, json_data);
	if (json_data.size() > 0)
	{
		json_result["user_id"] = user_id;
		json_result["coin"] = coin_name;
		json_result["address"] = json_data[0][1].get<std::string>();
		json_result["tag"] = json_data[0][2].get<std::string>();
		AccountInfo account_info;
		account_info.account_id = json_data[0][0].get<int>();
		account_info.address = json_data[0][1].get<std::string>();
		account_info.tag = json_data[0][2].get<std::string>();
		s_usercoin_accountinfo[encode_user] = account_info;

		g_db_mysql->closeDB();
		return json_result;
	}

	sql = strprintf("SELECT account_id, address, tag FROM account WHERE coin_id = %d AND \
			account_id > (SELECT MAX(account_id) FROM user WHERE coin_id = %d) ORDER BY account_id LIMIT 1", coin_id, coin_id);

	g_db_mysql->getData(sql, map_col_type, json_data);
	if (json_data.size() <= 0)
	{
		g_db_mysql->closeDB();
		throw std::runtime_error("address is creating ,wait a minute");
	}

	sql = strprintf("INSERT INTO `user` (`user_id`, `account_id`, `coin_id`) VALUES ('%d', '%d', '%d');", 
			user_id, json_data[0][0].get<int>(), coin_id);

	if ( !g_db_mysql->refreshDB(sql) )
	{	
		g_db_mysql->closeDB();
		throw std::runtime_error("db server can't write!");
	}

	json_result["user_id"] = user_id;
	json_result["coin"] = coin_name;
	json_result["address"] = json_data[0][1].get<std::string>();
	json_result["tag"] = json_data[0][2].get<std::string>();
	account_info.account_id = json_data[0][0].get<int>();
	account_info.address = json_data[0][1].get<std::string>();
	account_info.tag = json_data[0][2].get<std::string>();
	s_usercoin_accountinfo[encode_user] = account_info;

	g_db_mysql->closeDB();
	return json_result;
}

json transfer(const JSONRPCRequest& request)
{
	if (request.fHelp || request.params.size() != 6) {
		throw std::runtime_error(
				"\nArguments:\n"
				"1. \"name\" (string)  The name of coin or token\n"
				"2. \"type\" (bool) the type of coin ,maybe it is coin or token"
				"3. \"from\" (string) the transaction from"
				"4. \"to\" (string) the transaction to"
				"5. \"balance\" (double) the transaction amount"
				"6. \"fee\" (double) the transaction fee"
				);
	}

	ClientRpc client;
	json json_result;

	bool ret = client.transfer(request.params, json_result);
	if( !ret )
	{
		throw std::runtime_error("rpc call fail!");
	}

    return json_result;
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
    //  --------------------- ------------------------  -----------------------  ----------
    { "relay",            "getrechargeaddress",         &getrechargeaddress,           {} },
    { "relay",            "transfer",           		&transfer,           	 {} },
};


void RegisterRelayRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
