#pragma once
#include <stddef.h>
#include <assert.h>
#include <algorithm>

namespace ci0 {

    template <class Object>
    void GlobalDeleteObject(Object* pObject)
    {
        delete pObject;
    }

    template <class Object, void(*DeleteObject)(Object*) = &GlobalDeleteObject<Object> >
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
            ~OutParam()
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
            explicit OutParam(This& self)
                : m_pSelf(&self)
                , m_pObject(self.m_pObject)
            {
            }
            OutParam(OutParam&& rhs)
                : m_pSelf(rhs.m_pSelf)
                , m_pObject(rhs.m_pObject)
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
        ~UniquePtr()
        {
            Release();
        }
        UniquePtr()
            : m_pObject()
        {
        }
        UniquePtr(nullptr_t)
            : m_pObject()
        {
        }
        UniquePtr(This&& rhs)
        {
            Assign(std::move(rhs));
        }
        UniquePtr& operator=(This&& rhs)
        {
            if (this != &rhs)
            {
                Assign(std::move<This>(rhs));
                return *this;
            }
        }

        explicit UniquePtr(Object* pObject)
            : m_pObject(pObject)
        {
        }

        Object& operator*() const
        {
            return *m_pObject;
        }
        Object* operator->() const
        {
            return m_pObject;
        }

        operator Object*() const
        {
            return m_pObject;
        }
        explicit operator bool() const
        {
            return !!m_pObject;
        }
        template <class Type>
        explicit operator Type*() const
        {
            return static_cast<Type*>(m_pObject);
        }

        Object* const& Get() const
        {
            return m_pObject;
        }
        OutParam Out()
        {
            return OutParam(*this);
        }
        This& Attach(Object* pObject)
        {
            assert(m_pObject != pObject);
            Release();
            m_pObject = pObject;
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
        UniquePtr<Other> MoveAs()
        {
            UniquePtr<Other> pOther(static_cast<Other*>(m_pObject));
            m_pObject = nullptr;
            return pOther;
        }
    };

}
