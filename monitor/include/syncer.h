#ifndef SYNCER_H_
#define SYNCER_H_

#include "job/job.h"
#include "job/task.h"
#include "rpc.h"

class Syncer:public Task
{
public:
    Syncer()
    {
    }

    virtual ~Syncer()
    {

    }

    static Syncer& instance()
    {
        return single_;
    }

	void init();

    void setCallbackUrl(const std::string& url)
    {
        callback_url_ = url;
    }

    void callbackTransaction(const json& json_params);
	
    void refreshDB();

	void scanBTC();

	void scanETH();

	void scanEOS();

	void scanXRP();

    void scanOMNI_USDT();

	void reCallback();

	void verifyTransaction();

	bool getCurrentHeight(const std::string& coin, int& coin_id, uint64_t& height);
public:

    void registerTask(map_event_t& name_events, map_job_t& name_tasks);

    void setRpc(const Rpc& rpc)
	{
		rpc_ = rpc; 
	}

protected:
    static Syncer single_;
	Rpc rpc_;
	std::map<int, NodeInfo> map_coinid_node_;
	std::map<int, std::vector<int> > map_coinid_tokenid_;
	std::vector<std::string> vect_sql_;
    std::string callback_url_;
    std::string eos_base_address_;
    std::string xrp_base_address_;
};

#endif
