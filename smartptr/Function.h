#pragma once

#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <new>

#define FUNCREF_ENABLE_DEBUG 01

#if _MSC_VER
#pragma warning(push)
#pragma warning (disable : 4521) // "multiple copy constructors specified"
#pragma warning (disable : 4522) // "multiple assignment operators specified"
#endif

namespace ci0 {

    template <class TRet, class... TArgs>
    class FuncBase
    {
    public:
        typedef FuncBase<TRet, TArgs...> This;
        typedef TRet(*RawFn)(TArgs... args);

    private:
        typedef TRet(*WrapperFn)(char* pObj, TArgs... args);
        WrapperFn m_wrapperFn;
        char* m_pObj;

    protected:
        void BaseInitNull()
        {
            m_wrapperFn = nullptr;
            m_pObj = nullptr;
        }
        void BaseAssignCopy(const This& rhs)
        {
            m_wrapperFn = rhs.m_wrapperFn;
            m_pObj = rhs.m_pObj;
        }
        void BaseAssignRaw(RawFn rawFn)
        {
            struct Adapter
            {
                static TRet Invoke(char* pObj, TArgs... args)
                {
                    RawFn rawFn = (RawFn)pObj;
                    return rawFn(static_cast<TArgs&&>(args)...);
                }
            };
            m_wrapperFn = &Adapter::Invoke;
            m_pObj = (char*)rawFn;
        }
        template <class FuncObj>
        void BaseAssignFuncObj(FuncObj&& funcObj)
        {
            struct Adapter
            {
                static TRet Invoke(char* pObj, TArgs... args)
                {
                    FuncObj& funcObj = *(FuncObj*)pObj;
                    return funcObj(static_cast<TArgs&&>(args)...);
                }
            };
            m_wrapperFn = &Adapter::Invoke;
            m_pObj = (char*)&funcObj;
        }

    protected:
        void SetFnObj(const This& rhs, char* pObj)
        {
            m_wrapperFn = rhs.m_wrapperFn;
            m_pObj = pObj;
        }

        // no destructor
        // note: The default constructor does not initialize fields;
        // the derived class must call the appropriate BaseAssign*() function instead.
        FuncBase()
        {
        }

    public:
        explicit operator bool() const
        {
            return !!m_wrapperFn;
        }

        inline TRet operator()(TArgs... args) const
        {
            return m_wrapperFn(m_pObj, args...);
        }
    };

    template <class TRet, class... TArgs>
    FuncBase<TRet, TArgs...> SelectFuncBase(TRet(*)(TArgs...));

    template <class TSig>
    class FuncRef : public decltype(SelectFuncBase((TSig*)nullptr))
    {
    public:
        typedef decltype(SelectFuncBase((TSig*)nullptr)) Base;
        typedef FuncRef<TSig> This;

    private:
#if FUNCREF_ENABLE_DEBUG
        struct IRealObjPtr
        {
            // introduce a vtable, so that the debugger can display the derived type
            virtual void Dummy() {}
        };
        template <class RealObj>
        struct RealObjPtr : IRealObjPtr
        {
            const RealObj* pObj;

            RealObjPtr(const RealObj* pObj_)
                : pObj(pObj_)
            {
            }
        };
        struct RealObjRawFnPtr : IRealObjPtr
        {
            typename Base::RawFn rawFn;

            RealObjRawFnPtr(typename Base::RawFn rawFn_)
                : rawFn(rawFn_)
            {
            }
        };
        struct RealObjPtrBuffer
        {
            void* storage[2];
        };

        IRealObjPtr* m_pRealObj;
        RealObjPtrBuffer m_realObjPtrBuffer;
#endif

        void InitNull()
        {
            Base::BaseInitNull();
#if FUNCREF_ENABLE_DEBUG
            m_pRealObj = nullptr;
#endif
        }
        void AssignCopy(const This& rhs)
        {
            Base::BaseAssignCopy(rhs);
#if FUNCREF_ENABLE_DEBUG
            m_realObjPtrBuffer = rhs.m_realObjPtrBuffer;
            m_pRealObj = (IRealObjPtr*)m_realObjPtrBuffer.storage;
#endif
        }
        void AssignRawFn(typename Base::RawFn rawFn)
        {
            Base::BaseAssignRaw(rawFn);
#if FUNCREF_ENABLE_DEBUG
            m_pRealObj = new (m_realObjPtrBuffer.storage) RealObjRawFnPtr(rawFn);
#endif
        }
        template <class FuncObj>
        void AssignFuncObj(FuncObj& funcObj)
        {
            Base::BaseAssignFuncObj(static_cast<FuncObj&&>(funcObj));
#if FUNCREF_ENABLE_DEBUG
            static_assert(sizeof(RealObjPtr<FuncObj>) <= sizeof(m_realObjPtrBuffer.storage), "");
            m_pRealObj = new (m_realObjPtrBuffer.storage) RealObjPtr<FuncObj>(&funcObj);
#endif
        }

    public:
        FuncRef()
        {
            InitNull();
        }
        FuncRef(std::nullptr_t)
        {
            InitNull();
        }
        FuncRef(const This& rhs)
        {
            AssignCopy(rhs);
        }
        FuncRef(const This&& rhs)
        {
            AssignCopy(rhs);
        }
        FuncRef(This& rhs)
        {
            AssignCopy(rhs);
        }
        FuncRef(This&& rhs)
        {
            AssignCopy(rhs);
        }
        FuncRef(typename Base::RawFn rawFn)
        {
            AssignRawFn(rawFn);
        }
        template <class FuncObj>
        FuncRef(FuncObj& funcObj)
        {
            AssignFuncObj(funcObj);
        }

        This& operator=(std::nullptr_t)
        {
            InitNull();
            return *this;
        }
        This& operator=(const This& rhs)
        {
            AssignCopy(rhs);
            return *this;
        }
        This& operator=(const This&& rhs)
        {
            AssignCopy(rhs);
            return *this;
        }
        This& operator=(This& rhs)
        {
            AssignCopy(rhs);
            return *this;
        }
        This& operator=(This&& rhs)
        {
            AssignCopy(rhs);
            return *this;
        }
        This& operator=(typename Base::RawFn rawFn)
        {
            AssignRaw(rawFn);
            return *this;
        }
        template <class FuncObj>
        This& operator=(FuncObj& funcObj)
        {
            AssignFuncObj(funcObj);
            return *this;
        }
    };

}

#if _MSC_VER
#pragma warning(pop)
#endif
