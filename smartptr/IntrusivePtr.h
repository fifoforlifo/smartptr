#pragma once
#include <stddef.h>
#include <assert.h>
#include <algorithm>
#include <utility>
#include "Noexcept.h"

namespace ci0 {

    // IntrusivePtr is designed to be compatible with boost::intrusive_ptr:
    //  http://www.boost.org/doc/libs/master/libs/smart_ptr/intrusive_ptr.html
    //
    // IntrusivePtr assignment rules are similar to CComPtr
    //  *   Assignment from a raw pointer acts like a copy; add_ref() is called.
    //  *   The constructor performs add_ref() by default, for parity with assignment.
    //  *   Taking ownership requires calling Attach(), passing addRef=false.
    //  *   Taking ownership at construction time requires an explicit constructor
    //      call, passing addRef=false.
    //
    // Most new objects have an initial refcount of 1, to avoid the perf overhead
    // of an initial add_ref() call.  Such objects should never be created like this:
    //      IntrusivePtr<Foo> pFoo = new Foo{};
    //      pFoo = new Foo{};
    // In both cases, a memory leak will occur since the constructor/operator= will
    // perform an additional add_ref(), resulting in a single reference with refcount=2.
    //
    // Note that boost::intrusive_ref_counter has an initial refcount=0, which makes
    // the following code safe (but at the expense of some micro-perf):
    //      IntrusivePtr<Foo> pFoo = new intrusive_ref_counter{};
    //      pFoo = new intrusive_ref_counter{};
    //  http://www.boost.org/doc/libs/master/libs/smart_ptr/intrusive_ref_counter.html


    template <class Object>
    class IntrusivePtr
    {
    public:
        typedef IntrusivePtr<Object> This;

    private:
        Object* m_pObject;

    private:
        void Release()
        {
            if (m_pObject)
            {
                // clear member before calling 'release' to defend against re-entrancy [no perf cost, so why not]
                Object* pObject = m_pObject;
                m_pObject = nullptr;
                intrusive_ptr_release(pObject);
            }
        }

        static void SafeAddRef(Object* pObject)
        {
            if (pObject)
            {
                intrusive_ptr_add_ref(pObject);
            }
        }
        void CopyAssign(const This& rhs)
        {
            m_pObject = rhs.m_pObject;
            if (m_pObject)
            {
                SafeAddRef(m_pObject);
            }
        }
        void MoveAssign(This&& rhs)
        {
            m_pObject = rhs.m_pObject;
            rhs.m_pObject = nullptr;
        }

    private:
        // prevent naked delete from compiling; http://stackoverflow.com/a/3312507
        struct PreventDelete;
        operator PreventDelete*() const;

    public:
        friend class OutParam;
        class OutParam
        {
            This* m_pSelf;
            Object* m_pObject;
            bool m_assumeInitialAddRef;

        private:
            OutParam(const OutParam& rhs); // = delete
            OutParam& operator=(const OutParam& rhs); // = delete
            OutParam& operator=(OutParam&& rhs); // = delete

        public:
            ~OutParam() CI0_NOEXCEPT(true)
            {
                if (m_pSelf)
                {
                    if (!m_assumeInitialAddRef)
                    {
                        SafeAddRef(m_pObject);
                    }
                    m_pSelf->m_pObject = m_pObject;
                }
                else
                {
                    assert(!m_pObject);
                }
            }
            explicit OutParam(This& self, bool assumeInitialAddRef) CI0_NOEXCEPT(true)
                : m_pSelf(&self)
                , m_pObject()
                , m_assumeInitialAddRef(assumeInitialAddRef)
            {
            }
            OutParam(OutParam&& rhs) CI0_NOEXCEPT(true)
                : m_pSelf(rhs.m_pSelf)
                , m_pObject(rhs.m_pObject)
                , m_assumeInitialAddRef(rhs.m_assumeInitialAddRef)
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
        ~IntrusivePtr() CI0_NOEXCEPT(true)
        {
            Release();
        }
        IntrusivePtr() CI0_NOEXCEPT(true)
            : m_pObject()
        {
        }
        IntrusivePtr(nullptr_t) CI0_NOEXCEPT(true)
            : m_pObject()
        {
        }
        IntrusivePtr(const This& rhs) CI0_NOEXCEPT(true)
        {
            CopyAssign(rhs);
        }
        IntrusivePtr(This&& rhs) CI0_NOEXCEPT(true)
        {
            MoveAssign(std::move(rhs));
        }
        This& operator=(const This& rhs)
        {
            This(rhs).swap(*this);
            return *this;
        }
        This& operator=(This&& rhs) CI0_NOEXCEPT(true)
        {
            if (m_pObject != rhs.m_pObject)
            {
                Release();
                MoveAssign(std::move(rhs));
            }
            return *this;
        }

