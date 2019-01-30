/**
 * @CopyRight:
 * FISCO-BCOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FISCO-BCOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FISCO-BCOS.  If not, see <http:www.gnu.org/licenses/>
 * (c) 2016-2018 fisco-dev contributors.
 *
 * @file Rpc.cpp
 * @author: caryliao
 * @date 2018-10-25
 */

#include "Rpc.h"
#include "Common.h"
#include "JsonHelper.h"
#include <jsonrpccpp/common/exception.h>
#include <libconfig/SystemConfigMgr.h>
#include <libdevcore/CommonData.h>
#include <libdevcore/easylog.h>
#include <libethcore/Common.h>
#include <libethcore/CommonJS.h>
#include <libethcore/Transaction.h>
#include <libexecutive/ExecutionResult.h>
#include <libsync/SyncStatus.h>
#include <libtxpool/TxPoolInterface.h>
#include <boost/algorithm/hex.hpp>
#include <csignal>

using namespace jsonrpc;
using namespace dev::rpc;
using namespace dev::sync;
using namespace dev::ledger;

static const int64_t maxTransactionGasLimit = 0x7fffffffffffffff;
static const int64_t gasPrice = 1;


Rpc::Rpc(std::shared_ptr<dev::ledger::LedgerManager> _ledgerManager,
    std::shared_ptr<dev::p2p::P2PInterface> _service)
  : m_ledgerManager(_ledgerManager), m_service(_service)
{}

std::string Rpc::getSystemConfigByKey(int _groupID, std::string const& key)
{
    try
    {
        RPC_LOG(INFO) << LOG_BADGE("getSystemConfigByKey") << LOG_DESC("request")
                      << LOG_KV("groupID", _groupID);
        auto blockchain = ledgerManager()->blockChain(_groupID);
        if (!blockchain)
        {
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::GroupID, RPCMsg[RPCExceptionType::GroupID]));
        }
        return blockchain->getSystemConfigByKey(key);
    }
    catch (JsonRpcException& e)
    {
        throw e;
    }
    catch (std::exception& e)
    {
        BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR, e.what()));
    }
}

std::string Rpc::getBlockNumber(int _groupID)
{
    try
    {
        RPC_LOG(INFO) << LOG_BADGE("getBlockNumber") << LOG_DESC("request")
                      << LOG_KV("groupID", _groupID);

        auto blockchain = ledgerManager()->blockChain(_groupID);
        if (!blockchain)
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::GroupID, RPCMsg[RPCExceptionType::GroupID]));

        return toJS(blockchain->number());
    }
    catch (JsonRpcException& e)
    {
        throw e;
    }
    catch (std::exception& e)
    {
        BOOST_THROW_EXCEPTION(
            JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR, boost::diagnostic_information(e)));
    }
}


std::string Rpc::getPbftView(int _groupID)
{
    try
    {
        RPC_LOG(INFO) << LOG_BADGE("getPbftView") << LOG_DESC("request")
                      << LOG_KV("groupID", _groupID);

        auto consensus = ledgerManager()->consensus(_groupID);
        if (!consensus)
        {
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::GroupID, RPCMsg[RPCExceptionType::GroupID]));
        }
        std::string status = consensus->consensusStatus();
        Json::Reader reader;
        Json::Value statusJson;
        u256 view;
        if (!reader.parse(status, statusJson))
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::JsonParse, RPCMsg[RPCExceptionType::JsonParse]));

        view = statusJson[0]["currentView"].asUInt();
        return toJS(view);
    }
    catch (JsonRpcException& e)
    {
        throw e;
    }
    catch (std::exception& e)
    {
        BOOST_THROW_EXCEPTION(
            JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR, boost::diagnostic_information(e)));
    }
}

Json::Value Rpc::getMinerList(int _groupID)
{
    try
    {
        RPC_LOG(INFO) << LOG_BADGE("getMinerList") << LOG_DESC("request")
                      << LOG_KV("groupID", _groupID);

        auto blockchain = ledgerManager()->blockChain(_groupID);
        if (!blockchain)
        {
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::GroupID, RPCMsg[RPCExceptionType::GroupID]));
        }

        auto miners = blockchain->minerList();

        Json::Value response = Json::Value(Json::arrayValue);
        for (auto it = miners.begin(); it != miners.end(); ++it)
        {
            response.append((*it).hex());
        }

        return response;
    }
    catch (JsonRpcException& e)
    {
        throw e;
    }
    catch (std::exception& e)
    {
        BOOST_THROW_EXCEPTION(
            JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR, boost::diagnostic_information(e)));
    }
}

