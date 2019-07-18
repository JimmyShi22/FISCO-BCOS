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
#include <libdevcore/BasicLevelDB.h>
#include <libdevcore/Common.h>
#include <libdevcore/easylog.h>
#include <libstorage/LevelDBStorage.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/program_options.hpp>
INITIALIZE_EASYLOGGINGPP

using namespace std;
using namespace dev;
using namespace boost;
using namespace dev::storage;

int main(int argc, const char* argv[])
{
    (void)argc;
    auto storagePath = "./test_leveldb";
    cout << "DB path : " << storagePath << endl;
    filesystem::create_directories(storagePath);
    leveldb::Options option;
    option.create_if_missing = true;
    option.max_open_files = 1000;
    dev::db::BasicLevelDB* dbPtr = NULL;
    leveldb::Status s = dev::db::BasicLevelDB::Open(option, storagePath, &dbPtr);
    if (!s.ok())
    {
        cerr << "Open storage leveldb error: " << s.ToString() << endl;
        return -1;
    }

    auto storageDB = std::shared_ptr<dev::db::BasicLevelDB>(dbPtr);
    int loop = atoi(argv[1]);

    string start = to_string(utcTime());
    for (int i = 0; i < loop; i++)
    {
        auto batch = storageDB->createWriteBatch();
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
            batch->insertSlice(leveldb::Slice(key), leveldb::Slice(value));
        }

        leveldb::WriteOptions writeOptions;
        writeOptions.sync = false;
        auto s = storageDB->Write(writeOptions, &(batch->writeBatch()));
        if (!s.ok())
        {
            cerr << "Open storage leveldb error: " << s.ToString() << endl;
            return -1;
        }
        cout << "Write: " << i << endl;

        for (int j = 0; j < 50000; j++)
        {
            string key = start + to_string(i * 50000 + j);
            string value;
            storageDB->Get(leveldb::ReadOptions(), leveldb::Slice(key), &value);
        }
        cout << "Get: " << i << endl;
    }
    getchar();
    return 0;
}
