#include "common.h"
#include <iostream>
#include <fstream>
#include <glog/logging.h>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
static json s_json_conf;

static bool ParseCmd(int argc,char*argv[])
{
    using namespace boost::program_options;
    std::string conf_file ;

    boost::program_options::options_description opts_desc("All options");
    opts_desc.add_options()
            ("help,h", "help info")
            ("configure,c", value<std::string>(&conf_file)->default_value("../config.json"), "configure file");

    variables_map cmd_param_map;
    try
    {
        store(parse_command_line(argc, argv, opts_desc), cmd_param_map);
    }
    catch(boost::program_options::error_with_no_option_name &ex)
    {
        std::cerr << ex.what() << std::endl;
    }
    notify(cmd_param_map);

    if (cmd_param_map.count("help"))
    {
        std::cout << opts_desc << std::endl;
        return false;
    }
 	std::ifstream jfile(conf_file);

    if (!jfile)
    {
		std::cerr << "No " <<conf_file <<" such config file!\n";
        return false;
    }

    jfile >> s_json_conf;
    if(!s_json_conf.is_object())
    {
		std::cerr << conf_file  <<   "is not json object!\n";
        jfile.close();
        return false;
    }
   
    jfile.close();

    return true;
}

static bool InitLog(const std::string& log_path)
{
	boost::filesystem::path path_check(log_path);

	if( !boost::filesystem::exists(path_check) )
	{
		boost::filesystem::create_directory(path_check);
	}

	FLAGS_alsologtostderr = false;
	FLAGS_colorlogtostderr = true;
	FLAGS_max_log_size = 100;
	FLAGS_stop_logging_if_full_disk  = true;
	std::string log_exec = "log_exe";
	FLAGS_logbufsecs = 0;
	google::InitGoogleLogging(log_exec.c_str());
	FLAGS_log_dir = log_path;
	std::string log_dest = log_path+"/info_";
	google::SetLogDestination(google::GLOG_INFO,log_dest.c_str());
	log_dest = log_path+"/warn_";

	google::SetLogDestination(google::GLOG_WARNING,log_dest.c_str());
	log_dest = log_path+"/error_";
	google::SetLogDestination(google::GLOG_ERROR,log_dest.c_str());
	log_dest = log_path+"/fatal_";
	google::SetLogDestination(google::GLOG_FATAL,log_dest.c_str());
	google::SetStderrLogging(google::GLOG_ERROR);

	return true;
}
void RunTest(const std::string& param_file)
{
   	std::ifstream jfile(param_file);

    if (!jfile)
    {
		std::cerr << "No " << param_file <<" such config file!\n";
        return ;
    }
    json json_params;

    jfile >> json_params;
    if(!json_params.is_object())
    {
		std::cerr << param_file  <<   "is not json object!\n";
        jfile.close();
        return;
    }
   
    jfile.close();

    std::string url = json_params["url"].get<std::string>();

    CurlParams params;
    params.url = url;

    json json_post = json_params["post"];
    for ( size_t i = 0; i < json_post.size(); i++)
    {
        std::string response;
        params.data = json_post[i].dump(); 
        bool ret = CurlPostParams(params, response);
        std::cout << response  << std::endl;
    }

}

int main (int argc,char*argv[])
{
	assert(ParseCmd(argc,argv));
	std::string log_path = s_json_conf["logpath"].get<std::string>();
	assert(InitLog(log_path));
    std::string params_file = s_json_conf["params"].get<std::string>();

    RunTest(params_file);
    return 0;
}