Json::Value Rpc::getObserverList(int _groupID)
{
    try
    {
        RPC_LOG(INFO) << LOG_BADGE("getObserverList") << LOG_DESC("request")
                      << LOG_KV("groupID", _groupID);

        auto blockchain = ledgerManager()->blockChain(_groupID);
        if (!blockchain)
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::GroupID, RPCMsg[RPCExceptionType::GroupID]));

        auto observers = blockchain->observerList();

        Json::Value response = Json::Value(Json::arrayValue);
        for (auto it = observers.begin(); it != observers.end(); ++it)
        {
            response.append((*it).hex());
        }

        return response;
    }
    catch (JsonRpcException& e)
    {
        throw e;
    }
    catch (std::exception& e)
    {
        BOOST_THROW_EXCEPTION(
            JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR, boost::diagnostic_information(e)));
    }
}
Json::Value Rpc::getConsensusStatus(int _groupID)
{
    try
    {
        RPC_LOG(INFO) << LOG_BADGE("getConsensusStatus") << LOG_DESC("request")
                      << LOG_KV("groupID", _groupID);

        auto consensus = ledgerManager()->consensus(_groupID);
        if (!consensus)
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::GroupID, RPCMsg[RPCExceptionType::GroupID]));

        std::string status = consensus->consensusStatus();
        Json::Reader reader;
        Json::Value statusJson;
        if (!reader.parse(status, statusJson))
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::JsonParse, RPCMsg[RPCExceptionType::JsonParse]));

        return statusJson;
    }
    catch (JsonRpcException& e)
    {
        throw e;
    }
    catch (std::exception& e)
    {
        BOOST_THROW_EXCEPTION(
            JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR, boost::diagnostic_information(e)));
    }
}

Json::Value Rpc::getSyncStatus(int _groupID)
{
    try
    {
        RPC_LOG(INFO) << LOG_BADGE("getSyncStatus") << LOG_DESC("request")
                      << LOG_KV("groupID", _groupID);

        auto sync = ledgerManager()->sync(_groupID);
        if (!sync)
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::GroupID, RPCMsg[RPCExceptionType::GroupID]));

        std::string status = sync->syncInfo();
        Json::Reader reader;
        Json::Value statusJson;
        if (!reader.parse(status, statusJson))
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::JsonParse, RPCMsg[RPCExceptionType::JsonParse]));

        return statusJson;
    }
    catch (JsonRpcException& e)
    {
        throw e;
    }
    catch (std::exception& e)
    {
        BOOST_THROW_EXCEPTION(
            JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR, boost::diagnostic_information(e)));
    }
}


Json::Value Rpc::getClientVersion()
{
    try
    {
        RPC_LOG(INFO) << LOG_BADGE("getClientVersion") << LOG_DESC("request");

        Json::Value version;

#ifdef FISCO_GM
        version["FISCO-BCOS GM Version"] = "jimmyshi test for faster compile";
#else
        version["FISCO-BCOS Version"] = "jimmyshi test for faster compile";
#endif
        version["Build Time"] = DEV_QUOTED(FISCO_BCOS_BUILD_TIME);
        version["Build Type"] = std::string(DEV_QUOTED(FISCO_BCOS_BUILD_PLATFORM)) + "/" +
                                std::string(DEV_QUOTED(FISCO_BCOS_BUILD_TYPE));
        version["Git Branch"] = DEV_QUOTED(FISCO_BCOS_BUILD_BRANCH);
        version["Git Commit Hash"] = DEV_QUOTED(FISCO_BCOS_COMMIT_HASH);

        return version;
    }
    catch (JsonRpcException& e)
    {
        throw e;
    }
    catch (std::exception& e)
    {
        BOOST_THROW_EXCEPTION(
            JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR, boost::diagnostic_information(e)));
    }

    return Json::Value();
}

