#pragma once

#include <stddef.h>
#include <stdint.h>
#include <utility>

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

    public:
        FuncRef()
        {
            Base::BaseInitNull();
        }
        FuncRef(std::nullptr_t)
        {
            Base::BaseInitNull();
        }
        FuncRef(const This& rhs)
        {
            Base::BaseAssignCopy(rhs);
        }
        FuncRef(const This&& rhs)
        {
            Base::BaseAssignCopy(rhs);
        }
        FuncRef(This& rhs)
        {
            Base::BaseAssignCopy(rhs);
        }
        FuncRef(This&& rhs)
        {
            Base::BaseAssignCopy(rhs);
        }
        FuncRef(typename Base::RawFn rawFn)
        {
            Base::BaseAssignRaw(rawFn);
        }
        template <class FuncObj>
        FuncRef(FuncObj&& funcObj)
        {
            Base::BaseAssignFuncObj(static_cast<FuncObj&&>(funcObj));
        }

        This& operator=(std::nullptr_t)
        {
            Base::BaseInitNull();
            return *this;
        }
        This& operator=(const This& rhs)
        {
            Base::BaseAssignCopy(rhs);
            return *this;
        }
        This& operator=(const This&& rhs)
        {
            Base::BaseAssignCopy(rhs);
            return *this;
        }
        This& operator=(This& rhs)
        {
            Base::BaseAssignCopy(rhs);
            return *this;
        }
        This& operator=(This&& rhs)
        {
            Base::BaseAssignCopy(rhs);
            return *this;
        }
        This& operator=(typename Base::RawFn rawFn)
        {
            Base::BaseAssignRaw(rawFn);
            return *this;
        }
        template <class FuncObj>
        This& operator=(FuncObj&& funcObj)
        {
            Base::BaseAssignFuncObj(static_cast<FuncObj&&>(funcObj));
            return *this;
        }
    };

}

#if _MSC_VER
#pragma warning(pop)
#endif
