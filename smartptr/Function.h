#pragma once

#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <type_traits>
#include <new>
#include "Noexcept.h"
#include "ClonePtr.h"

#define FUNCTION_ENABLE_DEBUG 01

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

    protected:
        template <class RealObj>
        struct ObjectAdapter
        {
            static TRet Invoke(char* pObj, TArgs... args)
            {
                return (*(RealObj*)pObj)(static_cast<TArgs&&>(args)...);
            }
        };

#if FUNCTION_ENABLE_DEBUG
        struct IDebug
        {
            // introduce a vtable, so that the debugger can display the derived type
            virtual void Dummy() {}
        };
        template <class RealObj>
        struct DebugObj : IDebug
        {
            const RealObj* pObj;

            DebugObj(const RealObj* pObj_)
                : pObj(pObj_)
            {
            }
        };
        struct DebugRawFn : IDebug
        {
            typename RawFn rawFn;

            DebugRawFn(RawFn rawFn_)
                : rawFn(rawFn_)
            {
            }
        };
        struct DebugBuffer
        {
            void* storage[2];
        };
#endif

    protected:
        typedef TRet(*WrapperFn)(char* pObj, TArgs... args);
        WrapperFn m_wrapperFn;
        char* m_pObj;
#if FUNCTION_ENABLE_DEBUG
        DebugBuffer m_debugBuffer;