Json::Value Rpc::getPeers()
{
    try
    {
        RPC_LOG(INFO) << LOG_BADGE("getPeers") << LOG_DESC("request");

        Json::Value response = Json::Value(Json::arrayValue);

        auto sessions = service()->sessionInfos();
        for (auto it = sessions.begin(); it != sessions.end(); ++it)
        {
            Json::Value node;
            node["NodeID"] = it->nodeID.hex();
            node["IPAndPort"] = it->nodeIPEndpoint.name();
            node["Topic"] = Json::Value(Json::arrayValue);
            for (std::string topic : it->topics)
                node["Topic"].append(topic);
            response.append(node);
        }

        return response;
    }
    catch (JsonRpcException& e)
    {
        throw e;
    }
    catch (std::exception& e)
    {
        BOOST_THROW_EXCEPTION(
            JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR, boost::diagnostic_information(e)));
    }

    return Json::Value();
}

Json::Value Rpc::getGroupPeers(int _groupID)
{
    try
    {
        RPC_LOG(INFO) << LOG_BADGE("getGroupPeers") << LOG_DESC("request")
                      << LOG_KV("groupID", _groupID);

        Json::Value response = Json::Value(Json::arrayValue);

        auto _nodeList = service()->getNodeListByGroupID(_groupID);

        for (auto it = _nodeList.begin(); it != _nodeList.end(); ++it)
        {
            response.append((*it).hex());
        }

        return response;
    }
    catch (JsonRpcException& e)
    {
        throw e;
    }
    catch (std::exception& e)
    {
        BOOST_THROW_EXCEPTION(
            JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR, boost::diagnostic_information(e)));
    }

    return Json::Value();
}

Json::Value Rpc::getGroupList()
{
    try
    {
        RPC_LOG(INFO) << LOG_BADGE("getGroupList") << LOG_DESC("request");

        Json::Value response = Json::Value(Json::arrayValue);

        auto groupList = ledgerManager()->getGrouplList();
        for (dev::GROUP_ID id : groupList)
            response.append(id);

        return response;
    }
    catch (std::exception& e)
    {
        BOOST_THROW_EXCEPTION(
            JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR, boost::diagnostic_information(e)));
    }
}


Json::Value Rpc::getBlockByHash(
    int _groupID, const std::string& _blockHash, bool _includeTransactions)
{
    try
    {
        RPC_LOG(INFO) << LOG_BADGE("getBlockByHash") << LOG_DESC("request")
                      << LOG_KV("groupID", _groupID) << LOG_KV("blockHash", _blockHash)
                      << LOG_KV("includeTransaction", _includeTransactions);

        Json::Value response;

        auto blockchain = ledgerManager()->blockChain(_groupID);

        if (!blockchain)
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::GroupID, RPCMsg[RPCExceptionType::GroupID]));

        h256 hash = jsToFixed<32>(_blockHash);
        auto block = blockchain->getBlockByHash(hash);
        if (!block)
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::BlockHash, RPCMsg[RPCExceptionType::BlockHash]));

        response["number"] = toJS(block->header().number());
        response["hash"] = _blockHash;
        response["parentHash"] = toJS(block->header().parentHash());
        response["logsBloom"] = toJS(block->header().logBloom());
        response["transactionsRoot"] = toJS(block->header().transactionsRoot());
        response["stateRoot"] = toJS(block->header().stateRoot());
        response["sealer"] = toJS(block->header().sealer());
        response["extraData"] = Json::Value(Json::arrayValue);
        auto datas = block->header().extraData();
        for (auto data : datas)
            response["extraData"].append(toJS(data));
        response["gasLimit"] = toJS(block->header().gasLimit());
        response["gasUsed"] = toJS(block->header().gasUsed());
        response["timestamp"] = toJS(block->header().timestamp());
        const Transactions& transactions = block->transactions();
        response["transactions"] = Json::Value(Json::arrayValue);
        for (unsigned i = 0; i < transactions.size(); i++)
        {
            if (_includeTransactions)
                response["transactions"].append(
                    toJson(transactions[i], std::make_pair(hash, i), block->header().number()));
            else
                response["transactions"].append(toJS(transactions[i].sha3()));
        }

        return response;
    }
    catch (JsonRpcException& e)
    {
        throw e;
    }
    catch (std::exception& e)
    {
        BOOST_THROW_EXCEPTION(
            JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR, boost::diagnostic_information(e)));
    }
}


