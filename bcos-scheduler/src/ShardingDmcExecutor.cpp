//
// Created by Jimmy Shi on 2023/1/7.
//

#include "ShardingDmcExecutor.h"
using namespace bcos::scheduler;

void ShardingDmcExecutor::go(std::function<void(bcos::Error::UniquePtr, Status)> callback)
{
    {  // waiting for preExecute finish
        auto preExecuteGuard = bcos::WriteGuard(x_preExecute);
    }

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
    auto messages = std::make_shared<std::vector<protocol::ExecutionMessage::UniquePtr>>();

    m_executivePool.forEach(MessageHint::NEED_SEND,
        [this, messages](int64_t contextID, ExecutiveState::Ptr executiveState) {
            auto& message = executiveState->message;

            auto keyLocks = m_keyLocks->getKeyLocksNotHoldingByContext(message->to(), contextID);
            message->setKeyLocks(std::move(keyLocks));
            DMC_LOG(TRACE) << " SendPrepareToExecutor:\t >>>> " << executiveState->toString()
                           << " >>>> [" << m_name << "]:" << m_contractAddress
                           << ", staticCall:" << message->staticCall();
            messages->push_back(std::move(message));

            return true;
        });

    DMC_LOG(DEBUG) << LOG_BADGE("Sharding") << "send preExecute message" << LOG_KV("name", m_name)
                   << LOG_KV("contract", m_contractAddress) << LOG_KV("txNum", messages->size())
                   << LOG_KV("blockNumber", m_block->blockHeader()->number());

    auto preExecuteGuard = std::make_shared<bcos::WriteGuard>(x_preExecute);

    m_executor->preExecuteTransactions(m_schedulerTermId, m_block->blockHeaderConst(),
        m_contractAddress, *messages, [&, preExecuteGuard](bcos::Error::UniquePtr error) {
            if (error)
            {
                DMC_LOG(DEBUG) << LOG_BADGE("Sharding")
                               << "send preExecute message error:" << error->errorMessage()
                               << LOG_KV("name", m_name) << LOG_KV("contract", m_contractAddress)
                               << LOG_KV("blockNumber", m_block->blockHeader()->number());
            }
            else
            {
                DMC_LOG(DEBUG) << LOG_BADGE("Sharding") << "send preExecute message success "
                               << LOG_KV("name", m_name) << LOG_KV("contract", m_contractAddress)
                               << LOG_KV("blockNumber", m_block->blockHeader()->number());
            }
        });
};