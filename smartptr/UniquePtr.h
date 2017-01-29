#pragma once
#include <stddef.h>

namespace neu {

    template <class Object>
    void GlobalDeleteObject(Object* pObject)
    {
        delete pObject;
    }

    template <class Object, void(*DeleteObject)(Object*) = &GlobalDeleteObject>
    class UniquePtr
    {
    public:
        typedef UniquePtr<Object> This;

    private:
        Object* m_pObject;

    private:
        void Release()
        {
            if (m_pObject)
            {
                DeleteObject(m_pObject);
                m_pObject = nullptr;
            }
        }

        void Assign(This&& rhs)
        {
            m_pObject = rhs.m_pObject;
            rhs.m_pObject = nullptr;
        }

    private: // deleted members
        UniquePtr(const This& rhs);
        UniquePtr& operator=(const This& rhs);

    public:
        UniquePtr() : m_pObject() {}
        UniquePtr(nullptr_t) : m_pObject() {}
        UniquePtr(This&& rhs)
        {
            Assign(rhs);
        }
        UniquePtr& operator=(This&& rhs)
        {
            Assign(rhs);
            return *this;
        }

        explicit UniquePtr(Object* pObject)
            : m_pObject(pObject)
        {}

        Object& operator*() const
        {
            return *m_pObject;
        }
        Object* operator->() const
        {
            return m_pObject;
        }
        Object* const* operator&() const
        {
            return &m_pObject;
        }
        Object** operator&()
        {
            return &m_pObject;
        }

        operator Object*() const
        {
            return m_pObject;
        }
    };

}