Json::Value Rpc::getBlockByNumber(
    int _groupID, const std::string& _blockNumber, bool _includeTransactions)
{
    try
    {
        RPC_LOG(INFO) << LOG_BADGE("getBlockByNumber") << LOG_DESC("request")
                      << LOG_KV("groupID", _groupID) << LOG_KV("blockNumber", _blockNumber)
                      << LOG_KV("includeTransaction", _includeTransactions);

        Json::Value response;

        BlockNumber number = jsToBlockNumber(_blockNumber);
        auto blockchain = ledgerManager()->blockChain(_groupID);
        if (!blockchain)
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::GroupID, RPCMsg[RPCExceptionType::GroupID]));

        auto block = blockchain->getBlockByNumber(number);
        if (!block)
            BOOST_THROW_EXCEPTION(JsonRpcException(
                RPCExceptionType::BlockNumberT, RPCMsg[RPCExceptionType::BlockNumberT]));

        response["number"] = toJS(number);
        response["hash"] = toJS(block->headerHash());
        response["parentHash"] = toJS(block->header().parentHash());
        response["logsBloom"] = toJS(block->header().logBloom());
        response["transactionsRoot"] = toJS(block->header().transactionsRoot());
        response["stateRoot"] = toJS(block->header().stateRoot());
        response["sealer"] = toJS(block->header().sealer());
        response["extraData"] = Json::Value(Json::arrayValue);
        auto datas = block->header().extraData();
        for (auto data : datas)
            response["extraData"].append(toJS(data));
        response["gasLimit"] = toJS(block->header().gasLimit());
        response["gasUsed"] = toJS(block->header().gasUsed());
        response["timestamp"] = toJS(block->header().timestamp());
        const Transactions& transactions = block->transactions();
        response["transactions"] = Json::Value(Json::arrayValue);
        for (unsigned i = 0; i < transactions.size(); i++)
        {
            if (_includeTransactions)
                response["transactions"].append(toJson(transactions[i],
                    std::make_pair(block->headerHash(), i), block->header().number()));
            else
                response["transactions"].append(toJS(transactions[i].sha3()));
        }

        return response;
    }
    catch (JsonRpcException& e)
    {
        throw e;
    }
    catch (std::exception& e)
    {
        BOOST_THROW_EXCEPTION(
            JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR, boost::diagnostic_information(e)));
    }
}

std::string Rpc::getBlockHashByNumber(int _groupID, const std::string& _blockNumber)
{
    try
    {
        RPC_LOG(INFO) << LOG_BADGE("getBlockHashByNumber") << LOG_DESC("request")
                      << LOG_KV("groupID", _groupID) << LOG_KV("blockNumber", _blockNumber);

        auto blockchain = ledgerManager()->blockChain(_groupID);
        if (!blockchain)
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::GroupID, RPCMsg[RPCExceptionType::GroupID]));

        BlockNumber number = jsToBlockNumber(_blockNumber);
        h256 blockHash = blockchain->numberHash(number);
        return toJS(blockHash);
    }
    catch (JsonRpcException& e)
    {
        throw e;
    }
    catch (std::exception& e)
    {
        BOOST_THROW_EXCEPTION(
            JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR, boost::diagnostic_information(e)));
    }
}

Json::Value Rpc::getTransactionByHash(int _groupID, const std::string& _transactionHash)
{
    try
    {
        RPC_LOG(INFO) << LOG_BADGE("getTransactionByHash") << LOG_DESC("request")
                      << LOG_KV("groupID", _groupID) << LOG_KV("transactionHash", _transactionHash);

        Json::Value response;
        auto blockchain = ledgerManager()->blockChain(_groupID);
        if (!blockchain)
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::GroupID, RPCMsg[RPCExceptionType::GroupID]));

        h256 hash = jsToFixed<32>(_transactionHash);
        auto tx = blockchain->getLocalisedTxByHash(hash);
        if (tx.blockNumber() == INVALIDNUMBER)
            return Json::nullValue;

        response["blockHash"] = toJS(tx.blockHash());
        response["blockNumber"] = toJS(tx.blockNumber());
        response["from"] = toJS(tx.from());
        response["gas"] = toJS(tx.gas());
        response["gasPrice"] = toJS(tx.gasPrice());
        response["hash"] = toJS(hash);
        response["input"] = toJS(tx.data());
        response["nonce"] = toJS(tx.nonce());
        response["to"] = toJS(tx.to());
        response["transactionIndex"] = toJS(tx.transactionIndex());
        response["value"] = toJS(tx.value());

        return response;
    }
    catch (JsonRpcException& e)
    {
        throw e;
    }
    catch (std::exception& e)
    {
        BOOST_THROW_EXCEPTION(
            JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR, boost::diagnostic_information(e)));
    }
}


