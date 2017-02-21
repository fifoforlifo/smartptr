#pragma once
#include <stddef.h>
#include <assert.h>
#include <algorithm>

namespace ci0 {

    // IntrusivePtr assignment rules are similar to CComPtr
    //  *   Assignment from a raw pointer acts like a copy; add_ref() is called.
    //  *   The constructor performs add_ref() by default, for parity with assignment.
    //  *   Taking ownership requires calling Attach(), passing addRef=false.
    //  *   Taking ownership at construction time requires an explicit constructor
    //      call, passing addRef=false.
    //
    // Most new objects have an initial refcount of 1, to avoid the perf overhead
    // of an initial add_ref() call.  Such objects should never be created like this:
    //      IntrusivePtr<Foo> pFoo = new Foo();
    //      pFoo = new Foo();
    // In both cases, a memory leak will occur since the constructor/operator= will
    // perform an additional add_ref(), resulting in a single reference with refcount=2.


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
            ~OutParam()
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
            explicit OutParam(This& self, bool assumeInitialAddRef)
                : m_pSelf(&self)
                , m_pObject()
                , m_assumeInitialAddRef(assumeInitialAddRef)
            {
            }
            OutParam(OutParam&& rhs)
                : m_pSelf(rhs.m_pSelf)
                , m_pObject(rhs.m_pObject)
                , m_assumeInitialAddRef(rhs.m_assumeInitialAddRef)
            {
                rhs.m_pSelf = nullptr;
                rhs.m_pObject = nullptr;
            }

            operator Object* const*() const
            {
                assert(m_pSelf);
                return &m_pObject;
            }
            operator Object**()
            {
                assert(m_pSelf);
                return &m_pObject;
            }
        };

    public:
        ~IntrusivePtr()
        {
            Release();
        }
        IntrusivePtr()
            : m_pObject()
        {
        }
        IntrusivePtr(nullptr_t)
            : m_pObject()
        {
        }
        IntrusivePtr(const This& rhs)
        {
            CopyAssign(rhs);
        }
        IntrusivePtr(This&& rhs)
        {
            m_pObject = rhs.m_pObject;
            rhs.m_pObject = nullptr;
        }
        IntrusivePtr& operator=(const This& rhs)
        {
            if (m_pObject != rhs.m_pObject)
            {
                Release();
                CopyAssign(rhs);
            }
            return *this;
        }
        IntrusivePtr& operator=(This&& rhs)
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
        IntrusivePtr& operator=(Object* pObject)
        {
            if (m_pObject != pObject)
            {
                Release();
                m_pObject = pObject;
                SafeAddRef(pObject);
            }
            return *this;
        }

        Object& operator*() const
        {
            return *m_pObject;
        }
        Object* operator->() const
        {
            return m_pObject;
        }
        template <class Type>
        explicit operator Type*() const
        {
            return static_cast<Type*>(m_pObject);
        }

        operator Object*() const
        {
            return m_pObject;
        }
        explicit operator bool() const
        {
            return !!m_pObject;
        }

        Object* const& Get() const
        {
            return m_pObject;
        }
        OutParam Out(bool assumeInitialAddRef = true)
        {
            Release();
            return OutParam(*this, assumeInitialAddRef);
        }
        This& Attach(Object* pObject, bool addRef)
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
        Object* Detach()
        {
            Object* pObject = m_pObject;
            m_pObject = nullptr;
            return pObject;
        }
        This& Swap(This& rhs)
        {
            std::swap(m_pObject, rhs.m_pObject);
            return *this;
        }
        This& Swap(This&& rhs)
        {
            std::swap(m_pObject, rhs.m_pObject);
            return *this;
        }
        This& Reset()
        {
            Release();
            return *this;
        }
        template <class Other>
        IntrusivePtr<Other> MoveAs()
        {
            UniquePtr<Other> pOther(static_cast<Other*>(m_pObject));
            m_pObject = nullptr;
            return pOther;
        }
    };

}
