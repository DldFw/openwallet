#include "ethapi.h"
#include <string.h>
//#include <qlogging.h>

ETHAPI::ETHAPI()
{
    Py_Initialize();
    if (!Py_IsInitialized()) //如果没有初始化成功
    {
        //qWarning("fail to initial!" );
        return;
    }

    PyRun_SimpleString("import sys");
    PyRun_SimpleString("sys.path.append('../src')");
    
    pModoule = PyImport_ImportModule("ethwalletweb3");
    if (pModoule == nullptr)
    {
        //qWarning("Can't find the python file!" );
        return;
    }
    //模块的字典列表
    pDict = PyModule_GetDict(pModoule); //获得Python模块中的函数列
    if (pDict == nullptr)
    {
        //qWarning("Can't find the dictionary!" );
        return;
    }
    //获取类
    pClass = PyDict_GetItemString(pDict, "ETHAPI");//获取函数字典中的ETHAPI类
    if(pClass == nullptr)
    {
        //qWarning("Can't find ETHAPI class!" );
        return;
    }
}

ETHAPI::~ETHAPI()
{
    Py_DECREF(pInstance);
    Py_DECREF(pClass);
    Py_DECREF(pDict);
    Py_DECREF(pModoule);
    Py_Finalize();
}

void ETHAPI::Init(std::string const& endpoint)
{
    try{
        if (!isInit) {
            PyObject* poEndpoint = Py_BuildValue("(s)", endpoint.c_str());
            if (poEndpoint == nullptr) {
                //qWarning("Py_BuildValue fail!" );
                std::cout << "Py_BuildValue fail!" << std::endl;
                return;
            }

            //构造类的实例
            pInstance = PyObject_CallObject(pClass, poEndpoint);
            if (pInstance == nullptr)
            {
                //qWarning("Can't find ETHAPI instance!" );
                std::cout << "Can't find ETHAPI instance!" << std::endl;
                return;
            }
            isInit = true;
        }
    } catch (std::exception& ex) {
        //qWarning(("function ETHAPI::Init error: " + std::string(ex.what())).c_str());
        std::cout << "function ETHAPI::Init error: "  <<  std::string(ex.what()).c_str() << std::endl;
    }
}

bool ETHAPI::getResult(PyObject *pyResp, std::string &response)
{
    char *p = nullptr;
    PyArg_Parse(pyResp, "s", &p);

    auto json_reply = json::parse(p);
    auto ret = json_reply["code"].get<int>();

    if (ret != ETHAPI::OK)
    {
        //qWarning(json_reply["message"].get<std::string>().c_str());
        std::cout << json_reply["message"].get<std::string>().c_str() << std::endl;
        return false;
    }

    response = json_reply["data"].get<std::string>();
    return true;
}

bool ETHAPI::getResult(PyObject *pyResp, Account &response)
{
    char *p = nullptr;
    PyArg_Parse(pyResp, "s", &p);

    auto json_reply = json::parse(p);
    auto ret = json_reply["code"].get<int>();

    if (ret != ETHAPI::OK)
    {
        //qWarning(json_reply["message"].get<std::string>().c_str());
        std::cout << json_reply["message"].get<std::string>().c_str() << std::endl;

        return false;
    }

    auto accout = json_reply["data"].get<std::string>();

    auto json_account = json::parse(accout.c_str());
    response.address = json_account["address"].get<std::string>();
    response.publicKey = json_account["publicKey"].get<std::string>();
    response.privateKey = json_account["privateKey"].get<std::string>();

    return true;
}

bool ETHAPI::getBalance(std::string const& acc, std::string &response)
{
    try {
        //调用实例对象的函数
        PyObject *pyResp = PyObject_CallMethod(pInstance, "getBalance", "(s)", acc.c_str());
        if (!pyResp)
        {
            //qWarning("Can't find method getBalance!" );
            return false;
        }

        return getResult(pyResp, response);
    } catch (std::exception& ex) {
        //qWarning(("function getBalance error: " + std::string(ex.what())).c_str());
        return false;
    }
}

