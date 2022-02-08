// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <init.h>
#include <compat/sanity.h>
#include <fs.h>
#include <httpserver.h>
#include <httprpc.h>
#include <netbase.h>
#include <rpc/server.h>
#include <rpc/register.h>
#include <rpc/safemode.h>
#include <rpc/client.h>
#include <scheduler.h>
#include <timedata.h>
#include <util.h>
#include <warnings.h>
#include <stdint.h>
#include <stdio.h>
#include <memory>
#include <fstream>
#include "db_mysql.h"
#include "ethapi.h"
#ifndef WIN32
#include <signal.h>
#endif
#include <libethereum/Common.h>
#include <glog/logging.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/bind.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/thread.hpp>
#include <openssl/crypto.h>
#include <leveldb/db.h>
#include "ethapi.h"
#ifdef WIN32
// Win32 LevelDB doesn't use filedescriptors, and the ones used for
// accessing block files don't count towards the fd_set size limit
// anyway.
#define MIN_CORE_FILEDESCRIPTORS 0
#else
#define MIN_CORE_FILEDESCRIPTORS 150
#endif
json g_json_config;

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



bool InitError(const std::string& str)
{
    return false;
}

std::atomic<bool> fRequestShutdown(false);

void StartShutdown()
{
    fRequestShutdown = true;
}

bool ShutdownRequested()
{
    return fRequestShutdown;
}

static boost::thread_group threadGroup;
static CScheduler scheduler;

void Interrupt()
{
    InterruptHTTPServer();
    InterruptHTTPRPC();
    InterruptRPC();
   // InterruptREST();
    
}

void Shutdown()
{
    LogPrintf("%s: In progress...\n", __func__);
    static CCriticalSection cs_Shutdown;
    TRY_LOCK(cs_Shutdown, lockShutdown);
    if (!lockShutdown)
        return;

    /// Note: Shutdown() must be able to handle cases in which initialization failed part of the way,
    /// for example if the data directory was found to be locked.
    /// Be sure that anything that writes files or flushes caches only does this if the respective
    /// module was initialized.
    RenameThread("bitcoin-shutoff");

    StopHTTPRPC();
    //StopREST();
    StopRPC();
    StopHTTPServer();
    // CScheduler/checkqueue threadGroup
    threadGroup.interrupt_all();
    threadGroup.join_all();

#ifndef WIN32
    try {
        fs::remove(GetPidFile());
    } catch (const fs::filesystem_error& e) {
        LogPrintf("%s: Unable to remove pidfile: %s\n", __func__, e.what());
    }
#endif
}

/**
 * Signal handlers are very limited in what they are allowed to do.
 * The execution context the handler is invoked in is not guaranteed,
 * so we restrict handler operations to just touching variables:
 */
#ifndef WIN32
static void HandleSIGTERM(int)
{
    fRequestShutdown = true;
}

static void HandleSIGHUP(int)
{
    fReopenDebugLog = true;
}
#else
static BOOL WINAPI consoleCtrlHandler(DWORD dwCtrlType)
{
    fRequestShutdown = true;
    Sleep(INFINITE);
    return true;
}
#endif

#ifndef WIN32
static void registerSignalHandler(int signal, void(*handler)(int))
{
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(signal, &sa, nullptr);
}
#endif

void OnRPCStarted()
{
}

void OnRPCStopped()
{
}

static CWaitableCriticalSection cs_GenesisWait;
static CConditionVariable condvar_GenesisWait;

bool AppInitServers()
{
    RPCServer::OnStarted(&OnRPCStarted);
    RPCServer::OnStopped(&OnRPCStopped);
    if (!InitHTTPServer())
        return false;
    if (!StartRPC())
        return false;
    if (!StartHTTPRPC())
        return false;
    if(!StartREST())
        return false;
   // if (gArgs.GetBoolArg("-rest", DEFAULT_REST_ENABLE) && !StartREST())
   //     return false;
    if (!StartHTTPServer())
        return false;
    return true;
}
void InitLogging()
{
    /*fPrintToConsole = gArgs.GetBoolArg("-printtoconsole", false);
    fLogTimestamps = gArgs.GetBoolArg("-logtimestamps", DEFAULT_LOGTIMESTAMPS);
    fLogTimeMicros = gArgs.GetBoolArg("-logtimemicros", DEFAULT_LOGTIMEMICROS);
    fLogIPs = gArgs.GetBoolArg("-logips", DEFAULT_LOGIPS);

    LogPrintf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
    std::string version_string = "httpd";//FormatFullVersion();
#ifdef DEBUG
    version_string += " (debug build)";
#else
    version_string += " (release build)";
#endif
    LogPrintf(PACKAGE_NAME " version %s\n", version_string);*/
    std::string log_path = "./log";
    InitLog(log_path);

}

[[noreturn]] static void new_handler_terminate()
{
    // Rather than throwing std::bad-alloc if allocation fails, terminate
    // immediately to (try to) avoid chain corruption.
    // Since LogPrintf may itself allocate memory, set the handler directly
    // to terminate first.
    std::set_new_handler(std::terminate);
    LogPrintf("Error: Out of memory. Terminating.\n");

    // The log was successful, terminate now.
    std::terminate();
};

