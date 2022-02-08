# This Python file uses the following encoding: utf-8
#c++ python

from web3 import Web3
import web3
import json
import os
import re

def checkPwd(password):
    flag = 0
    while True:
        if (len(password)<3):
            flag = -1
            break
        elif not re.search("[a-z]", password):
            flag = -1
            break
        elif not re.search("[A-Z]", password):
            flag = -1
            break
        elif not re.search("[0-9]", password):
            flag = -1
            break
        # elif not re.search("[_@$]", password):
        #     flag = -1
        #     break
        # elif re.search("\s", password):
        #     flag = -1
        #     break
        else:
            flag = 0
            return True
    if flag == -1:
        return False


#w3 = Web3(Web3.HTTPProvider("http://192.168.3.213:8545"))

class ETHAPI(object):
    def __init__(self, endpoint):
        self.feeMin = 21000000000000 # 21000 * 1000000000 
        if endpoint.find('http://') == -1:
            endpoint = 'http://' + endpoint
        self.w3 = Web3(Web3.HTTPProvider(endpoint))

    def keystoreGen(self, passwd, extra_entropy = ''):
        try:
            # if checkPwd(passwd) is False:
            #     return json.dumps({'code': 1, 'message': 'Invalid password'})

            acc = self.w3.eth.account.create(extra_entropy)
            keyJson = self.w3.eth.account.encrypt(str(acc._key_obj), passwd)
            if isinstance(keyJson, dict):
                return json.dumps({'code': 0, 'data': json.dumps(keyJson)})
            else:
                return json.dumps({'code': 1, 'message': 'keystoreGen fail'})

        except ValueError as err:
            return json.dumps({'code': 1, 'message': str(err)})
        except Exception as err:
            error_msg = str(err).encode('utf-8')
            print(error_msg)
            return ''

    def decryptKeystore(self, keyfile_file, passwd):
        try:
            # print(os.path.realpath(__file__))
            if not os.path.exists(keyfile_file):
                return json.dumps({'code': 1, 'data': keyfile_file + ' file does not exist'})

            with open(keyfile_file, 'r', encoding='utf8') as fp:
                json_data = json.load(fp)
                fp.close()
                if 'version' not in json_data:
                    return json.dumps({'code': 1, 'data': keyfile_file + ' is not a keystore file'})

                privateKey = self.w3.eth.account.decrypt(json_data, passwd)
                acc = self.w3.eth.account.privateKeyToAccount(privateKey)
                privateKey = acc._key_obj
                publicKey = privateKey.public_key
                address = publicKey.to_checksum_address()

                keypair = {
                    'address': address,
                    'publicKey': str(publicKey),
                    'privateKey': str(privateKey)
                }

                return json.dumps({'code': 0, 'data': json.dumps(keypair)})

        except ValueError as err:
            return json.dumps({'code': 1, 'message': str(err)})
        # 其它异常
        except Exception as err:
            error_msg = str(err).encode('utf-8')
            print(error_msg)
            return ''

    def createAccount(self, extra_entropy = ''):
        try:
            acc = self.w3.eth.account.create(extra_entropy)
            privateKey = acc._key_obj
            publicKey = privateKey.public_key
            address = publicKey.to_checksum_address()
    
            keypair = {
                'address': address,
                'publicKey': str(publicKey),
                'privateKey': str(privateKey)
            }

            return json.dumps({'code': 0, 'data': json.dumps(keypair)})
        except ValueError as err:
            return json.dumps({'code': 1, 'message': str(err)})
        # 其它异常
        except Exception as err:
            error_msg = str(err).encode('utf-8')
            print(error_msg)
            return ''

    def getBalance(self, account):
        try:
            if len(account) != 40 and (len(account) != 42 or account[0:2] != '0x'):
                return json.dumps({'code': 1, 'data': 'Invalid address'})

            chkaccount = web3.Web3.toChecksumAddress(account)
            value = self.w3.eth.getBalance(chkaccount)
            return json.dumps({'code': 0, 'data': str(web3.Web3.fromWei(value, 'ether'))})
        except ValueError as err:
            return json.dumps({'code': 1, 'message': str(err)})
        # 其它异常
        except Exception as err:
            error_msg = str(err).encode('utf-8')
            print(error_msg)
            return ''

    def getERC20Balance(self, account, contractAddr, decimals):
        try:
            if len(account) != 40 and (len(account) != 42 or account[0:2] != '0x'):
                return json.dumps({'code': 1, 'data': 'Invalid address'})
            if len(contractAddr) != 40 and (len(contractAddr) != 42 or contractAddr[0:2] != '0x'):
                return json.dumps({'code': 1, 'data': 'Invalid contract address'})

            if account[0:2] == '0x':
                account = account[2:]

            chkContractAddr = web3.Web3.toChecksumAddress(contractAddr)
            transaction = {
                'data': '0x70a08231' + account.rjust(64, '0'),
                'to': chkContractAddr}
            ret = self.w3.eth.call(transaction, 'latest')
            if ret:
                return json.dumps({'code': 0, 'data': str(int(ret.hex(), 16) / (10**decimals))})
            else:
                return json.dumps({'code': 0, 'data': '0'})
        except ValueError as err:
            return json.dumps({'code': 1, 'message': str(err)})
        except Exception as err:
            error_msg = str(err).encode('utf-8')
            print(error_msg)
            return '0'

    def getblock(self):
        return self.w3.eth.blockNumber

    def getTransactionCount(self, account):
        return self.w3.eth.getTransactionCount(account, "pending")

    def getBlockByNumber(self, num):
        return self.w3.eth.getBlock(num)

    def getGasPrice(self):
        return self.w3.eth.gasPrice

    def calcGasAndPrice(self, fee, isERC20 = False):
        feeEth = web3.Web3.toWei(fee, 'ether')
        if feeEth < self.feeMin :
            return 0,0

        price = self.getGasPrice()
        gas = int(feeEth / price)

        gasMin = 60000 if isERC20 else 21000

        if gas < gasMin:
            price = int(feeEth / gasMin / 1000000000) * 1000000000
            if price == 0:
                return 0,0
            gas = int(feeEth / price)
            if gas < gasMin:
                return 0,0
        return gas, price

    def offlineSign(self, privkey, to, amount, fee):
        global web3
        try:
            if len(to) != 40 and (len(to) != 42 or to[0:2] != '0x'):
                return json.dumps({'code': 1, 'data': 'Invalid address'})

            pk = int(privkey, 16)
            sendfrom = web3.eth.Account.privateKeyToAccount(pk)

            gas, price = self.calcGasAndPrice(fee)
            if gas == 0 :
                return ''   #fee is too small

            nonce = self.getTransactionCount(sendfrom.address)
            chktoaddress = web3.Web3.toChecksumAddress(to)
            stx = sendfrom.signTransaction(dict(nonce = nonce,
            gasPrice = int(price),
            gas = int(gas),
            to = chktoaddress,
            value = web3.Web3.toWei(amount, 'ether'),
            data = b'')
            )
            data = self.w3.eth.sendRawTransaction(stx.rawTransaction)
            return json.dumps({'code': 0, 'data': data.hex()})

        except ValueError as err:
            return json.dumps({'code': 1, 'message': str(err)})
        except Exception as err:
            error_msg = str(err).encode('utf-8')
            print(error_msg)
            return '0'

    def offlineSignERC20(self, privkey, to, contractAddr, amount, fee, decimals):
        try:
            if len(to) != 40 and (len(to) != 42 or to[0:2] != '0x'):
                return json.dumps({'code': 1, 'data': 'Invalid address'})
            if len(contractAddr) != 40 and (len(contractAddr) != 42 or contractAddr[0:2] != '0x'):
                return json.dumps({'code': 1, 'data': 'Invalid contract address'})

            pk = int(privkey, 16)
            sendfrom = web3.eth.Account.privateKeyToAccount(pk)
            gas, price = self.calcGasAndPrice(fee, True)
            if gas == 0 :
                return json.dumps({'code': 0, 'data': 'fee is too small'})
            nonce = self.getTransactionCount(sendfrom.address)
            chkContractAddr = web3.Web3.toChecksumAddress(contractAddr)
            to = str(to)
            if to[0:2] == '0x':
                to = to[2:]
            data = '0xa9059cbb' + to.rjust(64, '0') + str(hex(int(amount * (10**decimals))))[2:].rjust(64, '0')

            stx = sendfrom.signTransaction(dict(nonce = nonce,
            gasPrice = int(price),
            gas = int(gas),
            to = chkContractAddr,
            value = 0,
            data = data)
            )
            data = self.w3.eth.sendRawTransaction(stx.rawTransaction)
            return json.dumps({'code': 0, 'data': data.hex()})
        except ValueError as err:
            return json.dumps({'code': 1, 'message': str(err)})
        except Exception as err:
            error_msg = str(err).encode('utf-8')
            print(error_msg)
            return '0'

    def getTransaction(self, hash):
        return self.w3.eth.getTransaction(hash)

    def suggestFee(self, isERC20 = False):
        try:
            price = self.getGasPrice()
            gas = 60000 if isERC20 else 21000
            suggestFee = price * gas
            return json.dumps({'code': 0, 'data': str(web3.Web3.fromWei(suggestFee, 'ether'))})
        except ValueError as err:
            return json.dumps({'code': 1, 'message': str(err)})
        except Exception as err:
            error_msg = str(err).encode('utf-8')
            print(error_msg)
            return '0'


