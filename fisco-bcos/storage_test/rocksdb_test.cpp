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
#include "libinitializer/Initializer.h"
#include "libstorage/MemoryTableFactory.h"
#include <leveldb/db.h>
#include <libdevcore/Common.h>
#include <libdevcore/easylog.h>
#include <libstorage/BasicRocksDB.h>
#include <libstorage/LevelDBStorage.h>
#include <rocksdb/table.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/program_options.hpp>
INITIALIZE_EASYLOGGINGPP

using namespace std;
using namespace dev;
using namespace boost;
using namespace dev::storage;
using namespace dev::db;
using namespace rocksdb;


int main(int argc, const char* argv[])
{
    (void)argc;
    auto storagePath = "./test_rocksdb";
    cout << "DB path : " << storagePath << endl;
    filesystem::create_directories(storagePath);

    rocksdb::Options options;
    ///*
    rocksdb::BlockBasedTableOptions table_options;
    // table_options.cache_index_and_filter_blocks = true;
    table_options.block_cache = rocksdb::NewLRUCache(1 * 256 * 1024 * 1024);

    options.table_factory.reset(NewBlockBasedTableFactory(table_options));
    //*/
    // set Parallelism to the hardware concurrency
    // options.IncreaseParallelism(std::max(1, (int)std::thread::hardware_concurrency()));

    // options.OptimizeLevelStyleCompaction();
    options.create_if_missing = true;
    options.max_open_files = 200;
    options.compression = rocksdb::kSnappyCompression;
    auto storageDB = std::make_shared<BasicRocksDB>();

    // any exception will cause initBasicRocksDB failed, and the program will be stopped
    storageDB->Open(options, storagePath);

    int loop = atoi(argv[1]);
    tbb::spin_mutex writeBatchMutex;

    string start = to_string(utcTime());
    for (int i = 0; i < loop; i++)
    {
        WriteBatch batch;
        for (int j = 0; j < 50000; j++)
        {
            string key = start + to_string(i * 50000 + j);
            string value =
                to_string(utcTime()) + start +
                "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                "fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffa"
                "aaaaaaa";
            storageDB->PutWithLock(batch, key, value, writeBatchMutex);
        }

        WriteOptions options;
        options.sync = false;
        storageDB->Write(options, batch);

        cout << "Write: " << i << endl;

        for (int j = 0; j < 50000; j++)
        {
            string key = start + to_string(i * 50000 + j);
            string value;
            storageDB->GetSimple(ReadOptions(), rocksdb::Slice(key), &value);
        }
        cout << "Get: " << i << endl;
    }
    getchar();
    return 0;
}
