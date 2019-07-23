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

#include <libdevcore/Common.h>
#include <libdevcore/easylog.h>
#include <rocksdb/db.h>
#include <rocksdb/table.h>
#include <rocksdb/utilities/leveldb_options.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <memory>
INITIALIZE_EASYLOGGINGPP

using namespace std;
using namespace dev;
using namespace boost;
using namespace rocksdb;

int main(int argc, char** argv)
{
    (void)argc;

    DB* db;
    auto storagePath = "./test_rocksleveldb";
    cout << "DB path : " << storagePath << endl;
    boost::filesystem::create_directories(storagePath);

    LevelDBOptions opt;
    opt.create_if_missing = true;
    opt.max_open_files = 1000;
    opt.block_size = 4096;

    Options rocksdb_options = ConvertOptions(opt);
    // add rocksdb specific options here

    Status s = DB::Open(rocksdb_options, storagePath, &db);
    std::shared_ptr<DB> storageDB;
    storageDB.reset(db);


    int loop = atoi(argv[1]);

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
            batch.Put(Slice(std::move(key)), Slice(std::move(value)));
        }

        WriteOptions options;
        options.sync = false;
        storageDB->Write(options, &batch);

        cout << "Write: " << i << endl;

        for (int j = 0; j < 50000; j++)
        {
            string key = start + to_string(i * 50000 + j);
            string value;
            storageDB->Get(ReadOptions(), rocksdb::Slice(key), &value);
        }
        cout << "Get: " << i << endl;
    }
    getchar();
    return 0;
}
