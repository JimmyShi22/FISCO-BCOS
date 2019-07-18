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

#include "SetIDStorage.h"
#include "Storage.h"
#include <libdevcore/Common.h>

using namespace std;
using namespace dev;
using namespace dev::storage;


size_t SetIDStorage::commit(h256 hash, int64_t num, const std::vector<TableData::Ptr>& datas)
{
    for (auto data : datas)
    {
        auto size = data->newEntries->size();
        for (size_t i = 0; i < size; i++)
        {
            auto commitEntry = data->newEntries->get(i);
            commitEntry->setID(++m_ID);
            commitEntry->setNum(num);
        }
    }

    return m_backend->commit(hash, num, datas);
}

/*
void SetIDStorage::init()
{
    // get id from backend
    auto out = m_backend->select(h256(), 0, tableInfo, SYS_KEY_CURRENT_ID, condition);
    if (out->size() > 0)
    {
        auto entry = out->get(0);
        auto numStr = entry->getField(SYS_VALUE);
        m_ID = boost::lexical_cast<size_t>(numStr);
    }
}
*/
