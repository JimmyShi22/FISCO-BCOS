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
/** @file MemoryTable.cpp
 *  @author ancelmo
 *  @date 20180921
 */
#include "MemoryTable.h"
#include "Common.h"
#include "Table.h"
#include <json/json.h>
#include <libdevcore/easylog.h>
#include <libdevcrypto/Hash.h>
#include <libprecompiled/Common.h>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/lexical_cast.hpp>

using namespace dev;
using namespace dev::storage;
using namespace dev::precompiled;
