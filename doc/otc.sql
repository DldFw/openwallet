-- --------------------------------------------------------
-- 主机:                           47.97.183.47
-- 服务器版本:                        5.7.30-0ubuntu0.18.04.1 - (Ubuntu)
-- 服务器操作系统:                      Linux
-- HeidiSQL 版本:                  11.0.0.5919
-- --------------------------------------------------------

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET NAMES utf8 */;
/*!50503 SET NAMES utf8mb4 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;


-- 导出 otc 的数据库结构
CREATE DATABASE IF NOT EXISTS `otc` /*!40100 DEFAULT CHARACTER SET latin1 */;
USE `otc`;

-- 导出  表 otc.account 结构
CREATE TABLE IF NOT EXISTS `account` (
  `account_id` int(11) DEFAULT NULL COMMENT '账户id，用于标识相同币种账户的不同性',
  `coin_id` int(11) DEFAULT NULL COMMENT '引用coin表，标识账户所在的币种',
  `address` varchar(200) DEFAULT NULL COMMENT '对应账户的区块链的上的地址',
  `tag` varchar(200) DEFAULT NULL COMMENT '对应账户地址的标签信息，用去区分特定公链（eos，xmr）账户信息'
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COMMENT='账户表，存储币种对应的地址和tag信息。';

-- 正在导出表  otc.account 的数据：~302 rows (大约)
/*!40000 ALTER TABLE `account` DISABLE KEYS */;
INSERT INTO `account` (`account_id`, `coin_id`, `address`, `tag`) VALUES
	(0, 1, 'btc0', 'btc'),
	(0, 2, 'eth0', 'eth');

/*!40000 ALTER TABLE `account` ENABLE KEYS */;

-- 导出  表 otc.block 结构
CREATE TABLE IF NOT EXISTS `block` (
  `coin_id` int(11) NOT NULL COMMENT '币种唯一标识',
  `hash` varchar(200) NOT NULL COMMENT '区块的唯一标识',
  `height` bigint(20) NOT NULL COMMENT '区块高度',
  PRIMARY KEY (`coin_id`,`height`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COMMENT='公链的区块扫描信息，用于维护，扫描区块时间点维护';

-- 正在导出表  otc.block 的数据：~2 rows (大约)
/*!40000 ALTER TABLE `block` DISABLE KEYS */;
INSERT INTO `block` (`coin_id`, `hash`, `height`) VALUES
	(1, 'btc_hash', 1),
	(2, 'eth_hash', 1);
/*!40000 ALTER TABLE `block` ENABLE KEYS */;

-- 导出  表 otc.coin 结构
CREATE TABLE IF NOT EXISTS `coin` (
  `coin_id` int(11) NOT NULL COMMENT '币种的唯一标识',
  `node_id` int(11) DEFAULT NULL COMMENT '对应的节点（node表）的唯一标识',
  `short_name` varchar(50) DEFAULT NULL COMMENT '币种的简称',
  PRIMARY KEY (`coin_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COMMENT='数字货币的基本描述信息';

-- 正在导出表  otc.coin 的数据：~2 rows (大约)
/*!40000 ALTER TABLE `coin` DISABLE KEYS */;
INSERT INTO `coin` (`coin_id`, `node_id`, `short_name`) VALUES
	(1, 1, 'BTC'),
	(2, 2, 'ETH');
/*!40000 ALTER TABLE `coin` ENABLE KEYS */;

-- 导出  表 otc.node 结构
CREATE TABLE IF NOT EXISTS `node` (
  `node_id` int(11) NOT NULL DEFAULT '0' COMMENT '节点唯一标识',
  `node_url` varchar(50) DEFAULT NULL COMMENT '节点调用地址',
  `wallet_url` varchar(50) DEFAULT NULL COMMENT '钱包调用地址',
  `node_auth` varchar(50) DEFAULT NULL COMMENT '节点调用认证信息',
  `wallet_auth` varchar(50) DEFAULT NULL COMMENT '钱包调用认证信息',
  PRIMARY KEY (`node_id`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COMMENT='数字货币对应的节点信息，用于rpc调用和上层接口维护';

-- 正在导出表  otc.node 的数据：~2 rows (大约)
/*!40000 ALTER TABLE `node` DISABLE KEYS */;
INSERT INTO `node` (`node_id`, `node_url`, `wallet_url`, `node_auth`, `wallet_auth`) VALUES
	(1, '192.168.3.189:18443', '192.168.3.189:18443', 'dev:a', 'dev:a'),
	(2, '127.0.0.1:8545', '127.0.0.1:8545', 'dev:a', 'dev:a');
/*!40000 ALTER TABLE `node` ENABLE KEYS */;

-- 导出  表 otc.token 结构
CREATE TABLE IF NOT EXISTS `token` (
  `coin_id` int(11) NOT NULL COMMENT 'token基于的主币的唯一标识',
  `token_id` int(11) NOT NULL COMMENT 'token为唯一标识',
  `short_name` varchar(50) NOT NULL COMMENT 'token的简称',
  `contract` varchar(200) DEFAULT NULL COMMENT 'token的合约地址',
  `decimal` int(11) NOT NULL COMMENT 'token的精度'
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COMMENT='基于公链上数字货币的发行的token';

-- 正在导出表  otc.token 的数据：~0 rows (大约)
/*!40000 ALTER TABLE `token` DISABLE KEYS */;
INSERT INTO `token` (`coin_id`, `token_id`, `short_name`, `contract`, `decimal`) VALUES
	(1, 1, 'USDT', 'not', 8);
/*!40000 ALTER TABLE `token` ENABLE KEYS */;

-- 导出  表 otc.txaccount 结构
CREATE TABLE IF NOT EXISTS `txaccount` (
  `coin_id` int(11) DEFAULT NULL COMMENT '币种的唯一标识',
  `token_id` int(11) DEFAULT NULL COMMENT 'token的唯一标识',
  `txid` varchar(200) DEFAULT NULL COMMENT '交易的id，唯一标识',
  `vin` varchar(200) DEFAULT NULL COMMENT '交易的输入地址，消耗币的地址',
  `vout` varchar(200) DEFAULT NULL COMMENT '交易的输出地址，收到币的地址',
  `amount` varchar(200) DEFAULT NULL COMMENT '交易的金额',
  `fee` varchar(50) DEFAULT NULL COMMENT '交易损耗的手续费'
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COMMENT='现有公链的记账模型，有基于账户模型和UTXO的，这是基于账户模型的公链的交易统计信息';

-- 正在导出表  otc.txaccount 的数据：~0 rows (大约)
/*!40000 ALTER TABLE `txaccount` DISABLE KEYS */;
/*!40000 ALTER TABLE `txaccount` ENABLE KEYS */;

-- 导出  表 otc.txstatus 结构
CREATE TABLE IF NOT EXISTS `txstatus` (
  `txid` varchar(200) DEFAULT NULL COMMENT '交易的唯一标识',
  `chain_status` smallint(6) DEFAULT NULL COMMENT '在链上的状态，1是成功，0是失败'
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COMMENT='某些公链，交易会可能回滚或者失败，这个用于收集，这些链上交易状态信息';

-- 正在导出表  otc.txstatus 的数据：~0 rows (大约)
/*!40000 ALTER TABLE `txstatus` DISABLE KEYS */;
/*!40000 ALTER TABLE `txstatus` ENABLE KEYS */;

-- 导出  表 otc.txutxo 结构
CREATE TABLE IF NOT EXISTS `txutxo` (
  `txid` varchar(200) DEFAULT NULL COMMENT '交易的唯一标识',
  `n` int(11) DEFAULT NULL COMMENT '交易的第几笔输出',
  `address` varchar(200) DEFAULT NULL COMMENT '交易的输出地址，收到币的地址',
  `amount` varchar(200) DEFAULT NULL COMMENT '收到币的数量',
  `flag` int(11) DEFAULT NULL COMMENT 'otc 转账的一种标记，0表示充入，1表示提走， 2表示内部互转'
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COMMENT='现有公链的记账模型，有基于账户模型和UTXO的，这是基于UTXO模型的公链的交易统计信息';

-- 正在导出表  otc.txutxo 的数据：~2 rows (大约)
/*!40000 ALTER TABLE `txutxo` DISABLE KEYS */;
/*!40000 ALTER TABLE `txutxo` ENABLE KEYS */;

-- 导出  表 otc.user 结构
CREATE TABLE IF NOT EXISTS `user` (
  `user_id` int(11) DEFAULT NULL COMMENT '用户的唯一标识',
  `account_id` int(11) DEFAULT NULL COMMENT '用户对应币种的账户',
  `coin_id` int(11) DEFAULT NULL COMMENT '用户对应的币种'
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COMMENT='这个表表示的是otc对外用户的一种映射，用user_id 作为传递对外关系的一种因变量, user_id 为 0 不对外属于otc的一种内部维护';

-- 正在导出表  otc.user 的数据：~8 rows (大约)
/*!40000 ALTER TABLE `user` DISABLE KEYS */;
INSERT INTO `user` (`user_id`, `account_id`, `coin_id`) VALUES
	(0, 0, 1),
	(0, 0, 2);
/*!40000 ALTER TABLE `user` ENABLE KEYS */;

/*!40101 SET SQL_MODE=IFNULL(@OLD_SQL_MODE, '') */;
/*!40014 SET FOREIGN_KEY_CHECKS=IF(@OLD_FOREIGN_KEY_CHECKS IS NULL, 1, @OLD_FOREIGN_KEY_CHECKS) */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