        IntrusivePtr(Object* pObject, bool addRef = true)
            : m_pObject(pObject)
        {
            if (addRef)
            {
                SafeAddRef(pObject);
            }
        }
        This& operator=(Object* pObject)
        {
            if (m_pObject != pObject)
            {
                SafeAddRef(pObject);
                Release();
                m_pObject = pObject;
            }
            return *this;
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
        OutParam out(bool assumeInitialAddRef = true) CI0_NOEXCEPT(true)
        {
            Release();
            return OutParam(*this, assumeInitialAddRef);
        }
        This& attach(Object* pObject, bool addRef = true)
        {
            if (addRef)
            {
                SafeAddRef(pObject);
                Release();
                m_pObject = pObject;
                return *this;
            }

            if (m_pObject != pObject)
            {
                Release();
                m_pObject = pObject;
            }
            return *this;
        }
        Object* detach() CI0_NOEXCEPT(true)
        {
            Object* pObject = m_pObject;
            m_pObject = nullptr;
            return pObject;
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
        This& reset(Object* pObject, bool addRef = true)
        {
            return attach(pObject, addRef);
        }
        template <class Other>
        IntrusivePtr<Other> move_as() CI0_NOEXCEPT(true)
        {
            UniquePtr<Other> pOther(static_cast<Other*>(m_pObject), false);
            m_pObject = nullptr;
            return pOther;
        }
    };

    template <class LhsObject, class RhsObject>
    inline bool operator==(const IntrusivePtr<LhsObject>& lhs, const IntrusivePtr<RhsObject>& rhs)
    {
        return lhs.get() == rhs.get();
    }
    template <class LhsObject, class RhsObject>
    inline bool operator!=(const IntrusivePtr<LhsObject>& lhs, const IntrusivePtr<RhsObject>& rhs)
    {
        return lhs.get() != rhs.get();
    }
    template <class LhsObject, class RhsObject>
    inline bool operator>=(const IntrusivePtr<LhsObject>& lhs, const IntrusivePtr<RhsObject>& rhs)
    {
        return lhs.get() >= rhs.get();
    }
    template <class LhsObject, class RhsObject>
    inline bool operator<=(const IntrusivePtr<LhsObject>& lhs, const IntrusivePtr<RhsObject>& rhs)
    {
        return lhs.get() <= rhs.get();
    }
    template <class LhsObject, class RhsObject>
    inline bool operator>(const IntrusivePtr<LhsObject>& lhs, const IntrusivePtr<RhsObject>& rhs)
    {
        return lhs.get() > rhs.get();
    }
    template <class LhsObject, class RhsObject>
    inline bool operator<(const IntrusivePtr<LhsObject>& lhs, const IntrusivePtr<RhsObject>& rhs)
    {
        return lhs.get() < rhs.get();
    }

    template <class Object>
    inline bool operator==(const IntrusivePtr<Object>& lhs, std::nullptr_t)
    {
        return lhs.get() == nullptr;
    }
    template <class Object>
    inline bool operator==(std::nullptr_t, const IntrusivePtr<Object>& rhs)
    {
        return nullptr == rhs.get();
    }
    template <class Object>
    inline bool operator!=(const IntrusivePtr<Object>& lhs, std::nullptr_t)
    {
        return lhs.get() != nullptr;
    }
    template <class Object>
    inline bool operator!=(std::nullptr_t, const IntrusivePtr<Object>& rhs)
    {
        return nullptr != rhs.get();
    }

    template <class Object>
    void swap(IntrusivePtr<Object>& lhs, IntrusivePtr<Object>& rhs)
    {
        lhs.swap(rhs);
    }
}
