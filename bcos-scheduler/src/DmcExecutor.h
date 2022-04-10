/*
 *  Copyright (C) 2022 FISCO BCOS.
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
 * @brief The executor of a contract
 * @file DmcExecutor.h
 * @author: jimmyshi
 * @date: 2022-03-23
 */


#pragma once

#include "Executive.h"
#include "ExecutorManager.h"
#include "GraphKeyLocks.h"
#include <bcos-framework/interfaces/protocol/Block.h>
#include <tbb/concurrent_set.h>
#include <tbb/concurrent_unordered_map.h>
#include <string>

namespace bcos::scheduler
{
class DmcExecutor
{
public:
    enum Status : int8_t
    {
        ERROR,
        NEED_PREPARE = 1,
        PAUSED = 2,
        FINISHED = 3
    };

    using Ptr = std::shared_ptr<DmcExecutor>;

    DmcExecutor(std::string contractAddress, bcos::protocol::Block::Ptr block,
        bcos::executor::ParallelTransactionExecutorInterface::Ptr executor,
        GraphKeyLocks::Ptr keyLocks, bcos::crypto::Hash::Ptr hashImpl)
      : m_contractAddress(contractAddress),
        m_block(block),
        m_executor(executor),
        m_keyLocks(keyLocks),
        m_hashImpl(hashImpl)
    {}

    void submit(protocol::ExecutionMessage::UniquePtr message, bool withDAG);
    bool prepare();  // return finished(true). If others need re-prepare, return unfinished(false)
    void go(std::function<void(bcos::Error::UniquePtr, Status)> callback);
    bool hasFinished() { return m_lockingPool.empty() && m_pendingPool.empty(); }

    void schedulerIn(ExecutiveState::Ptr executive);

    void setSchedulerOutHandler(std::function<void(ExecutiveState::Ptr)> onSchedulerOut)
    {
        f_onSchedulerOut = std::move(onSchedulerOut);
    };

    void setOnTxFinishedHandler(
        std::function<void(bcos::protocol::ExecutionMessage::UniquePtr)> onTxFinished)
    {
        f_onTxFinished = std::move(onTxFinished);
    }


    enum MessageHint : int8_t
    {
        NEED_SEND,
        SCHEDULER_OUT,
        LOCKED,
        END
    };

private:
    MessageHint handleExecutiveMessage(ExecutiveState::Ptr executive);
    void handleExecutiveOutputs(std::vector<bcos::protocol::ExecutionMessage::UniquePtr> outputs);
    void schedulerOut(ContextID contextID);
    void removeOutdatedStatus();

    std::string newEVMAddress(int64_t blockNumber, int64_t contextID, int64_t seq);
    std::string newEVMAddress(
        const std::string_view& _sender, bytesConstRef _init, u256 const& _salt);

private:
    std::string m_contractAddress;
    bcos::protocol::Block::Ptr m_block;
    bcos::executor::ParallelTransactionExecutorInterface::Ptr m_executor;
    GraphKeyLocks::Ptr m_keyLocks;
    bcos::crypto::Hash::Ptr m_hashImpl;
    tbb::concurrent_unordered_map<ContextID, ExecutiveState::Ptr> m_pendingPool;
    tbb::concurrent_set<ContextID> m_needPrepare;
    tbb::concurrent_set<ContextID> m_lockingPool;
    tbb::concurrent_set<ContextID> m_needSendPool;
    tbb::concurrent_set<ContextID> m_needRemove;


    mutable SharedMutex x_concurrentLock;

    std::function<void(bcos::protocol::ExecutionMessage::UniquePtr)> f_onTxFinished;
    std::function<void(ExecutiveState::Ptr)> f_onSchedulerOut;
};
}  // namespace bcos::scheduler
