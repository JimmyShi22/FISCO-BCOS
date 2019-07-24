/**
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
 *
 * @file storage_main.cpp
 * @author: jimmyshi
 * @date 2018-11-14
 */
#include <stdlib.h>
#include <tbb/concurrent_queue.h>
#include <tbb/concurrent_unordered_map.h>
#include <tbb/mutex.h>
#include <tbb/recursive_mutex.h>
#include <tbb/spin_mutex.h>
#include <tbb/spin_rw_mutex.h>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>
#include <iostream>
#include <memory>
#include <string>

using namespace std;
using namespace boost;

class MRU
{
public:
    using Ptr = std::shared_ptr<MRU>;
    void update(const std::string& table, const std::string& key, ssize_t capacity)
    {
        (void)capacity;
        auto r = m_mru.push_back(std::make_pair(table, key));
        if (!r.second)
        {
            m_mru.relocate(m_mru.end(), r.first);
        }
    }

    size_t size() { return m_mru.size(); }

private:
    boost::multi_index_container<std::pair<std::string, std::string>,
        boost::multi_index::indexed_by<boost::multi_index::sequenced<>,
            boost::multi_index::hashed_unique<
                boost::multi_index::identity<std::pair<std::string, std::string>>>>>
        m_mru;
};

class MapCache
{
public:
    using Ptr = std::shared_ptr<MapCache>;

    void update(const std::string& table, const std::string& key, ssize_t capacity)
    {
        (void)capacity;
        auto cacheKey = table + "_" + key;
        m_caches.insert(std::make_pair(cacheKey, make_shared<int>(0)));
    }

private:
    tbb::concurrent_unordered_map<std::string, std::shared_ptr<int>> m_caches;
};

void randCallMRU(MRU::Ptr _mru, MapCache::Ptr _cache, size_t _limit)
{
    ssize_t seed = rand() % (_limit * 5);
    ssize_t capacity = seed;
    string seedStr = to_string(seed);
    string table = "_contract_data_2426edaa1173f65cd7d62c93c935bfde" + seedStr + "_";
    string key = "625140098866070291072905618058385853340797980745687129245832307977346" + seedStr;
    _mru->update(table, key, capacity);
    _cache->update(table, key, capacity);
}

int main(int argc, const char* argv[])
{
    (void)argc;

    MRU::Ptr mru = make_shared<MRU>();
    MapCache::Ptr cache = make_shared<MapCache>();
    size_t limit = atoi(argv[1]);

    size_t i = 0;
    while (mru->size() < limit)
    {
        randCallMRU(mru, cache, limit);
        i++;
        if (i % 10000 == 0)
        {
            cout << "loop " << i << " times, mru size " << mru->size() << endl;
        }
    }
    cout << "loop " << i << " times, mru size " << mru->size() << endl;
    getchar();
    return 0;
}