if __name__ == '__main__':
    api = ETHAPI("192.168.3.213:8545")
    # ret = api.keystoreGen('1233aF5624')
    # ret = api.decryptKeystore('UTC--2020-08-12T07-00-30.465097180Z--fef7aacaab058c4b453441eb775ab18b38834317', 'test')
    # ret = api.decryptKeystore('db.json', 'test')
    # print(api.getBlockByNumber(100))
    ret = api.offlineSign('0x307f0597800e43d4dcbc83247ab87be921a71c0e0645d0200d091c4ec9f1bedd', '0x5Ba95bE757144aE0Ff4adA2a05FC755DdB9715d0', 0.003, 0.00798)
    # ret = api.createAccount()
    # ret = api.getBalance('0x49a12f31a9ab26bfd99e26f5a5186efb77d777db')
    # print(api.getblock())
    # ret = api.suggestFee()
    # print(api.calcGasAndPrice(0.0006, True))
    # ret = api.offlineSignERC20('0x02586838eaa717c3450674cedb2f8133d5d6295e9a60407b77029d8de9923f0c', '0x3C8b54da46d09cca464fc95Af219633C3981D1B7', '0xdac17f958d2ee523a2206206994597c13d831ec7', 0.1, 0.0005, 6)
    # ret = api.getERC20Balance('0x49a12f31a9ab26bfd99e26f5a5186efb77d777db', '0x7dfe7b7c0e9402b3479193d6f23a255faf02de15', 8)
    # print(api.getTransaction("0x17e152c2729e75eb3f0e0929eee9b285ff5f0aae8043bef323cbd56bf5d7d4ce"))
    print(ret)
    pass
