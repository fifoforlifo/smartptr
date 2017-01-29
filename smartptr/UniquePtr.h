#pragma once
#include <stddef.h>
#include <assert.h>

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

    private: // deleted members
        UniquePtr(const This& rhs);
        UniquePtr& operator=(const This& rhs);

        // prevent naked delete from compiling; http://stackoverflow.com/a/3312507
        struct DontDeleteMeBro;
        operator DontDeleteMeBro*() const;

    public:
        friend class OutParam;
        class OutParam
        {
            This* m_pSelf;
            Object* m_pObject;

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
            Assign(rhs);
        }
        UniquePtr& operator=(This&& rhs)
        {
            Assign(rhs);
            return *this;
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

        OutParam Out()
        {
            return OutParam(*this);
        }
    };

}