#endif

    protected:
        void BaseInitNull()
        {
            m_wrapperFn = nullptr;
            m_pObj = nullptr;
        }
        void BaseInitCopy(const This& rhs)
        {
            m_wrapperFn = rhs.m_wrapperFn;
            m_pObj = rhs.m_pObj;
#if FUNCTION_ENABLE_DEBUG
            m_debugBuffer = rhs.m_debugBuffer;
#endif
        }
        void BaseInitRawFn(RawFn rawFn)
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
        template <class RealObj>
        void BaseInitFuncObj(RealObj&& realObj)
        {
            typedef typename std::decay<RealObj>::type Obj;
            m_wrapperFn = &ObjectAdapter<Obj>::Invoke;
            m_pObj = (char*)&realObj;
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

    template <class TSig, size_t SboSize = sizeof(void*)>
    class Function : public decltype(SelectFuncBase((TSig*)nullptr))
    {
    public:
        typedef decltype(SelectFuncBase((TSig*)nullptr)) Base;
        typedef Function<TSig, SboSize> This;

    private:
        // note: m_pDebug is placed first & in the derived class, for direct access in the debugger (fewer node expansions)
#if FUNCTION_ENABLE_DEBUG
        typename Base::IDebug* m_pDebug;
#endif
        const IClonePtrCloner* m_pCloner;
        char m_sbo[SboSize];

        // NOTE: Release() leaves m_pObj etc. pointing at a destructed object.
        // Callers must subsequently call some Init*() function (except in ~Function).
        void Release()
        {
            if (m_pCloner)
            {
                m_pCloner->Destruct(this->m_pObj, m_sbo, SboSize);
            }
        }

#if FUNCTION_ENABLE_DEBUG
        void InitDebugPtr(const This& rhs)
        {
            // TODO: make this typesafe
            this->m_debugBuffer.storage[0] = rhs.m_debugBuffer.storage[0];
            this->m_debugBuffer.storage[1] = m_pObj;
            m_pDebug = (typename Base::IDebug*)this->m_debugBuffer.storage;
        }
#endif

        void InitNull()
        {
            Base::BaseInitNull();
#if FUNCTION_ENABLE_DEBUG
            m_pDebug = nullptr;
#endif
        }
        void InitCopy(const This& rhs)
        {
            InitNull(); // reset members here, in case Copy() throws
            if (!rhs.m_pObj)
            {
                return;
            }
            this->m_pObj = rhs.m_pCloner->Copy(rhs.m_pObj, m_sbo, SboSize);
            this->m_wrapperFn = rhs.m_wrapperFn;
            m_pCloner = rhs.m_pCloner;
#if FUNCTION_ENABLE_DEBUG
            InitDebugPtr(rhs);
#endif
        }
        void InitMove(This&& rhs)
        {
            if (!rhs.m_pObj)
            {
                InitNull();
                return;
            }

            if (rhs.IsObjectInSboBuffer())
            {
                // RHS Object lives in its SBO; invoke the object's move constructor.
                this->m_pObj = rhs.m_pCloner->Move(rhs.m_pObj, m_sbo, SboSize);
                this->m_wrapperFn = rhs.m_wrapperFn;
                m_pCloner = rhs.m_pCloner;
#if FUNCTION_ENABLE_DEBUG
                InitDebugPtr(rhs);
#endif
                rhs.InitNull();
                return;
            }

            // steal the object pointer
            this->m_pObj = rhs.m_pObj;
            this->m_wrapperFn = rhs.m_wrapperFn;
            m_pCloner = rhs.m_pCloner;
#if FUNCTION_ENABLE_DEBUG
            InitDebugPtr(rhs);
#endif
            rhs.InitNull();
        }
        void InitRawFn(typename Base::RawFn rawFn)
        {
            Base::BaseInitRawFn(rawFn);
            m_pCloner = nullptr;
#if FUNCTION_ENABLE_DEBUG
            m_pDebug = new (this->m_debugBuffer.storage) DebugRawFn(rawFn);
#endif
        }
        template <class RealObj>
        void InitFuncObj(RealObj&& realObj)
        {
            InitNull(); // reset members here, in case the constructor throws
            typedef typename std::decay<RealObj>::type Obj;
            if (std::is_nothrow_move_constructible<Obj>::value && sizeof(Obj) <= SboSize)
            {
                Obj* pObj = new (m_sbo) Obj(static_cast<RealObj&&>(realObj));
                this->m_pObj = (char*)pObj;
            }
            else
            {
                Obj* pObj = new Obj(static_cast<RealObj&&>(realObj));
                this->m_pObj = (char*)pObj;
            }
            this->m_wrapperFn = &Base::ObjectAdapter<Obj>::Invoke;
            m_pCloner = &ClonePtrCloner<Obj>::Instance;
#if FUNCTION_ENABLE_DEBUG
            static_assert(sizeof(DebugObj<Obj>) <= sizeof(this->m_debugBuffer.storage), "");
            m_pDebug = new (this->m_debugBuffer.storage) DebugObj<Obj>(&realObj);
#endif
        }
        bool IsObjectInSboBuffer() const
        {
            bool result = (m_pObj - m_sbo) < SboSize;
            return result;
        }

    public:
        ~Function() CI0_NOEXCEPT(true)
        {
            Release();
        }
        Function() CI0_NOEXCEPT(true)
        {
            InitNull();
        }
        Function(std::nullptr_t) CI0_NOEXCEPT(true)
        {
            InitNull();
        }
        Function(const This& rhs)
        {
            InitCopy(rhs);
        }
        Function(const This&& rhs)
        {
            InitCopy(rhs);
        }
        Function(This& rhs)
        {
            InitCopy(rhs);
        }
        Function(This&& rhs) CI0_NOEXCEPT(true)
        {
            InitMove(static_cast<This&&>(rhs));
        }
        Function(typename Base::RawFn rawFn) CI0_NOEXCEPT(true)
        {
            InitRawFn(rawFn);
        }
        template <class RealObj>
        Function(RealObj&& realObj)
        {
            InitFuncObj(static_cast<RealObj&&>(realObj));
        }

        This& operator=(std::nullptr_t)
        {
            InitNull();
            return *this;
        }
        This& operator=(const This& rhs)
        {
            InitCopy(rhs);
            return *this;
        }
        This& operator=(const This&& rhs)
        {
            InitCopy(rhs);
            return *this;
        }
        This& operator=(This& rhs)
        {
            InitCopy(rhs);
            return *this;
        }
        This& operator=(This&& rhs) CI0_NOEXCEPT(true)
        {
            InitMove(static_cast<This&&>(rhs));
            return *this;
        }
        This& operator=(typename Base::RawFn rawFn) CI0_NOEXCEPT(true)
        {
            InitRawFn(rawFn);
            return *this;
        }
        template <class RealObj>
        This& operator=(RealObj&& realObj)
        {
            InitFuncObj(static_cast<RealObj&&>(realObj));
            return *this;
        }
    };

    // Specialization of ClonePtr with SboSize=0.
    template <class TSig>
    class Function<TSig, 0u> : public decltype(SelectFuncBase((TSig*)nullptr))
    {
    public:
        typedef decltype(SelectFuncBase((TSig*)nullptr)) Base;
        typedef Function<TSig, 0u> This;

    private:
        // note: m_pDebug is placed first & in the derived class, for direct access in the debugger (fewer node expansions)
#if FUNCTION_ENABLE_DEBUG
        typename Base::IDebug* m_pDebug;
#endif
        const IClonePtrCloner* m_pCloner;

        // NOTE: Release() leaves m_pObj etc. pointing at a destructed object.
        // Callers must subsequently call some Init*() function (except in ~Function).
        void Release()
        {
            if (m_pCloner)
            {
                m_pCloner->Destruct(this->m_pObj, nullptr, 0u);
            }
        }

#if FUNCTION_ENABLE_DEBUG
        void InitDebugPtr(const This& rhs)
        {
            // TODO: make this typesafe
            this->m_debugBuffer.storage[0] = rhs.m_debugBuffer.storage[0];
            this->m_debugBuffer.storage[1] = m_pObj;
            m_pDebug = (typename Base::IDebug*)this->m_debugBuffer.storage;
        }
#endif

        void InitNull()
        {
            Base::BaseInitNull();
            m_pCloner = nullptr;
#if FUNCTION_ENABLE_DEBUG
            m_pDebug = nullptr;
#endif
        }
        void InitCopy(const This& rhs)
        {
            InitNull(); // reset members here, in case Copy() throws
            if (!rhs.m_pObj)
            {
                return;
            }
            this->m_pObj = rhs.m_pCloner->Copy(rhs.m_pObj, nullptr, 0u);
            this->m_wrapperFn = rhs.m_wrapperFn;
            m_pCloner = rhs.m_pCloner;
#if FUNCTION_ENABLE_DEBUG
            InitDebugPtr(rhs);
#endif
        }
        void InitMove(This&& rhs)
        {
            if (!rhs.m_pObj)
            {
                InitNull();
                return;
            }

            if (rhs.IsObjectInSboBuffer())
            {
                // RHS Object lives in its SBO; invoke the object's move constructor.
                this->m_pObj = rhs.m_pCloner->Move(rhs.m_pObj, nullptr, 0u);
                this->m_wrapperFn = rhs.m_wrapperFn;
                m_pCloner = rhs.m_pCloner;
#if FUNCTION_ENABLE_DEBUG
                InitDebugPtr(rhs);
#endif
                rhs.InitNull();
                return;
            }

            // steal the object pointer
            this->m_pObj = rhs.m_pObj;
            this->m_wrapperFn = rhs.m_wrapperFn;
            m_pCloner = rhs.m_pCloner;
#if FUNCTION_ENABLE_DEBUG
            InitDebugPtr(rhs);
#endif
            rhs.InitNull();
        }
        void InitRawFn(typename Base::RawFn rawFn)
        {
            Base::BaseInitRawFn(rawFn);
            m_pCloner = nullptr;
#if FUNCTION_ENABLE_DEBUG
            m_pDebug = new (this->m_debugBuffer.storage) DebugRawFn(rawFn);
#endif
        }
        template <class RealObj>
        void InitFuncObj(RealObj&& realObj)
        {
            InitNull(); // reset members here, in case the constructor throws
            typedef typename std::decay<RealObj>::type Obj;
            Obj* pObj = new Obj(static_cast<RealObj&&>(realObj));
            this->m_pObj = (char*)pObj;
            this->m_wrapperFn = &Base::ObjectAdapter<Obj>::Invoke;
            m_pCloner = &ClonePtrCloner<Obj>::Instance;
#if FUNCTION_ENABLE_DEBUG
            static_assert(sizeof(DebugObj<Obj>) <= sizeof(this->m_debugBuffer.storage), "");
            m_pDebug = new (this->m_debugBuffer.storage) DebugObj<Obj>(&realObj);
#endif
        }
        bool IsObjectInSboBuffer() const
        {
            return false;
        }

    public:
        ~Function() CI0_NOEXCEPT(true)
        {
            Release();
        }
        Function() CI0_NOEXCEPT(true)
        {
            InitNull();
        }
        Function(std::nullptr_t) CI0_NOEXCEPT(true)
        {
            InitNull();
        }
        Function(const This& rhs)
        {
            InitCopy(rhs);
        }
        Function(const This&& rhs)
        {
            InitCopy(rhs);
        }
        Function(This& rhs)
        {
            InitCopy(rhs);
        }
        Function(This&& rhs) CI0_NOEXCEPT(true)
        {
            InitMove(static_cast<This&&>(rhs));
        }
        Function(typename Base::RawFn rawFn) CI0_NOEXCEPT(true)
        {
            InitRawFn(rawFn);
        }
        template <class RealObj>
        Function(RealObj&& realObj)
        {
            InitFuncObj(static_cast<RealObj&&>(realObj));
        }

        This& operator=(std::nullptr_t)
        {
            InitNull();
            return *this;
        }
        This& operator=(const This& rhs)
        {
            InitCopy(rhs);
            return *this;
        }
        This& operator=(const This&& rhs)
        {
            InitCopy(rhs);
            return *this;
        }
        This& operator=(This& rhs)
        {
            InitCopy(rhs);
            return *this;
        }
        This& operator=(This&& rhs) CI0_NOEXCEPT(true)
        {
            InitMove(static_cast<This&&>(rhs));
            return *this;
        }
        This& operator=(typename Base::RawFn rawFn) CI0_NOEXCEPT(true)
        {
            InitRawFn(rawFn);
            return *this;
        }
        template <class RealObj>
        This& operator=(RealObj&& realObj)
        {
            InitFuncObj(static_cast<RealObj&&>(realObj));
            return *this;
        }
    };

    template <class TSig>
    class FuncRef : public decltype(SelectFuncBase((TSig*)nullptr))
    {
    public:
        typedef decltype(SelectFuncBase((TSig*)nullptr)) Base;
        typedef FuncRef<TSig> This;

    private:
        // note: m_pDebug is placed first & in the derived class, for direct access in the debugger (fewer node expansions)
#if FUNCTION_ENABLE_DEBUG
        typename Base::IDebug* m_pDebug;
#endif

    private:
        void InitNull()
        {
            Base::BaseInitNull();
#if FUNCTION_ENABLE_DEBUG
            m_pDebug = nullptr;
#endif
        }
        void InitCopy(const This& rhs)
        {
            Base::BaseInitCopy(rhs);
#if FUNCTION_ENABLE_DEBUG
            m_pDebug = (typename Base::IDebug*)this->m_debugBuffer.storage;
#endif
        }
        void InitRawFn(typename Base::RawFn rawFn)
        {
            Base::BaseInitRawFn(rawFn);
#if FUNCTION_ENABLE_DEBUG
            m_pDebug = new (this->m_debugBuffer.storage) DebugRawFn(rawFn);
#endif
        }
        template <class RealObj>
        void InitFuncObj(RealObj&& realObj)
        {
            typedef typename std::decay<RealObj>::type Obj;
            Base::BaseInitFuncObj(static_cast<RealObj&&>(realObj));
#if FUNCTION_ENABLE_DEBUG
            static_assert(sizeof(DebugObj<Obj>) <= sizeof(this->m_debugBuffer.storage), "");
            m_pDebug = new (this->m_debugBuffer.storage) DebugObj<Obj>(&realObj);
#endif
        }
        template <size_t RhsSboSize>
        void InitFunction(const Function<TSig, RhsSboSize>& func)
        {
            Base::BaseInitCopy(func);
#if FUNCTION_ENABLE_DEBUG
            m_pDebug = (typename Base::IDebug*)this->m_debugBuffer.storage;
#endif
        }

    public:
        // no destructor because it's just a reference
        FuncRef() CI0_NOEXCEPT(true)
        {
            InitNull();
        }
        FuncRef(std::nullptr_t) CI0_NOEXCEPT(true)
        {
            InitNull();
        }
        FuncRef(const This& rhs) CI0_NOEXCEPT(true)
        {
            InitCopy(rhs);
        }
        FuncRef(const This&& rhs) CI0_NOEXCEPT(true)
        {
            InitCopy(rhs);
        }
        FuncRef(This& rhs) CI0_NOEXCEPT(true)
        {
            InitCopy(rhs);
        }
        FuncRef(This&& rhs) CI0_NOEXCEPT(true)
        {
            InitCopy(rhs);
        }
        template <size_t RhsSboSize>
        FuncRef(const Function<TSig, RhsSboSize>& func) CI0_NOEXCEPT(true)
        {
            InitFunction(func);
        }
        template <size_t RhsSboSize>
        FuncRef(const Function<TSig, RhsSboSize>&& func) CI0_NOEXCEPT(true)
        {
            InitFunction(func);
        }
        template <size_t RhsSboSize>
        FuncRef(Function<TSig, RhsSboSize>& func) CI0_NOEXCEPT(true)
        {
            InitFunction(func);
        }
        template <size_t RhsSboSize>
        FuncRef(Function<TSig, RhsSboSize>&& func) CI0_NOEXCEPT(true)
        {
            InitFunction(func);
        }
        FuncRef(typename Base::RawFn rawFn) CI0_NOEXCEPT(true)
        {
            InitRawFn(rawFn);
        }
        template <class RealObj>
        FuncRef(RealObj&& realObj) CI0_NOEXCEPT(true)
        {
            InitFuncObj(static_cast<RealObj&&>(realObj));
        }

        This& operator=(std::nullptr_t) CI0_NOEXCEPT(true)
        {
            InitNull();
            return *this;
        }
        This& operator=(const This& rhs) CI0_NOEXCEPT(true)
        {
            InitCopy(rhs);
            return *this;
        }
        This& operator=(const This&& rhs) CI0_NOEXCEPT(true)
        {
            InitCopy(rhs);
            return *this;
        }
        This& operator=(This& rhs) CI0_NOEXCEPT(true)
        {
            InitCopy(rhs);
            return *this;
        }
        This& operator=(This&& rhs) CI0_NOEXCEPT(true)
        {
            InitCopy(rhs);
            return *this;
        }
        template <size_t RhsSboSize>
        This& operator=(const Function<TSig, RhsSboSize>& func) CI0_NOEXCEPT(true)
        {
            InitFunction(func);
            return *this;
        }
        template <size_t RhsSboSize>
        This& operator=(const Function<TSig, RhsSboSize>&& func) CI0_NOEXCEPT(true)
        {
            InitFunction(func);
            return *this;
        }
        template <size_t RhsSboSize>
        This& operator=(Function<TSig, RhsSboSize>& func) CI0_NOEXCEPT(true)
        {
            InitFunction(func);
            return *this;
        }
        template <size_t RhsSboSize>
        This& operator=(Function<TSig, RhsSboSize>&& func) CI0_NOEXCEPT(true)
        {
            InitFunction(func);
            return *this;
        }
        This& operator=(typename Base::RawFn rawFn) CI0_NOEXCEPT(true)
        {
            AssignRaw(rawFn);
            return *this;
        }
        template <class RealObj>
        This& operator=(RealObj&& realObj) CI0_NOEXCEPT(true)
        {
            InitFuncObj(static_cast<RealObj&&>(realObj));
            return *this;
        }
    };

}

#if _MSC_VER
#pragma warning(pop)
#endif
