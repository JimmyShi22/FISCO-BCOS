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
 * @brief : SetIDStorage
 * @author: jimmyshi
 * @date: 2019-05-22
 */

#pragma once
#include "Storage.h"
#include <libdevcore/Common.h>

namespace dev
{
namespace storage
{
class SetIDStorage : public Storage
{
public:
    typedef std::shared_ptr<SetIDStorage> Ptr;
    virtual ~SetIDStorage(){};
    // void init();
    Entries::Ptr select(h256 hash, int64_t num, TableInfo::Ptr tableInfo, const std::string& key,
        Condition::Ptr condition = nullptr) override
    {
        return m_backend->select(hash, num, tableInfo, key, condition);
    };
    size_t commit(h256 hash, int64_t num, const std::vector<TableData::Ptr>& datas) override;

    bool onlyDirty() override { return m_backend->onlyDirty(); };

    void setGroupID(dev::GROUP_ID const& groupID) { m_backend->setGroupID(groupID); }
    dev::GROUP_ID groupID() const { return m_backend->groupID(); }
    void setBackend(Storage::Ptr backend) { m_backend = backend; }

private:
    Storage::Ptr m_backend;
    uint64_t m_ID = 1;
};
}  // namespace storage
}  // namespace dev