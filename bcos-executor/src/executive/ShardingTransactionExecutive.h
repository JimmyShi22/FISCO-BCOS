/*
 *  Copyright (C) 2021 FISCO BCOS.
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
 * @brief Execute transaction in a shard
 * @file ShardingTransactionExecutive.h
 * @author: JimmyShi22
 * @date: 2023-01-07
 */

#pragma once
#include "PromiseTransactionExecutive.h"
namespace bcos::executor
{
class ShardingTransactionExecutive : public CoroutineTransactionExecutive
{
public:
    ShardingTransactionExecutive(std::weak_ptr<BlockContext> blockContext,
        std::string contractAddress, int64_t contextID, int64_t seq,
        std::shared_ptr<wasm::GasInjector>& gasInjector);

    ~ShardingTransactionExecutive() override = default;

    CallParameters::UniquePtr start(CallParameters::UniquePtr input) override;

    CallParameters::UniquePtr externalCall(CallParameters::UniquePtr input) override;

    std::string getContractShard(const std::string_view& contractAddress);

private:
    std::optional<std::string> m_shardName;
};
}  // namespace bcos::executor
