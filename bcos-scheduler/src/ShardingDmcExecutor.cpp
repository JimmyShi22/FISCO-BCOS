//
// Created by Jimmy Shi on 2023/1/7.
//

#include "ShardingDmcExecutor.h"
#include <bcos-framework/executor/ExecuteError.h>
using namespace bcos::scheduler;

void ShardingDmcExecutor::submit(protocol::ExecutionMessage::UniquePtr message, bool withDAG)
{
    (void)withDAG;  // no need to use this param
    handleCreateMessage(message, 0);
    m_preparedMessages->emplace_back(std::move(message));
};

void ShardingDmcExecutor::dagGo(std::function<void(bcos::Error::UniquePtr, Status)> callback)
{
    {  // waiting for preExecute finish
        auto preExecuteGuard = bcos::WriteGuard(x_preExecute);
    }

    auto messages = std::move(m_preparedMessages);

    if (messages && messages->size() == 1 && (*messages)[0]->staticCall())
    {
        DMC_LOG(TRACE) << "send call request, address:" << m_contractAddress
                       << LOG_KV("executor", m_name) << LOG_KV("to", (*messages)[0]->to())
                       << LOG_KV("contextID", (*messages)[0]->contextID())
                       << LOG_KV("internalCall", (*messages)[0]->internalCall())
                       << LOG_KV("type", (*messages)[0]->type());
        // is static call
        executorCall(std::move((*messages)[0]),
            [this, callback = std::move(callback)](
                bcos::Error::UniquePtr error, bcos::protocol::ExecutionMessage::UniquePtr output) {
                if (error)
                {
                    SCHEDULER_LOG(ERROR) << "Call error: " << boost::diagnostic_information(*error);

                    if (error->errorCode() == bcos::executor::ExecuteError::SCHEDULER_TERM_ID_ERROR)
                    {
                        triggerSwitch();
                    }
                    callback(std::move(error), ERROR);
                }
                else
                {
                    f_onTxFinished(std::move(output));
                    callback(nullptr, PAUSED);
                }
            });
    }
    else
    {
        auto lastT = utcTime();
        if (!messages)
        {
            DMC_LOG(DEBUG) << LOG_BADGE("Stat")
                           << "DAGExecute:\t --> Send to executor by preExecute cache\t"
                           << LOG_KV("name", m_name) << LOG_KV("shard", m_contractAddress)
                           << LOG_KV("txNum", messages ? messages->size() : 0)
                           << LOG_KV("blockNumber", m_block && m_block->blockHeader() ?
                                                        m_block->blockHeader()->number() :
                                                        0)
                           << LOG_KV("cost", utcTime() - lastT);
            messages = std::make_shared<std::vector<protocol::ExecutionMessage::UniquePtr>>();
        }
        else
        {
            DMC_LOG(DEBUG) << LOG_BADGE("Stat") << "DAGExecute:\t --> Send to executor\t"
                           << LOG_KV("name", m_name) << LOG_KV("shard", m_contractAddress)
                           << LOG_KV("txNum", messages ? messages->size() : 0)
                           << LOG_KV("blockNumber", m_block && m_block->blockHeader() ?
                                                        m_block->blockHeader()->number() :
                                                        0)
                           << LOG_KV("cost", utcTime() - lastT);
        }

        executorExecuteTransactions(m_contractAddress, *messages,
            [this, lastT, messages, callback = std::move(callback)](bcos::Error::UniquePtr error,
                std::vector<bcos::protocol::ExecutionMessage::UniquePtr> outputs) {
                // update batch
                DMC_LOG(DEBUG) << LOG_BADGE("Stat") << "DAGExecute:\t <-- Receive from executor\t"
                               << LOG_KV("name", m_name) << LOG_KV("shard", m_contractAddress)
                               << LOG_KV("txNum", messages ? messages->size() : 0)
                               << LOG_KV("blockNumber", m_block && m_block->blockHeader() ?
                                                            m_block->blockHeader()->number() :
                                                            0)
                               << LOG_KV("cost", utcTime() - lastT);

                if (error)
                {
                    SCHEDULER_LOG(ERROR)
                        << "DAGExecute transaction error: " << error->errorMessage();

                    if (error->errorCode() == bcos::executor::ExecuteError::SCHEDULER_TERM_ID_ERROR)
                    {
                        triggerSwitch();
                    }

                    callback(std::move(error), Status::ERROR);
                }
                else
                {
                    for (auto& output : outputs)
                    {
                        if (output->type() == protocol::ExecutionMessage::FINISHED ||
                            output->type() == protocol::ExecutionMessage::REVERT)
                        {
                            f_onTxFinished(std::move(output));
                        }
                        else
                        {
                            DmcExecutor::submit(std::move(output), false);
                        }
                    }
                    callback(nullptr, Status::FINISHED);
                }
            });
    }
}

void ShardingDmcExecutor::go(std::function<void(bcos::Error::UniquePtr, Status)> callback)
{
    DmcExecutor::go(std::move(callback));
}


void ShardingDmcExecutor::executorCall(bcos::protocol::ExecutionMessage::UniquePtr input,
    std::function<void(bcos::Error::UniquePtr, bcos::protocol::ExecutionMessage::UniquePtr)>
        callback)
{
    m_executor->call(std::move(input), std::move(callback));
};

void ShardingDmcExecutor::executorExecuteTransactions(std::string contractAddress,
    gsl::span<bcos::protocol::ExecutionMessage::UniquePtr> inputs,

    // called every time at all tx stop( pause or finish)
    std::function<void(
        bcos::Error::UniquePtr, std::vector<bcos::protocol::ExecutionMessage::UniquePtr>)>
        callback)
{
    m_executor->executeTransactions(
        std::move(contractAddress), std::move(inputs), std::move(callback));
};


void ShardingDmcExecutor::preExecute()
{
    DMC_LOG(DEBUG) << LOG_BADGE("Sharding") << "send preExecute message" << LOG_KV("name", m_name)
                   << LOG_KV("contract", m_contractAddress)
                   << LOG_KV("txNum", m_preparedMessages->size())
                   << LOG_KV("blockNumber", m_block->blockHeader()->number())
                   << LOG_KV("timestamp", m_block->blockHeader()->timestamp());

    auto preExecuteGuard = std::make_shared<bcos::WriteGuard>(x_preExecute);

    auto message = std::move(m_preparedMessages);

    m_executor->preExecuteTransactions(m_schedulerTermId, m_block->blockHeaderConst(),
        m_contractAddress, *message, [&, preExecuteGuard](bcos::Error::UniquePtr error) {
            if (error)
            {
                DMC_LOG(DEBUG) << LOG_BADGE("Sharding")
                               << "send preExecute message error:" << error->errorMessage()
                               << LOG_KV("name", m_name) << LOG_KV("contract", m_contractAddress)
                               << LOG_KV("blockNumber", m_block->blockHeader()->number())
                               << LOG_KV("timestamp", m_block->blockHeader()->timestamp());
            }
            else
            {
                DMC_LOG(DEBUG) << LOG_BADGE("Sharding") << "send preExecute message success "
                               << LOG_KV("name", m_name) << LOG_KV("contract", m_contractAddress)
                               << LOG_KV("blockNumber", m_block->blockHeader()->number())
                               << LOG_KV("timestamp", m_block->blockHeader()->timestamp());
            }
        });
};