Json::Value Rpc::getTransactionByBlockHashAndIndex(
    int _groupID, const std::string& _blockHash, const std::string& _transactionIndex)
{
    try
    {
        RPC_LOG(INFO) << LOG_BADGE("getTransactionByBlockHashAndIndex") << LOG_DESC("request")
                      << LOG_KV("groupID", _groupID) << LOG_KV("blockHash", _blockHash)
                      << LOG_KV("transactionIndex", _transactionIndex);

        Json::Value response;

        auto blockchain = ledgerManager()->blockChain(_groupID);
        if (!blockchain)
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::GroupID, RPCMsg[RPCExceptionType::GroupID]));

        h256 hash = jsToFixed<32>(_blockHash);
        auto block = blockchain->getBlockByHash(hash);
        if (!block)
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::BlockHash, RPCMsg[RPCExceptionType::BlockHash]));

        const Transactions& transactions = block->transactions();
        unsigned int txIndex = jsToInt(_transactionIndex);
        if (txIndex >= transactions.size())
            BOOST_THROW_EXCEPTION(JsonRpcException(
                RPCExceptionType::TransactionIndex, RPCMsg[RPCExceptionType::TransactionIndex]));

        Transaction tx = transactions[txIndex];
        response["blockHash"] = _blockHash;
        response["blockNumber"] = toJS(block->header().number());
        response["from"] = toJS(tx.from());
        response["gas"] = toJS(tx.gas());
        response["gasPrice"] = toJS(tx.gasPrice());
        response["hash"] = toJS(tx.sha3());
        response["input"] = toJS(tx.data());
        response["nonce"] = toJS(tx.nonce());
        response["to"] = toJS(tx.to());
        response["transactionIndex"] = toJS(txIndex);
        response["value"] = toJS(tx.value());

        return response;
    }
    catch (JsonRpcException& e)
    {
        throw e;
    }
    catch (std::exception& e)
    {
        BOOST_THROW_EXCEPTION(
            JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR, boost::diagnostic_information(e)));
    }
}


Json::Value Rpc::getTransactionByBlockNumberAndIndex(
    int _groupID, const std::string& _blockNumber, const std::string& _transactionIndex)
{
    try
    {
        RPC_LOG(INFO) << LOG_BADGE("getTransactionByBlockHashAndIndex") << LOG_DESC("request")
                      << LOG_KV("groupID", _groupID) << LOG_KV("blockNumber", _blockNumber)
                      << LOG_KV("transactionIndex", _transactionIndex);

        Json::Value response;

        auto blockchain = ledgerManager()->blockChain(_groupID);
        if (!blockchain)
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::GroupID, RPCMsg[RPCExceptionType::GroupID]));

        BlockNumber number = jsToBlockNumber(_blockNumber);
        auto block = blockchain->getBlockByNumber(number);
        if (!block)
            BOOST_THROW_EXCEPTION(JsonRpcException(
                RPCExceptionType::BlockNumberT, RPCMsg[RPCExceptionType::BlockNumberT]));

        const Transactions& transactions = block->transactions();
        unsigned int txIndex = jsToInt(_transactionIndex);
        if (txIndex >= transactions.size())
            BOOST_THROW_EXCEPTION(JsonRpcException(
                RPCExceptionType::TransactionIndex, RPCMsg[RPCExceptionType::TransactionIndex]));

        Transaction tx = transactions[txIndex];
        response["blockHash"] = toJS(block->header().hash());
        response["blockNumber"] = toJS(block->header().number());
        response["from"] = toJS(tx.from());
        response["gas"] = toJS(tx.gas());
        response["gasPrice"] = toJS(tx.gasPrice());
        response["hash"] = toJS(tx.sha3());
        response["input"] = toJS(tx.data());
        response["nonce"] = toJS(tx.nonce());
        response["to"] = toJS(tx.to());
        response["transactionIndex"] = toJS(txIndex);
        response["value"] = toJS(tx.value());

        return response;
    }
    catch (JsonRpcException& e)
    {
        throw e;
    }
    catch (std::exception& e)
    {
        BOOST_THROW_EXCEPTION(
            JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR, boost::diagnostic_information(e)));
    }
}


