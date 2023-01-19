//
// Created by Jimmy Shi on 2023/1/7.
//

#include "ExecutiveDagFlow.h"
#include "TransactionExecutive.h"
#include <bcos-framework/executor/ExecuteError.h>

using namespace bcos;
using namespace bcos::executor;

#define DAGFLOW_LOG(OBV) EXECUTOR_LOG(OBV) << LOG_BADGE("DAGFlow")

void ExecutiveDagFlow::submit(CallParameters::UniquePtr txInput)
{
    bcos::RecursiveGuard lock(x_lock);
    if (m_pausedExecutive)
    {
        DAGFLOW_LOG(DEBUG) << "submit: resume tx" << txInput->toString();
        // is tx to resume
        assert(m_pausedExecutive->getContextID() == txInput->contextID);
        m_pausedExecutive->setResumeParam(std::move(txInput));
        return;
    }

    DAGFLOW_LOG(DEBUG) << "submit: new tx" << txInput->toString();
    // is new tx
    auto contextID = txInput->contextID;

    m_inputs.emplace(contextID, std::move(txInput));
};
void ExecutiveDagFlow::submit(std::shared_ptr<std::vector<CallParameters::UniquePtr>> txInputs)
{
    bcos::RecursiveGuard lock(x_lock);
    for (auto& txInput : *txInputs)
    {
        submit(std::move(txInput));
    }
};

void ExecutiveDagFlow::asyncRun(std::function<void(CallParameters::UniquePtr)> onTxReturn,
    std::function<void(bcos::Error::UniquePtr)> onFinished)
{
    try
    {
        auto self = std::weak_ptr<ExecutiveDagFlow>(shared_from_this());
        asyncTo([self, onTxReturn = std::move(onTxReturn), onFinished = std::move(onFinished)]() {
            try
            {
                auto flow = self.lock();
                if (flow)
                {
                    flow->run(onTxReturn, onFinished);
                }
            }
            catch (std::exception& e)
            {
                onFinished(BCOS_ERROR_UNIQUE_PTR(ExecuteError::EXECUTE_ERROR,
                    "ExecutiveDagFlow asyncRun exception:" + std::string(e.what())));
            }
        });
    }
    catch (std::exception const& e)
    {
        onFinished(BCOS_ERROR_UNIQUE_PTR(ExecuteError::EXECUTE_ERROR,
            "ExecutiveDagFlow asyncTo exception:" + std::string(e.what())));
    }
}

void ExecutiveDagFlow::run(std::function<void(CallParameters::UniquePtr)> onTxReturn,
    std::function<void(bcos::Error::UniquePtr)> onFinished)
{
    try
    {
        auto contextID =
            m_pausedExecutive ? m_pausedExecutive->getContextID() : m_inputs.begin()->first;
        for (; contextID < (int64_t)m_inputs.size(); contextID++)
        {
            if (!m_isRunning)
            {
                DAGFLOW_LOG(DEBUG) << "ExecutiveDagFlow has stopped during running";
                onFinished(BCOS_ERROR_UNIQUE_PTR(
                    ExecuteError::STOPPED, "ExecutiveDagFlow has stopped during running"));
                return;
            }


            // run evm
            ExecutiveState::Ptr executiveState;
            if (!m_pausedExecutive)
            {
                auto& txInput = m_inputs[contextID];
                executiveState =
                    std::make_shared<ExecutiveState>(m_executiveFactory, std::move(txInput));
                m_inputs.erase(contextID);
            }
            else
            {
                executiveState = std::move(m_pausedExecutive);
            }

            auto seq = executiveState->getSeq();
            DAGFLOW_LOG(DEBUG) << "Execute tx:" << contextID << " | " << seq;

            auto output = executiveState->go();

            // set result
            output->contextID = contextID;
            output->seq = seq;

            if (output->type == CallParameters::MESSAGE || output->type == CallParameters::KEY_LOCK)
            {
                m_pausedExecutive = std::move(executiveState);
                // call back
                DAGFLOW_LOG(DEBUG) << "execute tx externalCall" << output->toString();
                onTxReturn(std::move(output));
                break;
            }

            // call back
            DAGFLOW_LOG(DEBUG) << "execute tx finish" << output->toString();
            onTxReturn(std::move(output));
        }

        onFinished(nullptr);
    }
    catch (std::exception& e)
    {
        EXECUTIVE_LOG(ERROR) << "ExecutiveDagFlow run error: " << boost::diagnostic_information(e);
        onFinished(BCOS_ERROR_WITH_PREV_UNIQUE_PTR(-1, "ExecutiveDagFlow run error", e));
    }
};
