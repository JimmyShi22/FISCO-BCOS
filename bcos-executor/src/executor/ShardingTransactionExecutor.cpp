/*
 *  Copyright (C) 2023 FISCO BCOS.
 *  SPDX-License-Identifier: Apache-2.0
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 * @brief ShardingExecutor
 * @file ShardingExecutor.cpp
 * @author: JimmyShi22
 * @date: 2023-01-07
 */

#include "ShardingTransactionExecutor.h"
#include "../executive/ExecutiveDagFlow.h"
#include "../executive/ExecutiveFactory.h"
#include <bcos-framework/executor/ExecuteError.h>

using namespace std;
using namespace bcos::executor;
using namespace bcos::protocol;

void ShardingTransactionExecutor::executeTransactions(std::string contractAddress,
    gsl::span<bcos::protocol::ExecutionMessage::UniquePtr> inputs,
    std::function<void(
        bcos::Error::UniquePtr, std::vector<bcos::protocol::ExecutionMessage::UniquePtr>)>
        callback)
{
    if (m_blockVersion >= uint32_t(bcos::protocol::BlockVersion::V3_3_VERSION))
    {
        do
        {
            if (!m_blockContext)
            {
                TransactionExecutor::executeTransactions(
                    contractAddress, std::move(inputs), std::move(callback));
                break;
            }

            // get dagFlow from cache
            auto number = m_blockContext->number();
            auto timestamp = m_blockContext->timestamp();
            PreExeCache::Ptr cache;
            {
                // get dagFlow if it has been prepared beforehand
                ReadGuard l(x_preparedCache);
                auto itr = m_preparedCache.find({number, timestamp, contractAddress});

                if (itr != m_preparedCache.end())
                {
                    cache = itr->second;
                }
            }

            if (!cache)
            {
                EXECUTOR_NAME_LOG(DEBUG)
                    << LOG_BADGE("preExeBlock")
                    << "Not hit prepared dagFlow cache, execute directly"
                    << LOG_KV("number", number) << LOG_KV("timestamp", timestamp)
                    << LOG_KV("shard", contractAddress);
                TransactionExecutor::executeTransactions(
                    contractAddress, std::move(inputs), std::move(callback));
                break;
            }

            auto executiveFlow = getExecutiveFlow(m_blockContext, contractAddress, true, false);
            auto dagExecutiveFlow = std::dynamic_pointer_cast<ExecutiveDagFlow>(executiveFlow);
            {
                // waiting cache initialize finish
                EXECUTOR_NAME_LOG(TRACE)
                    << LOG_BADGE("preExeBlock") << " waiting for dagFlow cache preparing"
                    << LOG_KV("number", number) << LOG_KV("timestamp", timestamp)
                    << LOG_KV("codeAddress", contractAddress);
                bcos::ReadGuard cacheGuard(cache->x_cache);
                dagExecutiveFlow->setDagFlowIfNotExists(cache->dagFlow);
                dagExecutiveFlow->submit(cache->inputs);
            }


            EXECUTOR_NAME_LOG(DEBUG)
                << LOG_BADGE("preExeBlock") << " hit prepared dagFlow cache, use it"
                << LOG_KV("number", number) << LOG_KV("timestamp", timestamp)
                << LOG_KV("codeAddress", contractAddress);

            auto recoredT = utcTime();
            asyncExecuteExecutiveFlow(executiveFlow,
                [callback = std::move(callback)](bcos::Error::UniquePtr&& error,
                    std::vector<bcos::protocol::ExecutionMessage::UniquePtr>&& messages) {
                    callback(std::move(error), std::move(messages));
                });
            EXECUTOR_NAME_LOG(DEBUG) << LOG_DESC("ShardingTransactionExecutor execute transaction")
                                     << LOG_KV("cost", (utcTime() - recoredT));
        } while (false);
    }
    else
    {
        TransactionExecutor::executeTransactions(
            contractAddress, std::move(inputs), std::move(callback));
    }

    // clear prepared cache
    WriteGuard l(x_preparedCache);
    auto itr = m_preparedCache.begin();
    while (itr != m_preparedCache.end() && m_blockContext)
    {
        auto number = get<0>(itr->first);
        if (number < m_blockContext->number())
        {
            EXECUTOR_NAME_LOG(DEBUG)
                << LOG_BADGE("preExeBlock") << LOG_BADGE("DAGFlow")
                << "clear prepared dagFlow cache" << LOG_KV("blockNumber", get<0>(itr->first))
                << LOG_KV("timestamp", get<1>(itr->first)) << LOG_KV("shard", get<2>(itr->first));
            itr = m_preparedCache.erase(itr);
        }
        else
        {
            itr++;
        }
    }
}

