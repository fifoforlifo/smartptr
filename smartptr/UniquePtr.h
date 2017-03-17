#pragma once
#include <stddef.h>
#include <assert.h>
#include <utility>
#include "Noexcept.h"

namespace ci0 {

    template <class Object>
    void DeleteObjectWithGlobalDelete(Object* pObject)
    {
        delete pObject;
    }

    template <class Object, void(*DeleteObject)(Object*) = &DeleteObjectWithGlobalDelete<Object> >
    class UniquePtr
    {
    public:
        typedef UniquePtr<Object> This;

        template <class RhsObject, void(*RhsDeleteObject)(RhsObject*)>
        friend class UniquePtr;

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

        template <class RhsObject, void(*RhsDeleteObject)(RhsObject*)>
        void Assign(UniquePtr<RhsObject, RhsDeleteObject>&& rhs)
        {
            m_pObject = rhs.m_pObject;
            rhs.m_pObject = nullptr;
        }

    private:
        // deleted members
        UniquePtr(const This& rhs);
        UniquePtr& operator=(const This& rhs);

        // prevent naked delete from compiling; http://stackoverflow.com/a/3312507
        struct PreventDelete;
        operator PreventDelete*() const;

    public:
        friend class OutParam;
        class OutParam
        {
            This* m_pSelf;
            Object* m_pObject;

        private:
            OutParam(const OutParam& rhs); // = delete
            OutParam& operator=(const OutParam& rhs); // = delete
            OutParam& operator=(OutParam&& rhs); // = delete

        public:
            ~OutParam() CI0_NOEXCEPT(true)
            {
                if (m_pSelf)
                {
                    This& self = *m_pSelf;
                    if (self.m_pObject != m_pObject)
                    {
                        self.Release();
                        self.m_pObject = m_pObject;
                    }
                }
                else
                {
                    assert(!m_pObject);
                }
            }
            explicit OutParam(This& self) CI0_NOEXCEPT(true)
                : m_pSelf(&self)
                , m_pObject(self.m_pObject)
            {
            }
            OutParam(OutParam&& rhs) CI0_NOEXCEPT(true)
                : m_pSelf(rhs.m_pSelf)
                , m_pObject(rhs.m_pObject)
            {
                rhs.m_pSelf = nullptr;
                rhs.m_pObject = nullptr;
            }

            operator Object* const*() const CI0_NOEXCEPT(true)
            {
                assert(m_pSelf);
                return &m_pObject;
            }
            operator Object**() CI0_NOEXCEPT(true)
            {
                assert(m_pSelf);
                return &m_pObject;
            }
        };

    public:
        ~UniquePtr() CI0_NOEXCEPT(true)
        {
            Release();
        }
        UniquePtr() CI0_NOEXCEPT(true)
            : m_pObject()
        {
        }
        UniquePtr(nullptr_t) CI0_NOEXCEPT(true)
            : m_pObject()
        {
        }
        UniquePtr(This&& rhs) CI0_NOEXCEPT(true)
        {
            Assign(std::move(rhs));
        }
        template <class RhsObject, void(*RhsDeleteObject)(RhsObject*)>
        UniquePtr(UniquePtr<RhsObject, RhsDeleteObject>&& rhs) CI0_NOEXCEPT(true)
        {
            Assign(std::move(rhs));
        }
        UniquePtr& operator=(This&& rhs) CI0_NOEXCEPT(true)
        {
            if (this != &rhs)
            {
                Release();
                Assign(std::move(rhs));
            }
            return *this;
        }
        template <class RhsObject, void(*RhsDeleteObject)(RhsObject*)>
        UniquePtr& operator=(UniquePtr<RhsObject, RhsDeleteObject>&& rhs) CI0_NOEXCEPT(true)
        {
            Release();
            Assign(std::move(rhs));
            return *this;
        }

        explicit UniquePtr(Object* pObject) CI0_NOEXCEPT(true)
            : m_pObject(pObject)
        {
        }

        Object& operator*() const CI0_NOEXCEPT(true)
        {
            return *m_pObject;
        }
        Object* operator->() const CI0_NOEXCEPT(true)
        {
            return m_pObject;
        }

        operator Object*() const CI0_NOEXCEPT(true)
        {
            return m_pObject;
        }
        explicit operator bool() const CI0_NOEXCEPT(true)
        {
            return !!m_pObject;
        }
        template <class Type>
        explicit operator Type*() const CI0_NOEXCEPT(true)
        {
            return static_cast<Type*>(m_pObject);
        }