Json::Value Rpc::getTransactionReceipt(int _groupID, const std::string& _transactionHash)
{
    try
    {
        RPC_LOG(INFO) << LOG_BADGE("getTransactionByBlockHashAndIndex") << LOG_DESC("request")
                      << LOG_KV("groupID", _groupID) << LOG_KV("transactionHash", _transactionHash);

        Json::Value response;

        auto blockchain = ledgerManager()->blockChain(_groupID);
        if (!blockchain)
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::GroupID, RPCMsg[RPCExceptionType::GroupID]));

        h256 hash = jsToFixed<32>(_transactionHash);
        auto txReceipt = blockchain->getLocalisedTxReceiptByHash(hash);
        if (txReceipt.blockNumber() == INVALIDNUMBER)
            return Json::nullValue;

        response["transactionHash"] = _transactionHash;
        response["transactionIndex"] = toJS(txReceipt.transactionIndex());
        response["blockNumber"] = toJS(txReceipt.blockNumber());
        response["blockHash"] = toJS(txReceipt.blockHash());
        response["from"] = toJS(txReceipt.from());
        response["to"] = toJS(txReceipt.to());
        response["gasUsed"] = toJS(txReceipt.gasUsed());
        response["contractAddress"] = toJS(txReceipt.contractAddress());
        response["logs"] = Json::Value(Json::arrayValue);
        for (unsigned int i = 0; i < txReceipt.log().size(); ++i)
        {
            Json::Value log;
            log["address"] = toJS(txReceipt.log()[i].address);
            log["topics"] = Json::Value(Json::arrayValue);
            for (unsigned int j = 0; j < txReceipt.log()[i].topics.size(); ++j)
                log["topics"].append(toJS(txReceipt.log()[i].topics[j]));
            log["data"] = toJS(txReceipt.log()[i].data);
            response["logs"].append(log);
        }
        response["logsBloom"] = toJS(txReceipt.bloom());
        response["status"] = toJS(txReceipt.status());
        response["output"] = toJS(txReceipt.outputBytes());

        return response;
    }
    catch (JsonRpcException& e)
    {
        throw e;
    }
    catch (std::exception& e)
    {
        BOOST_THROW_EXCEPTION(
            JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR, boost::diagnostic_information(e)));
    }
}


Json::Value Rpc::getPendingTransactions(int _groupID)
{
    try
    {
        RPC_LOG(INFO) << LOG_BADGE("getPendingTransactions") << LOG_DESC("request")
                      << LOG_KV("groupID", _groupID);

        Json::Value response;

        auto txPool = ledgerManager()->txPool(_groupID);
        if (!txPool)
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::GroupID, RPCMsg[RPCExceptionType::GroupID]));

        response = Json::Value(Json::arrayValue);
        Transactions transactions = txPool->pendingList();
        for (size_t i = 0; i < transactions.size(); ++i)
        {
            auto tx = transactions[i];
            Json::Value txJson;
            txJson["from"] = toJS(tx.from());
            txJson["gas"] = toJS(tx.gas());
            txJson["gasPrice"] = toJS(tx.gasPrice());
            txJson["hash"] = toJS(tx.sha3());
            txJson["input"] = toJS(tx.data());
            txJson["nonce"] = toJS(tx.nonce());
            txJson["to"] = toJS(tx.to());
            txJson["value"] = toJS(tx.value());
            response.append(txJson);
        }

        return response;
    }
    catch (JsonRpcException& e)
    {
        throw e;
    }
    catch (std::exception& e)
    {
        BOOST_THROW_EXCEPTION(
            JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR, boost::diagnostic_information(e)));
    }
}

