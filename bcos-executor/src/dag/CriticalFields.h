//
// Created by jimmy on 2022/1/6.
//



#pragma once
#include <map>
#include <functional>
#include "TrieSet.h"

namespace bcos
{
namespace executor
{
namespace critical
{
using ID = uint32_t;
static const ID INVALID_ID = (ID(0) - 1);

using OnConflictHandler = std::function<void(ID, ID)>;  // conflict from -> to
using OnFirstConflictHandler = std::function<void(ID)>;       // conflict
using OnEmptyConflictHandler = std::function<void(ID)>;       // conflict
using OnAllConflictHandler = std::function<void(ID)>;         // conflict

class CriticalFieldsInterface
{
public:
    using Ptr = std::shared_ptr<CriticalFieldsInterface>;

    virtual size_t size() = 0;

    virtual bool contains(size_t id) = 0;

    virtual void parse(OnConflictHandler const& _onConflict,
        OnFirstConflictHandler const& _onFirstConflict,
        OnEmptyConflictHandler const& _onEmptyConflict,
        OnAllConflictHandler const& _onAllConflict) = 0;
};

template<typename T>
class LatestCriticalFields {
public:
    std::vector<ID> get(std::vector<T> const& _c) {
        return m_trie.get(_c);
    }

    void update(std::vector<T> const& _c, ID _txId) {
        m_trie.set(_c, _txId);
    }

private:
    TrieSet<T, ID> m_trie;
};

template <typename T>
class CriticalFields : public virtual CriticalFieldsInterface
{
public:
    using Ptr = std::shared_ptr<CriticalFields>;
    using CriticalField = std::vector<std::vector<T>>;
    using CriticalFieldPtr = std::shared_ptr<CriticalField>;

    CriticalFields(size_t _size): m_criticals(std::vector<CriticalFieldPtr>(_size)) {}

    size_t size() override { return m_criticals.size(); }
    bool contains(size_t id) override { return id < size() && get(id) != nullptr; };
    void put(size_t _id, CriticalFieldPtr _criticalField) { m_criticals[_id] = _criticalField; }
    CriticalFieldPtr get(size_t _id) { return m_criticals[_id];}

    void parse(OnConflictHandler const& _onConflict, OnFirstConflictHandler const& _onFirstConflict,
        OnEmptyConflictHandler const& _onEmptyConflict,
        OnAllConflictHandler const& _onAllConflict) override
    {
        LatestCriticalFields<T> latestCriticals;

        for (ID id = 0; id < m_criticals.size(); ++id)
        {
            auto criticals = m_criticals[id];

            if (criticals == nullptr)
            {
                _onAllConflict(id);
            }
            else if (criticals->empty())
            {
                _onEmptyConflict(id);
            }
            else if (!criticals->empty())
            {

                // Get conflict parent's id set
                std::set<ID> pIds;
                for (auto const& c : *criticals)
                {
                    for(auto pId : latestCriticals.get(c))
                    {
                        pIds.insert(pId);
                    }
                }

                if (pIds.empty())
                {
                    _onFirstConflict(id);
                } else {
                    for (ID pId : pIds)
                    {
                        _onConflict(pId, id);
                    }
                }

                for (auto const& c : *criticals)
                {
                    latestCriticals.update(c, id);
                }
            }
            else
            {
                continue;
            }
        }
    };


private:
    std::vector<CriticalFieldPtr> m_criticals;
};
}
}}