        Object* const& get() const CI0_NOEXCEPT(true)
        {
            return m_pObject;
        }
        OutParam out() CI0_NOEXCEPT(true)
        {
            return OutParam(*this);
        }
        This& attach(Object* pObject) CI0_NOEXCEPT(true)
        {
            assert(m_pObject != pObject);
            Release();
            m_pObject = pObject;
            return *this;
        }
        Object* detach() CI0_NOEXCEPT(true)
        {
            Object* pObject = m_pObject;
            m_pObject = nullptr;
            return pObject;
        }
        // note: present for STL/boost compatibility, but you should prefer to call detach() instead
        Object* release() CI0_NOEXCEPT(true)
        {
            return detach();
        }
        This& swap(This& rhs) CI0_NOEXCEPT(true)
        {
            Object* pObject = m_pObject;
            m_pObject = rhs.m_pObject;
            rhs.m_pObject = pObject;
            return *this;
        }
        This& swap(This&& rhs) CI0_NOEXCEPT(true)
        {
            Object* pObject = m_pObject;
            m_pObject = rhs.m_pObject;
            rhs.m_pObject = pObject;
            return *this;
        }
        This& reset() CI0_NOEXCEPT(true)
        {
            Release();
            return *this;
        }
        // note: present for STL/boost compatibility, but you should prefer to call attach() instead
        This& reset(Object* pObject) CI0_NOEXCEPT(true)
        {
            return attach(pObject);
        }
        template <class Other>
        UniquePtr<Other> move_as() CI0_NOEXCEPT(true)
        {
            UniquePtr<Other> pOther(static_cast<Other*>(m_pObject));
            m_pObject = nullptr;
            return pOther;
        }
    };

    template <class LhsObject, void(*LhsDeleteObject)(LhsObject*), class RhsObject, void(*RhsDeleteObject)(RhsObject*)>
    inline bool operator==(const UniquePtr<LhsObject, LhsDeleteObject>& lhs, const UniquePtr<RhsObject, RhsDeleteObject>& rhs)
    {
        return lhs.get() == rhs.get();
    }
    template <class LhsObject, void(*LhsDeleteObject)(LhsObject*), class RhsObject, void(*RhsDeleteObject)(RhsObject*)>
    inline bool operator!=(const UniquePtr<LhsObject, LhsDeleteObject>& lhs, const UniquePtr<RhsObject, RhsDeleteObject>& rhs)
    {
        return lhs.get() != rhs.get();
    }
    template <class LhsObject, void(*LhsDeleteObject)(LhsObject*), class RhsObject, void(*RhsDeleteObject)(RhsObject*)>
    inline bool operator>=(const UniquePtr<LhsObject, LhsDeleteObject>& lhs, const UniquePtr<RhsObject, RhsDeleteObject>& rhs)
    {
        return lhs.get() >= rhs.get();
    }
    template <class LhsObject, void(*LhsDeleteObject)(LhsObject*), class RhsObject, void(*RhsDeleteObject)(RhsObject*)>
    inline bool operator<=(const UniquePtr<LhsObject, LhsDeleteObject>& lhs, const UniquePtr<RhsObject, RhsDeleteObject>& rhs)
    {
        return lhs.get() <= rhs.get();
    }
    template <class LhsObject, void(*LhsDeleteObject)(LhsObject*), class RhsObject, void(*RhsDeleteObject)(RhsObject*)>
    inline bool operator>(const UniquePtr<LhsObject, LhsDeleteObject>& lhs, const UniquePtr<RhsObject, RhsDeleteObject>& rhs)
    {
        return lhs.get() > rhs.get();
    }
    template <class LhsObject, void(*LhsDeleteObject)(LhsObject*), class RhsObject, void(*RhsDeleteObject)(RhsObject*)>
    inline bool operator<(const UniquePtr<LhsObject, LhsDeleteObject>& lhs, const UniquePtr<RhsObject, RhsDeleteObject>& rhs)
    {
        return lhs.get() < rhs.get();
    }

    template <class Object, void(*DeleteObject)(Object*)>
    inline bool operator==(const UniquePtr<Object, DeleteObject>& lhs, std::nullptr_t)
    {
        return lhs.get() == nullptr;
    }
    template <class Object, void(*DeleteObject)(Object*)>
    inline bool operator==(std::nullptr_t, const UniquePtr<Object, DeleteObject>& rhs)
    {
        return nullptr == rhs.get();
    }
    template <class Object, void(*DeleteObject)(Object*)>
    inline bool operator!=(const UniquePtr<Object, DeleteObject>& lhs, std::nullptr_t)
    {
        return lhs.get() != nullptr;
    }
    template <class Object, void(*DeleteObject)(Object*)>
    inline bool operator!=(std::nullptr_t, const UniquePtr<Object, DeleteObject>& rhs)
    {
        return nullptr != rhs.get();
    }

    template <class Object, void(*DeleteObject)(Object*)>
    void swap(UniquePtr<Object, DeleteObject>& lhs, UniquePtr<Object, DeleteObject>& rhs)
    {
        lhs.swap(rhs);
    }
}
