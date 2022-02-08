#ifndef ETHAPI_H
#define ETHAPI_H

#include <python3.6m/Python.h>
#include <iostream>
#include "json.hpp"
using json = nlohmann::json;

struct Account {
    std::string address;
    std::string publicKey;
    std::string privateKey;
} ;
using Account_t = struct Account;

class ETHAPI
{
public:
    ETHAPI(const ETHAPI&)=delete;
    ETHAPI& operator=(const ETHAPI&)=delete;
    static ETHAPI& getInstance(){
        static ETHAPI instance;
        return instance;
    }

    enum
    {
        OK,
        ERR
    };

    ~ETHAPI();

    void Init(std::string const& endpoint);
    bool getBalance(std::string const& acc, std::string &response);
    bool getERC20Balance(std::string const& acc, std::string const& contractAddr, int const& decimals, std::string &response);
    bool createAccount(Account &response, std::string const& extraEntropy = "");
    bool offlineSign(std::string const& privkey, std::string const& to, double const& amount, double const& fee, std::string &response);
    bool offlineSignERC20(std::string const& privkey, std::string const&to, std::string const&contractAddr, double const& amount, double const& fee, int const& decimals, std::string &response);
    bool suggestFee(std::string &response, int const& isERC20 = 0);

    /***
     *  param passwd: The password which you will need to unlock the account in your client
     *  param extraEntropy: Add extra randomness to whatever randomness your OS can provide
     *  returns: The data to use in your encrypted file
     */
    bool keystoreGen(std::string const& passwd, std::string &response, std::string const& extraEntropy = "");

    /***
     *  param keystoreFile: The keystore file path
     *  param passwd: The password that was used to encrypt the key
     *  returns: the Account
     */
    bool decryptKeystore(std::string const& keystoreFile, std::string const& passwd, Account &response);

private:
    ETHAPI();
    bool getResult(PyObject *resp, std::string &response);
    bool getResult(PyObject *resp, Account &response);

    PyObject* pModoule = nullptr;
    PyObject* pDict = nullptr;
    PyObject* pClass = nullptr;
    PyObject* pInstance = nullptr;
    bool isInit = false;
};

#endif // ETHAPI_H
