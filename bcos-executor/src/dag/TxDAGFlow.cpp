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


void DagTxsTask::run()
{
    // TODO: add timeout logic
    DAGFLOW_LOG(TRACE) << "Task run: DagTxsTask" << LOG_KV("size", m_tasks.size());
    m_startTask.try_put(continue_msg());
    m_dag.wait_for_all();
};
void DagTxsTask::makeEdge(critical::ID from, critical::ID to)
{
    auto& fromTask =
        m_tasks.try_emplace(from, Task(m_dag, [this, from](Msg) { (*f_executeTx)(from); }))
            .first->second;
    auto& toTask =
        m_tasks.try_emplace(to, Task(m_dag, [this, to](Msg) { (*f_executeTx)(to); })).first->second;
    make_edge(fromTask, toTask);
};
void DagTxsTask::makeVertex(critical::ID id)
{
    auto& task =
        m_tasks.try_emplace(id, Task(m_dag, [this, id](Msg) { (*f_executeTx)(id); })).first->second;
    make_edge(m_startTask, task);
};

inline bool isDagTx(critical::CriticalFieldsInterface::Ptr _txsCriticals, critical::ID id)
{
    return _txsCriticals->contains(id);
}

void TxDAGFlow::init(critical::CriticalFieldsInterface::Ptr _txsCriticals, ExecuteTxFunc const& _f)
{
    auto txsSize = _txsCriticals->size();
    DAGFLOW_LOG(INFO) << LOG_DESC("Begin init TxDAGFlow") << LOG_KV("transactionNum", txsSize);
    auto f_executeTx = std::make_shared<ExecuteTxFunc>(_f);

    FlowTask::Ptr currentTask = nullptr;
    // define conflict handler
    auto onConflictHandler = [&](critical::ID pId, critical::ID id) {
        // only DAG Task
        if (!currentTask || currentTask->type() != FlowTask::Type::DAG)
        {
            currentTask = std::make_shared<DagTxsTask>(f_executeTx);
            m_tasks.push_back(currentTask);
        }

        auto dagTask = std::dynamic_pointer_cast<DagTxsTask>(currentTask);
        dagTask->makeEdge(pId, id);
    };

    auto onFirstConflictHandler = [&](critical::ID id) {
        if (isDagTx(_txsCriticals, id))
        {
            // only DAG Task
            if (!currentTask || currentTask->type() != FlowTask::Type::DAG)
            {
                currentTask = std::make_shared<DagTxsTask>(f_executeTx);
                m_tasks.push_back(currentTask);
            }

            auto dagTask = std::dynamic_pointer_cast<DagTxsTask>(currentTask);
            dagTask->makeVertex(id);
        }
        else
        {
            // only Normal Task
            currentTask = std::make_shared<NormalTxTask>(f_executeTx, id);
            m_tasks.push_back(currentTask);
        }
    };
    auto onEmptyConflictHandler = [&](critical::ID id) {
        // only DAG Task
        if (!currentTask || currentTask->type() != FlowTask::Type::DAG)
        {
            currentTask = std::make_shared<DagTxsTask>(f_executeTx);
            m_tasks.push_back(currentTask);
        }

        auto dagTask = std::dynamic_pointer_cast<DagTxsTask>(currentTask);
        dagTask->makeVertex(id);
    };

    auto onAllConflictHandler = [&](critical::ID id) {
        // only Normal Task
        currentTask = std::make_shared<NormalTxTask>(f_executeTx, id);
        m_tasks.push_back(currentTask);
    };

    // parse criticals
    _txsCriticals->traverseDag(onConflictHandler, onFirstConflictHandler, onEmptyConflictHandler,
        onAllConflictHandler, true);

    DAGFLOW_LOG(TRACE) << LOG_DESC("End init TxDAGFlow");
}


void TxDAGFlow::run(unsigned int threadNum)
{
    for (size_t i = 0; i < m_tasks.size(); i++)
    {
        auto& task = m_tasks[i];
        task->run();
    }
};