BlockContext::Ptr ShardingTransactionExecutor::createTmpBlockContext(
    const protocol::BlockHeader::ConstPtr& currentHeader)
{
    bcos::storage::StateStorageInterface::Ptr stateStorage;

    if (m_cachedStorage)
    {
        stateStorage = createStateStorage(m_cachedStorage);
    }
    else
    {
        stateStorage = createStateStorage(m_backendStorage);
    }

    return createBlockContext(currentHeader, stateStorage);
}

void ShardingTransactionExecutor::preExecuteTransactions(int64_t schedulerTermId,
    const bcos::protocol::BlockHeader::ConstPtr& blockHeader, std::string contractAddress,
    gsl::span<bcos::protocol::ExecutionMessage::UniquePtr> inputs,
    std::function<void(bcos::Error::UniquePtr)> callback)
{
    if (!m_isRunning)
    {
        EXECUTOR_NAME_LOG(ERROR) << "TransactionExecutor is not running";
        callback(
            BCOS_ERROR_UNIQUE_PTR(ExecuteError::STOPPED, "TransactionExecutor is not running"));
        return;
    }

    if (blockHeader->version() >= uint32_t(bcos::protocol::BlockVersion::V3_3_VERSION))
    {
        auto blockContext = createTmpBlockContext(blockHeader);
        auto executiveFactory = std::make_shared<ExecutiveFactory>(blockContext,
            m_precompiledContract, m_constantPrecompiled, m_builtInPrecompiled, m_gasInjector);

        auto txNum = inputs.size();
        auto blockNumber = blockHeader->number();
        auto timestamp = blockHeader->timestamp();

        EXECUTOR_NAME_LOG(DEBUG) << LOG_BADGE("preExeBlock") << LOG_BADGE("DAGFlow")
                                 << "preExecuteTransactions start"
                                 << LOG_KV("blockNumber", blockNumber)
                                 << LOG_KV("timestamp", timestamp);

        PreExeCache::Ptr cache = std::make_shared<PreExeCache>();
        std::shared_ptr<bcos::WriteGuard> cacheGuard = nullptr;
        bool hasPrepared = false;
        {
            WriteGuard l(x_preparedCache);
            std::tuple<bcos::protocol::BlockNumber, int64_t, const std::string> key = {
                blockNumber, timestamp, contractAddress};
            hasPrepared = !m_preparedCache.try_emplace(key, cache).second;

            if (!hasPrepared)
            {
                // acquire lock start to prepare
                cacheGuard = std::make_shared<bcos::WriteGuard>(cache->x_cache);
            }
        }

        if (hasPrepared)
        {
            EXECUTOR_NAME_LOG(DEBUG)
                << LOG_BADGE("preExeBlock") << LOG_BADGE("DAGFlow")
                << "preExecuteTransactions: dagFlow has been prepared"
                << LOG_KV("blockNumber", blockNumber) << LOG_KV("timestamp", timestamp);
            callback(
                BCOS_ERROR_UNIQUE_PTR(ExecuteError::EXECUTE_ERROR, "dagFlow has been prepared"));
            return;
        }


        auto recoredT = utcTime();
        auto startT = utcTime();

        // for fill block
        auto txHashes = std::make_shared<HashList>();
        std::vector<decltype(inputs)::size_type> indexes;
        auto fillInputs =
            std::make_shared<std::vector<bcos::protocol::ExecutionMessage::UniquePtr>>();

        // final result
        auto callParametersList =
            std::make_shared<std::vector<CallParameters::UniquePtr>>(inputs.size());

        bool isStaticCall = true;

        std::mutex writeMutex;
        tbb::parallel_for(tbb::blocked_range<size_t>(0U, inputs.size()), [&, this](
                                                                             auto const& range) {
            for (auto i = range.begin(); i < range.end(); ++i)
            {
                auto& params = inputs[i];

                if (!params->staticCall())
                {
                    isStaticCall = false;
                }

                switch (params->type())
                {
                case ExecutionMessage::TXHASH:
                {
                    std::unique_lock lock(writeMutex);
                    txHashes->emplace_back(params->transactionHash());
                    indexes.emplace_back(i);
                    fillInputs->emplace_back(std::move(params));

                    break;
                }
                case ExecutionMessage::MESSAGE:
                case bcos::protocol::ExecutionMessage::REVERT:
                case bcos::protocol::ExecutionMessage::FINISHED:
                case bcos::protocol::ExecutionMessage::KEY_LOCK:
                {
                    callParametersList->at(i) = createCallParameters(*params, params->staticCall());
                    break;
                }
                default:
                {
                    auto message =
                        (boost::format("Unsupported message type: %d") % params->type()).str();
                    EXECUTOR_NAME_LOG(ERROR)
                        << BLOCK_NUMBER(blockNumber) << "DAG Execute error, " << message;
                    // callback(BCOS_ERROR_UNIQUE_PTR(ExecuteError::DAG_ERROR, message), {});
                    break;
                }
                }
            }
        });

        if (isStaticCall)
        {
            EXECUTOR_NAME_LOG(FATAL)
                << "preExecuteTransactions only handle non static transactions but "
                   "receive static call";
            assert(false);
        }

        auto prepareT = utcTime() - startT;
        startT = utcTime();

        if (!txHashes->empty())
        {
            m_txpool->asyncFillBlock(txHashes,
                [this, cache, cacheGuard, blockContext, executiveFactory, startT, contractAddress,
                    timestamp, indexes = std::move(indexes), fillInputs = std::move(fillInputs),
                    callParametersList = std::move(callParametersList),
                    callback = std::move(callback), txHashes,
                    blockNumber](Error::Ptr error, protocol::TransactionsPtr transactions) mutable {
                    auto fillTxsT = (utcTime() - startT);

                    if (!m_isRunning)
                    {
                        callback(BCOS_ERROR_UNIQUE_PTR(
                            ExecuteError::STOPPED, "TransactionExecutor is not running"));
                        return;
                    }


                    if (error)
                    {
                        auto errorMessage = "[" + m_name + "] asyncFillBlock failed";
                        EXECUTOR_NAME_LOG(ERROR)
                            << BLOCK_NUMBER(blockNumber) << errorMessage << error->errorMessage();
                        callback(BCOS_ERROR_WITH_PREV_UNIQUE_PTR(
                            ExecuteError::PREPARE_ERROR, errorMessage, *error));
                        return;
                    }
                    auto recordT = utcTime();
                    tbb::parallel_for(tbb::blocked_range<size_t>(0U, transactions->size()),
                        [this, &transactions, &callParametersList, &indexes, &fillInputs](
                            auto const& range) {
                            for (auto i = range.begin(); i < range.end(); ++i)
                            {
                                assert(transactions->at(i));
                                callParametersList->at(indexes[i]) =
                                    createCallParameters(*fillInputs->at(i), *transactions->at(i));
                            }
                        });

                    auto prepareT = utcTime() - recordT;
                    recordT = utcTime();

                    EXECUTOR_NAME_LOG(DEBUG)
                        << BLOCK_NUMBER(blockNumber) << LOG_BADGE("preExeBlock")
                        << LOG_BADGE("DAGFlow")
                        << LOG_DESC("preExecuteTransaction fillblock finish")
                        << LOG_KV("fillTxsT", fillTxsT) << LOG_KV("prepareT", prepareT)
                        << LOG_KV("blockNumber", blockNumber) << LOG_KV("timestamp", timestamp)
                        << LOG_KV("dmcT", (utcTime() - recordT));
                    recordT = utcTime();

                    auto executiveFlow = std::dynamic_pointer_cast<ExecutiveDagFlow>(
                        getExecutiveFlow(blockContext, contractAddress, true, false));
                    auto dagFlow = executiveFlow->prepareDagFlow(
                        *blockContext, executiveFactory, *callParametersList, m_abiCache);
                    {
                        WriteGuard l(x_preparedCache);

                        cache->inputs = callParametersList;
                        cache->dagFlow = dagFlow;

                        EXECUTOR_NAME_LOG(DEBUG)
                            << BLOCK_NUMBER(blockNumber) << LOG_BADGE("preExeBlock")
                            << LOG_DESC("add prepared cache") << LOG_KV("blockNumber", blockNumber)
                            << LOG_KV("timestamp", timestamp);
                    }
                    EXECUTOR_NAME_LOG(DEBUG)
                        << BLOCK_NUMBER(blockNumber) << LOG_BADGE("preExeBlock")
                        << LOG_DESC("preExecuteTransaction prepareDagFlow finish")
                        << LOG_KV("blockNumber", blockNumber) << LOG_KV("timestamp", timestamp)
                        << LOG_KV("cost", (utcTime() - recordT));

                    callback(nullptr);
                });
        }
        else
        {
            auto recordT = utcTime();
            auto executiveFlow = std::dynamic_pointer_cast<ExecutiveDagFlow>(
                getExecutiveFlow(blockContext, contractAddress, true, false));
            auto dagFlow = executiveFlow->prepareDagFlow(
                *blockContext, executiveFactory, *callParametersList, m_abiCache);
            {
                WriteGuard l(x_preparedCache);

                cache->inputs = callParametersList;
                cache->dagFlow = dagFlow;

                EXECUTOR_NAME_LOG(DEBUG)
                    << BLOCK_NUMBER(blockNumber) << LOG_BADGE("PreExec")
                    << LOG_DESC("add prepared cache") << LOG_KV("blockNumber", blockNumber)
                    << LOG_KV("timestamp", timestamp);
            }
            EXECUTOR_NAME_LOG(DEBUG)
                << BLOCK_NUMBER(blockNumber) << LOG_BADGE("DAGFlow")
                << LOG_DESC("preExecuteTransaction prepareDagFlow finish")
                << LOG_KV("blockNumber", blockNumber) << LOG_KV("timestamp", timestamp)
                << LOG_KV("cost", (utcTime() - recordT));

            callback(nullptr);
        }
    }
    else
    {
        EXECUTOR_NAME_LOG(TRACE) << BLOCK_NUMBER(blockHeader->number()) << LOG_BADGE("DAGFlow")
                                 << LOG_DESC("preExecuteTransaction do nothing");

        callback(nullptr);
    }
}