std::string Rpc::getCode(int _groupID, const std::string& _address)
{
    try
    {
        RPC_LOG(INFO) << LOG_BADGE("getCode") << LOG_DESC("request") << LOG_KV("groupID", _groupID)
                      << LOG_KV("address", _address);

        auto blockChain = ledgerManager()->blockChain(_groupID);
        if (!blockChain)
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::GroupID, RPCMsg[RPCExceptionType::GroupID]));

        return toJS(blockChain->getCode(jsToAddress(_address)));
    }
    catch (JsonRpcException& e)
    {
        throw e;
    }
    catch (std::exception& e)
    {
        BOOST_THROW_EXCEPTION(
            JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR, boost::diagnostic_information(e)));
    }
}

Json::Value Rpc::getTotalTransactionCount(int _groupID)
{
    try
    {
        RPC_LOG(INFO) << LOG_BADGE("getTotalTransactionCount") << LOG_DESC("request")
                      << LOG_KV("groupID", _groupID);

        auto blockChain = ledgerManager()->blockChain(_groupID);
        if (!blockChain)
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::GroupID, RPCMsg[RPCExceptionType::GroupID]));

        Json::Value response;
        std::pair<int64_t, int64_t> result = blockChain->totalTransactionCount();
        response["txSum"] = toJS(result.first);
        response["blockNumber"] = toJS(result.second);
        return response;
    }
    catch (JsonRpcException& e)
    {
        throw e;
    }
    catch (std::exception& e)
    {
        BOOST_THROW_EXCEPTION(
            JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR, boost::diagnostic_information(e)));
    }
}

Json::Value Rpc::call(int _groupID, const Json::Value& request)
{
    try
    {
        RPC_LOG(INFO) << LOG_BADGE("call") << LOG_DESC("request") << LOG_KV("groupID", _groupID)
                      << LOG_KV("callParams", request.toStyledString());

        if (request["from"].empty() || request["from"].asString().empty())
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::CallFrom, RPCMsg[RPCExceptionType::CallFrom]));

        auto blockchain = ledgerManager()->blockChain(_groupID);
        auto blockverfier = ledgerManager()->blockVerifier(_groupID);
        if (!blockchain || !blockverfier)
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::GroupID, RPCMsg[RPCExceptionType::GroupID]));

        BlockNumber blockNumber = blockchain->number();
        auto block = blockchain->getBlockByNumber(blockNumber);
        if (!block)
            BOOST_THROW_EXCEPTION(JsonRpcException(
                RPCExceptionType::BlockNumberT, RPCMsg[RPCExceptionType::BlockNumberT]));

        TransactionSkeleton txSkeleton = toTransactionSkeleton(request);
        Transaction tx(txSkeleton.value, gasPrice, maxTransactionGasLimit, txSkeleton.to,
            txSkeleton.data, txSkeleton.nonce);
        auto blockHeader = block->header();
        tx.forceSender(txSkeleton.from);
        auto executionResult = blockverfier->executeTransaction(blockHeader, tx);

        Json::Value response;
        response["currentBlockNumber"] = toJS(blockNumber);
        response["output"] = toJS(executionResult.second.outputBytes());
        return response;
    }
    catch (JsonRpcException& e)
    {
        throw e;
    }
    catch (std::exception& e)
    {
        BOOST_THROW_EXCEPTION(
            JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR, boost::diagnostic_information(e)));
    }
}

std::string Rpc::sendRawTransaction(int _groupID, const std::string& _rlp)
{
    try
    {
        RPC_LOG(INFO) << LOG_BADGE("sendRawTransaction") << LOG_DESC("request")
                      << LOG_KV("groupID", _groupID) << LOG_KV("rlp", _rlp);

        auto txPool = ledgerManager()->txPool(_groupID);
        if (!txPool)
            BOOST_THROW_EXCEPTION(
                JsonRpcException(RPCExceptionType::GroupID, RPCMsg[RPCExceptionType::GroupID]));

        Transaction tx(jsToBytes(_rlp, OnFailed::Throw), CheckTransaction::Everything);
        std::pair<h256, Address> ret = txPool->submit(tx);

        return toJS(ret.first);
    }
    catch (JsonRpcException& e)
    {
        throw e;
    }
    catch (std::exception& e)
    {
        BOOST_THROW_EXCEPTION(
            JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR, boost::diagnostic_information(e)));
    }
}