bool ETHAPI::getERC20Balance(std::string const& acc, std::string const&contractAddr, int const& decimals, std::string &response)
{
    try {
        //调用实例对象的函数
        PyObject *pyResp = PyObject_CallMethod(pInstance, "getERC20Balance", "(ssi)", acc.c_str(), contractAddr.c_str(), decimals);
        if (!pyResp)
        {
            //qWarning("Can't find method getERC20Balance!" );
            return false;
        }
        return getResult(pyResp, response);
    } catch (std::exception& ex) {
        //qWarning(("function getERC20Balance error: " + std::string(ex.what())).c_str());
        return false;
    }
}

bool ETHAPI::createAccount(Account &response, std::string const& extraEntropy)
{
    try {
        //调用实例对象的函数
        PyObject *pyResp = PyObject_CallMethod(pInstance, "createAccount", "(s)", extraEntropy.c_str());
        if (!pyResp)
        {
            //qWarning("Can't find method createAccount!" );
            return false;
        }

        return getResult(pyResp, response);
    } catch (std::exception& ex) {
        //qWarning(("function createAccount error: " + std::string(ex.what())).c_str());
        return false;
    }
}

bool ETHAPI::offlineSign(std::string const& privkey, std::string const&to, double const& amount, double const& fee, std::string &response)
{
    try {
        PyObject *pyResp = PyObject_CallMethod(pInstance, "offlineSign", "(ssdd)", privkey.c_str(), to.c_str(), amount, fee);
        if (!pyResp)
        {
            //qWarning("Can't find method offlineSign!" );
            return false;
        }

        return getResult(pyResp, response);
    } catch (std::exception& ex) {
        //qWarning(("function offlineSign error: " + std::string(ex.what())).c_str());
        return false;
    }
}

bool ETHAPI::offlineSignERC20(std::string const& privkey, std::string const&to, std::string const&contractAddr, double const& amount, double const& fee, int const& decimals, std::string &response)
{
    try {
        //调用实例对象的函数
        PyObject *pyResp = PyObject_CallMethod(pInstance, "offlineSignERC20", "(sssddi)", privkey.c_str(), to.c_str(), contractAddr.c_str(), amount, fee, decimals);
        if (!pyResp)
        {
            //qWarning("Can't find method offlineSignERC20!" );
            return false;
        }

        return getResult(pyResp, response);
    } catch (std::exception& ex) {
        //qWarning(("function offlineSignERC20 error: " + std::string(ex.what())).c_str());
        return false;
    }
}

bool ETHAPI::suggestFee(std::string &response, const int &isERC20)
{
    try {
        //调用实例对象的函数
        PyObject *pyResp = PyObject_CallMethod(pInstance, "suggestFee", "(d)", isERC20);
        if (!pyResp)
        {
            //qWarning("Can't find method suggestFee!" );
            return false;
        }

        return getResult(pyResp, response);
    } catch (std::exception& ex) {
        //qWarning(("function suggestFee error: " + std::string(ex.what())).c_str());
        return false;
    }
}

bool ETHAPI::keystoreGen(std::string const& passwd, std::string &response, std::string const& extraEntropy)
{
    try {
        PyObject *pyResp = PyObject_CallMethod(pInstance, "keystoreGen", "(ss)", passwd.c_str(), extraEntropy.c_str());
        if (!pyResp)
        {
            //qWarning("Can't find method keystoreGen!" );
            return false;
        }

        return getResult(pyResp, response);
    } catch (std::exception& ex) {
        //qWarning(("function keystoreGen error: " + std::string(ex.what())).c_str());
        return false;
    }
}

bool ETHAPI::decryptKeystore(std::string const& keystoreFile, std::string const& passwd, Account &response)
{
    try {
        PyObject *pyResp = PyObject_CallMethod(pInstance, "decryptKeystore", "(ss)", keystoreFile.c_str(), passwd.c_str());
        if (!pyResp)
        {
            //qWarning("Can't find method decryptKeystore!" );
            return false;
        }
        return getResult(pyResp, response);
    } catch (std::exception& ex) {
        //qWarning(("function decryptKeystore error: " + std::string(ex.what())).c_str());
        return false;
    }
}