std::shared_ptr<ExecutiveFlowInterface> ShardingTransactionExecutor::getExecutiveFlow(
    std::shared_ptr<BlockContext> blockContext, std::string codeAddress, bool useCoroutine,
    bool isStaticCall)
{
    if (m_blockVersion >= uint32_t(bcos::protocol::BlockVersion::V3_3_VERSION))
    {
        EXECUTOR_NAME_LOG(DEBUG) << "getExecutiveFlow" << LOG_KV("codeAddress", codeAddress);

        ExecutiveFlowInterface::Ptr executiveFlow = blockContext->getExecutiveFlow(codeAddress);
        if (executiveFlow == nullptr)
        {
            if (!isStaticCall)
            {
                auto executiveFactory =
                    std::make_shared<ShardingExecutiveFactory>(blockContext, m_precompiledContract,
                        m_constantPrecompiled, m_builtInPrecompiled, m_gasInjector);

                executiveFlow = std::make_shared<ExecutiveDagFlow>(executiveFactory, m_abiCache);
                executiveFlow->setThreadPool(m_threadPool);
                blockContext->setExecutiveFlow(codeAddress, executiveFlow);
            }
            else
            {
                // use serial to call
                executiveFlow = TransactionExecutor::getExecutiveFlow(
                    blockContext, codeAddress, false, isStaticCall);
            }
        }
        return executiveFlow;
    }
    else
    {
        return TransactionExecutor::getExecutiveFlow(
            blockContext, codeAddress, useCoroutine, isStaticCall);
    }
}
