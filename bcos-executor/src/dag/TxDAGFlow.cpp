/**
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
 * @file TxDAGFlow.cpp
 * @author: Jimmy Shi
 * @date 2023/1/31
 */
#include "TxDAGFlow.h"

using namespace bcos::executor;
using namespace tbb::flow;


int DagTxsTask::executeUnit()
{
    int exeCnt = 0;
    ID id = m_dag.waitPop();
    while (id != INVALID_ID)
    {
        do
        {
            exeCnt += 1;
            f_executeTx(id);
            id = m_dag.consume(id);
        } while (id != INVALID_ID);
        id = m_dag.waitPop();
    }
    if (exeCnt > 0)
    {
        Guard l(x_exeCnt);
        m_exeCnt += exeCnt;
    }
    return exeCnt;
}


void DagTxsTask::run()
{
    auto threadNum = std::thread::hardware_concurrency();
    // TODO: add timeout logic
    DAGFLOW_LOG(TRACE) << "Task run: DagTxsTask" << LOG_KV("size", m_totalParaTxs);
    auto parallelTimeOut = utcSteadyTime() + 30000;  // 30 timeout

    std::atomic<bool> isWarnedTimeout(false);
    tbb::parallel_for(tbb::blocked_range<unsigned int>(0, threadNum),
        [&](const tbb::blocked_range<unsigned int>& _r) {
            (void)_r;

            while (!hasFinished())
            {
                if (!isWarnedTimeout.load() && utcSteadyTime() >= parallelTimeOut)
                {
                    isWarnedTimeout.store(true);
                    EXECUTOR_LOG(WARNING)
                        << LOG_BADGE("executeBlock") << LOG_DESC("Para execute block timeout")
                        << LOG_KV("txNum", m_totalParaTxs);
                }
                executeUnit();
            }
        });
}
void DagTxsTask::makeEdge(critical::ID from, critical::ID to)
{
    makeVertex(from);
    makeVertex(to);
    m_dag.addEdge(from, to);
}
void DagTxsTask::makeVertex(critical::ID id)
{
    m_dag.resize(id + 1);
    m_totalParaTxs = std::max(m_totalParaTxs, id + 1);
}


void DagTxs2Task::run()
{
    // TODO: add timeout logic
    DAGFLOW_LOG(TRACE) << "Task run: DagTxs2Task" << LOG_KV("size", m_tasks.size());
    m_startTask.try_put(continue_msg());
    m_dag.wait_for_all();
}
void DagTxs2Task::makeEdge(critical::ID from, critical::ID to)
{
    auto& fromTask =
        m_tasks.try_emplace(from, Task(m_dag, [this, from](Msg) { f_executeTx(from); }))
            .first->second;
    auto& toTask =
        m_tasks.try_emplace(to, Task(m_dag, [this, to](Msg) { f_executeTx(to); })).first->second;
    make_edge(fromTask, toTask);
}
void DagTxs2Task::makeVertex(critical::ID id)
{
    auto& task =
        m_tasks.try_emplace(id, Task(m_dag, [this, id](Msg) { f_executeTx(id); })).first->second;
    make_edge(m_startTask, task);
}

void TxDAGFlow::init(critical::CriticalFieldsInterface::Ptr _txsCriticals)
{
    m_txsCriticals = _txsCriticals;
    auto txsSize = _txsCriticals->size();
    DAGFLOW_LOG(INFO) << LOG_DESC("Begin init TxDAGFlow") << LOG_KV("transactionNum", txsSize);

    FlowTask::Ptr currentTask = nullptr;
    auto executeTxHandler = [this](uint32_t id) { runExecuteTxFunc(id); };
    // define conflict handler
    auto onConflictHandler = [&](critical::ID pId, critical::ID id) {
        // only DAG Task
        if (!currentTask || currentTask->type() != FlowTask::Type::DAG)
        {
            currentTask = std::make_shared<DagTxsTask>(executeTxHandler);
            if (!m_tasks.empty())
            {
                m_tasks.back()->makeFinish();
            }
            m_tasks.push_back(currentTask);
        }

        auto dagTask = std::dynamic_pointer_cast<DagTxsTask>(currentTask);
        dagTask->makeEdge(pId, id);
    };

    auto onFirstConflictHandler = [&](critical::ID id) {
        if (isDagTx(id))
        {
            // only DAG Task
            if (!currentTask || currentTask->type() != FlowTask::Type::DAG)
            {
                currentTask = std::make_shared<DagTxsTask>(executeTxHandler);
                if (!m_tasks.empty())
                {
                    m_tasks.back()->makeFinish();
                }
                m_tasks.push_back(currentTask);
            }

            auto dagTask = std::dynamic_pointer_cast<DagTxsTask>(currentTask);
            dagTask->makeVertex(id);
        }
        else
        {
            // only Normal Task
            currentTask = std::make_shared<NormalTxTask>(executeTxHandler, id);
            if (!m_tasks.empty())
            {
                m_tasks.back()->makeFinish();
            }
            m_tasks.push_back(currentTask);
        }
    };
    auto onEmptyConflictHandler = [&](critical::ID id) {
        // only DAG Task
        if (!currentTask || currentTask->type() != FlowTask::Type::DAG)
        {
            currentTask = std::make_shared<DagTxsTask>(executeTxHandler);
            if (!m_tasks.empty())
            {
                m_tasks.back()->makeFinish();
            }
            m_tasks.push_back(currentTask);
        }

        auto dagTask = std::dynamic_pointer_cast<DagTxsTask>(currentTask);
        dagTask->makeVertex(id);
    };

    auto onAllConflictHandler = [&](critical::ID id) {
        // only Normal Task
        currentTask = std::make_shared<NormalTxTask>(executeTxHandler, id);
        if (!m_tasks.empty())
        {
            m_tasks.back()->makeFinish();
        }
        m_tasks.push_back(currentTask);
    };

    // parse criticals
    _txsCriticals->traverseDag(onConflictHandler, onFirstConflictHandler, onEmptyConflictHandler,
        onAllConflictHandler, true);

    if (!m_tasks.empty())
    {
        m_tasks.back()->makeFinish();
    }

    DAGFLOW_LOG(TRACE) << LOG_DESC("End init TxDAGFlow");
}


void TxDAGFlow::run(unsigned int threadNum)
{
    m_paused = false;

    while (currentTaskItr < m_tasks.size())
    {
        auto& task = m_tasks[currentTaskItr];
        task->run();
        currentTaskItr++;

        if (m_paused)
        {
            break;
        }
    }
}