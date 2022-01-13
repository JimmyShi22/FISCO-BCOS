//
// Created by jimmy on 2022/1/6.
//



#pragma once
#include <map>
#include <functional>


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

    virtual void parse(OnConflictHandler const& _onConflict,
        OnFirstConflictHandler const& _onFirstConflict,
        OnEmptyConflictHandler const& _onEmptyConflict,
        OnAllConflictHandler const& _onAllConflict) = 0;
};


template <typename T>
class CriticalFieldsRecorder
{
public:
    ID get(T const& _c)
    {
        auto it = m_criticals.find(_c);
        if (it == m_criticals.end())
        {
            if (m_criticalAll != INVALID_ID)
                return m_criticalAll;
            return INVALID_ID;
        }
        return it->second;
    }

    void update(T const& _c, ID _txId) { m_criticals[_c] = _txId; }

    void foreachField(std::function<void(ID)> _f)
    {
        for (auto const& _fieldAndId : m_criticals)
        {
            _f(_fieldAndId.second);
        }

        if (m_criticalAll != INVALID_ID)
            _f(m_criticalAll);
    }

    void setCriticalAll(ID _id)
    {
        m_criticalAll = _id;
        m_criticals.clear();
    }

private:
    std::map<T, ID> m_criticals;
    ID m_criticalAll = INVALID_ID;
};

template <typename T>
class CriticalFields : public virtual CriticalFieldsInterface
{
public:
    using Ptr = std::shared_ptr<CriticalFields>;
    using CriticalField = std::vector<T>;
    using CriticalFieldPtr = std::shared_ptr<CriticalField>;

    void push(CriticalFieldPtr _critical) { m_criticals.push_back(_critical); }
    size_t size() override { return m_criticals.size(); }

    void parse(OnConflictHandler const& _onConflict, OnFirstConflictHandler const& _onFirstConflict,
        OnEmptyConflictHandler const& _onEmptyConflict,
        OnAllConflictHandler const& _onAllConflict) override
    {
        CriticalFieldsRecorder<T> latestCriticals;

        for (ID id = 0; id < m_criticals.size(); ++id)
        {
            auto criticals = m_criticals[id];

            if (criticals == nullptr)
            {
                _onAllConflict(id);
            }
            else if (criticals.empty())
            {
                _onEmptyConflict(id);
            }
            else if (!criticals.empty())
            {

                // Get conflict parent's id set
                std::set<ID> pIds;
                for (T const& c : criticals)
                {
                    ID pId = latestCriticals.get(c);
                    if (pId != INVALID_ID)
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

                for (T const& c : criticals)
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
    std::vector<CriticalFieldPtr> m_criticals = std::vector<CriticalFieldPtr>();
};
}
}}