bool AppInitBasicSetup()
{
    // ********************************************************* Step 1: setup
#ifdef _MSC_VER
    // Turn off Microsoft heap dump noise
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, CreateFileA("NUL", GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, 0));
    // Disable confusing "helpful" text message on abort, Ctrl-C
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
#ifdef WIN32
    // Enable Data Execution Prevention (DEP)
    // Minimum supported OS versions: WinXP SP3, WinVista >= SP1, Win Server 2008
    // A failure is non-critical and needs no further attention!
#ifndef PROCESS_DEP_ENABLE
    // We define this here, because GCCs winbase.h limits this to _WIN32_WINNT >= 0x0601 (Windows 7),
    // which is not correct. Can be removed, when GCCs winbase.h is fixed!
#define PROCESS_DEP_ENABLE 0x00000001
#endif
    typedef BOOL (WINAPI *PSETPROCDEPPOL)(DWORD);
    PSETPROCDEPPOL setProcDEPPol = (PSETPROCDEPPOL)GetProcAddress(GetModuleHandleA("Kernel32.dll"), "SetProcessDEPPolicy");
    if (setProcDEPPol != nullptr) setProcDEPPol(PROCESS_DEP_ENABLE);
#endif

   /* if (!SetupNetworking())
        return InitError("Initializing networking failed");
*/
#ifndef WIN32
    if (!gArgs.GetBoolArg("-sysperms", false)) {
        umask(077);
    }

    // Clean shutdown on SIGTERM
    registerSignalHandler(SIGTERM, HandleSIGTERM);
    registerSignalHandler(SIGINT, HandleSIGTERM);

    // Reopen debug.log on SIGHUP
    registerSignalHandler(SIGHUP, HandleSIGHUP);

    // Ignore SIGPIPE, otherwise it will bring the daemon down if the client closes unexpectedly
    signal(SIGPIPE, SIG_IGN);
#else
    SetConsoleCtrlHandler(consoleCtrlHandler, true);
#endif

    std::set_new_handler(new_handler_terminate);

    return true;
}


static bool LockDataDirectory(bool probeOnly)
{
    // Make sure only a single Bitcoin process is using the data directory.
    fs::path datadir = GetDataDir();
    if (!LockDirectory(datadir, ".lock", probeOnly)) {
        return InitError(strprintf(_("Cannot obtain a lock on data directory %s. %s is probably already running."), datadir.string(), _(PACKAGE_NAME)));
    }
    return true;
}

void CreateBtcAddress()
{
	ClientRpc client;
	std::string coin_name = "BTC";
	size_t address_quantity = 100;
	client.getNewAddress(coin_name, address_quantity);
}

void CreateEthAddress()
{
    ClientRpc client;
    std::string coin_name = "ETH";
    size_t address_quantity = 100;
    client.getNewAddress(coin_name, address_quantity);
}

void CreateXrpAddress()
{
	ClientRpc client;
	std::string coin_name = "XRP";
	size_t address_quantity = 100;
	client.getNewAddress(coin_name, address_quantity);
}

void CreateEosAddress()
{
    ClientRpc client;
	std::string coin_name = "EOS";
	size_t address_quantity = 100;
	client.getNewAddress(coin_name, address_quantity);
}

void CreateOmniUstdAddress()
{
    ClientRpc client;
	std::string coin_name = "USDT";
	size_t address_quantity = 100;
	client.getNewAddress(coin_name, address_quantity);
}

static bool InitConfigure()
{
	 std::string config_file = gArgs.GetArg("-configure", "../config.json");
	 std::ifstream ss_config(config_file);
	 if(!ss_config)
	 {
		 return false;
	 }

	 ss_config >> g_json_config;
	 if(!g_json_config.is_object())
	 {
		 ss_config.close();
		 return false;
	 }
	 ss_config.close();

	 json json_db;
	 json_db = g_json_config["otc_db"];

	 assert(g_db_mysql->openDB(json_db));
	 g_db_mysql->closeDB();
	 return true;

}

bool AppInitMain()
{   
    bool ret = ClientRpc::init();
    
    // Start the lightweight task scheduler thread
	//scheduler.scheduleEvery(timeOut, 10000000);

	//CreateEthAddress();
	scheduler.scheduleEvery(CreateBtcAddress, 10000);
    scheduler.scheduleEvery(CreateEthAddress, 10000);
	scheduler.scheduleEvery(CreateXrpAddress, 10000);
	scheduler.scheduleEvery(CreateEosAddress, 10000);
    scheduler.scheduleEvery(CreateOmniUstdAddress, 10000);
    

    CScheduler::Function serviceLoop = boost::bind(&CScheduler::serviceQueue, &scheduler);
    //threadGroup.create_thread(boost::bind(&TraceThread<CScheduler::Function>, "scheduler", serviceLoop));

    std::thread  thread_scheduler(boost::bind(&TraceThread<CScheduler::Function>, "scheduler", serviceLoop));

    //scheduler.scheduleEvery(timeOut, 1000);
    /* Register RPC commands regardless of -server setting so they will be
     * available in the GUI RPC console even if external calls are disabled.
     */
    RegisterAllCoreRPCCommands(tableRPC);
    /* Start the RPC server already.  It will be started in "warmup" mode
     * and not really process calls already (but it will signify connections
     * that the server is there and will be ready later).  Warmup mode will
     * be disabled when initialisation is finished.
     */
     if (!AppInitServers())
            return InitError(_("Unable to start HTTP server. See debug log for details."));
    
    SetRPCWarmupFinished();
	thread_scheduler.join();
    return ret;
}

