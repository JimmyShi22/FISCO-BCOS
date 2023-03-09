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
 * @file StorageWrapper.cpp
 * @author: Jimmy Shi
 * @date 2023/3/8
 */

#include "StorageWrapper.h"


bcos::RateCollector bcos::storage::StorageWrapper::m_openTableRateCollector =
    bcos::RateCollector("open_table_rate_collector", 1000, true);
bcos::RateCollector bcos::storage::StorageWrapper::m_createTableRateCollector =
    bcos::RateCollector("create_table_rate_collector", 1000, true);
bcos::RateCollector bcos::storage::StorageWrapper::m_getTableRateCollector =
    bcos::RateCollector("get_row_table_rate_collector", 1000, true);
bcos::RateCollector bcos::storage::StorageWrapper::m_setTableRateCollector =
    bcos::RateCollector("set_row_table_rate_collector", 1000, true);
