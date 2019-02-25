/*
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
 * along with FISCO-BCOS.  If not, see <http://www.gnu.org/licenses/>
 * (c) 2016-2018 fisco-dev contributors.
 */
/**
 * @brief : Generate transaction DAG for parallel execution
 * @author: jimmyshi
 * @date: 2019-1-8
 */

#pragma once
#include "DAG.h"
#include "ExecutiveContext.h"
#include <libethcore/Block.h>
#include <libethcore/Transaction.h>
#include <memory>
#include <queue>
#include <vector>


namespace dev
{
namespace blockverifier
{
using ExecuteTxFunc = std::function<bool(dev::eth::Transaction const&, ID)>;

class TxDAGFace
{
public:
    virtual bool hasFinished() = 0;
    virtual ~TxDAGFace(){}

    // Called by thread
    // Execute a unit in DAG
    // This function can be parallel
    virtual int executeUnit() = 0;

    virtual void getHaventRun(){};
};

class TxDAG : public TxDAGFace
{
public:
    TxDAG() : m_dag() {}
    virtual ~TxDAG() {}

    // Generate DAG according with given transactions
    void init(ExecutiveContext::Ptr _ctx, dev::eth::Transactions const& _txs);

    // Set transaction execution function
    void setTxExecuteFunc(ExecuteTxFunc const& _f);

    // Called by thread
    // Has the DAG reach the end?
    bool hasFinished() override { return m_exeCnt >= m_totalParaTxs; }

    // Called by thread
    // Execute a unit in DAG
    // This function can be parallel
    int executeUnit() override;

    void executeSerialTxs();

    ID paraTxsNumber() { return m_totalParaTxs; }

    ID haveExecuteNumber() { return m_exeCnt; }

private:
    ExecuteTxFunc f_executeTx;
    std::shared_ptr<dev::eth::Transactions const> m_txs;

    IDs serialTxs;
    DAG m_dag;

    ID m_exeCnt = 0;
    ID m_totalParaTxs = 0;

    mutable std::mutex x_exeCnt;
};

template <typename T>
class CriticalField
{
public:
    ID get(T const& _c)
    {
        auto it = m_criticals.find(_c);
        if (it == m_criticals.end())
            return INVALID_ID;
        return it->second;
    }

    void update(T const& _c, ID _txId) { m_criticals[_c] = _txId; }

private:
    std::map<T, ID> m_criticals;
};

}  // namespace blockverifier
}  // namespace dev