void InitParameterInteraction()
{
    // when specifying an explicit binding address, you want to listen on it
    // even when -connect or -proxy is specified
    if (gArgs.IsArgSet("-bind")) {
        if (gArgs.SoftSetBoolArg("-listen", true))
            LogPrintf("%s: parameter interaction: -bind set -> setting -listen=1\n", __func__);
    }
    if (gArgs.IsArgSet("-whitebind")) {
        if (gArgs.SoftSetBoolArg("-listen", true))
            LogPrintf("%s: parameter interaction: -whitebind set -> setting -listen=1\n", __func__);
    }

    if (gArgs.IsArgSet("-connect")) {
        // when only connecting to trusted nodes, do not seed via DNS, or listen by default
        if (gArgs.SoftSetBoolArg("-dnsseed", false))
            LogPrintf("%s: parameter interaction: -connect set -> setting -dnsseed=0\n", __func__);
        if (gArgs.SoftSetBoolArg("-listen", false))
            LogPrintf("%s: parameter interaction: -connect set -> setting -listen=0\n", __func__);
    }

    if (gArgs.IsArgSet("-proxy")) {
        // to protect privacy, do not listen by default if a default proxy server is specified
        if (gArgs.SoftSetBoolArg("-listen", false))
            LogPrintf("%s: parameter interaction: -proxy set -> setting -listen=0\n", __func__);
        // to protect privacy, do not use UPNP when a proxy is set. The user may still specify -listen=1
        // to listen locally, so don't rely on this happening through -listen below.
        if (gArgs.SoftSetBoolArg("-upnp", false))
            LogPrintf("%s: parameter interaction: -proxy set -> setting -upnp=0\n", __func__);
        // to protect privacy, do not discover addresses by default
        if (gArgs.SoftSetBoolArg("-discover", false))
            LogPrintf("%s: parameter interaction: -proxy set -> setting -discover=0\n", __func__);
    }

/*    if (!gArgs.GetBoolArg("-listen", DEFAULT_LISTEN)) {
        // do not map ports or try to retrieve public IP when not listening (pointless)
        if (gArgs.SoftSetBoolArg("-upnp", false))
            LogPrintf("%s: parameter interaction: -listen=0 -> setting -upnp=0\n", __func__);
        if (gArgs.SoftSetBoolArg("-discover", false))
            LogPrintf("%s: parameter interaction: -listen=0 -> setting -discover=0\n", __func__);
        if (gArgs.SoftSetBoolArg("-listenonion", false))
            LogPrintf("%s: parameter interaction: -listen=0 -> setting -listenonion=0\n", __func__);
    }
*/
    if (gArgs.IsArgSet("-externalip")) {
        // if an explicit public IP is specified, do not try to find others
        if (gArgs.SoftSetBoolArg("-discover", false))
            LogPrintf("%s: parameter interaction: -externalip set -> setting -discover=0\n", __func__);
    }

    // disable whitelistrelay in blocksonly mode
    /*if (gArgs.GetBoolArg("-blocksonly", DEFAULT_BLOCKSONLY)) {
        if (gArgs.SoftSetBoolArg("-whitelistrelay", false))
            LogPrintf("%s: parameter interaction: -blocksonly=1 -> setting -whitelistrelay=0\n", __func__);
    }*/

    // Forcing relay from whitelisted hosts implies we will accept relays from them in the first place.
/*    if (gArgs.GetBoolArg("-whitelistforcerelay", DEFAULT_WHITELISTFORCERELAY)) {
        if (gArgs.SoftSetBoolArg("-whitelistrelay", true))
            LogPrintf("%s: parameter interaction: -whitelistforcerelay=1 -> setting -whitelistrelay=1\n", __func__);
    }*/
}
 
int main(int argc, char* argv[])
{
    gArgs.ParseParameters(argc, argv);
    try
    {
		//InitDB();
		assert(InitConfigure());
        InitLogging();
        LOG(INFO) << "INIT LOGGING"; 
        InitParameterInteraction();
        assert(AppInitMain());
    }
    catch (const std::exception& e) {
        PrintExceptionContinue(&e, "AppInit()");
    } catch (...) {
        PrintExceptionContinue(nullptr, "AppInit()");
    }

    return 0;
}

