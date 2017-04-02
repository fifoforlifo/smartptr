#pragma once
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <algorithm>
#include <utility>
#include "Noexcept.h"

#if _MSC_VER
#pragma warning(push)
#pragma warning (disable : 4521) // "multiple copy constructors specified"
#pragma warning (disable : 4522) // "multiple assignment operators specified"
#endif

namespace ci0 {

    struct IClonePtrCloner
    {
        const size_t sizeofObject;

        virtual ~IClonePtrCloner() {}
        IClonePtrCloner(size_t sizeofObject_)
            : sizeofObject(sizeofObject_)
        {
        }

        virtual char* Copy(const char* pRhsObj, char* pSbo, size_t sboSize) const = 0;
        virtual char* Move(char* pRhsObj, char* pSbo, size_t sboSize) const = 0;
        virtual void Destruct(char* pObj, char* pSbo, size_t sboSize) const = 0;
    };

    template <class Object>
    struct ClonePtrCloner : public IClonePtrCloner
    {
        ClonePtrCloner()
            : IClonePtrCloner(sizeof(Object))
        {
        }

        virtual char* Copy(const char* pRhsObj, char* pSbo, size_t sboSize) const
        {
            const Object& rhs = *(Object*)pRhsObj;

            // In ClonePtr<>, the copy-assignment operator must copy-then-move to be exception-safe.
            // This requires the move to be noexcept.
            if (std::is_nothrow_move_constructible<Object>::value)
            {
                if (sizeof(Object) <= sboSize)
                {
                    Object* pNew = new (pSbo) Object(rhs);
                    return (char*)pNew;
                }
            }

            Object* pNew = new Object(rhs);
            return (char*)pNew;
        }

        // Move() is only called when is_nothrow_move_constructible<Object>::value == true.
        virtual char* Move(char* pRhsObj, char* pSbo, size_t sboSize) const
        {
            Object& rhs = *(Object*)pRhsObj;
            if (sizeof(Object) <= sboSize)
            {
                Object* pNew = new (pSbo) Object(std::move(rhs));
                return (char*)pNew;
            }

            Object* pNew = new Object(std::move(rhs));
            return (char*)pNew;
        }

        virtual void Destruct(char* pObj, char* pSbo, size_t sboSize) const
        {
            Object* pObject = (Object*)pObj;
            if (uintptr_t(pObj - pSbo) < sboSize)
            {
                pObject->~Object();
                return;
            }

            delete pObject;
        }

        static ClonePtrCloner<Object> Instance;
    };

    template <class Object>
    ClonePtrCloner<Object> ClonePtrCloner<Object>::Instance;

    template <class Interface, size_t SboSize = sizeof(void*)>
    class ClonePtr
    {
    public:
        typedef ClonePtr<Interface, SboSize> This;

        template <class RhsInterface, size_t RhsSboSize>
        friend class ClonePtr;

    private:
        Interface* m_pInterface;
        char* m_pObject;
        const IClonePtrCloner* m_pCloner;
        char m_sbo[SboSize];

    private:
        template <class Src, class Dst>
        struct CastImplicit
        {
            typename std::remove_reference<Dst>::type* operator()(typename std::remove_reference<Src>::type* pSrc)
            {
                return pSrc;
            }
        };

    private:
        // prevent naked delete from compiling; http://stackoverflow.com/a/3312507
        struct PreventDelete;
        operator PreventDelete*() const;

    private:
        // NOTE: Release() leaves m_pObject etc. pointing at a destructed object.
        // Callers must subsequently call some Init*() function (except in ~ClonePtr).
        void Release()
        {
            if (m_pInterface)
            {
                m_pCloner->Destruct(m_pObject, m_sbo, SboSize);
            }
        }

        void InitNull()
        {
            m_pInterface = nullptr;
            m_pObject = nullptr;
            m_pCloner = nullptr;
        }

        // NOTE: InitCopy*() and InitMove*() are written out to produce clear error messages when
        // incompatible types are assigned.
        //
        // DO NOT refactor or collapse these with a template argument like CastToInterface.
        //
        // The difference is: (cryptic)
        //      error: no matching member function for call to 'InitCopy_ImplicitCast'
        //      note: candidate template ignored: substitution failure
        // versus: (clear)
        //      error: assigning to 'Derived *' from incompatible type 'Base *'

        template <class RhsInterface, size_t RhsSboSize>
        void InitCopy_ImplicitCast(const ClonePtr<RhsInterface, RhsSboSize>& rhs)
        {
            InitNull(); // reset members here, in case Copy() throws
            if (!rhs.m_pInterface)
            {
                return;
            }
            m_pObject = rhs.m_pCloner->Copy(rhs.m_pObject, m_sbo, SboSize);
            m_pInterface = (RhsInterface*)(m_pObject + ((char*)rhs.m_pInterface - rhs.m_pObject));
            m_pCloner = rhs.m_pCloner;
        }
        template <class RhsInterface, size_t RhsSboSize>
        void InitCopy_StaticCast(const ClonePtr<RhsInterface, RhsSboSize>& rhs)
        {
            InitNull(); // reset members here, in case Copy() throws
            if (!rhs.m_pInterface)
            {
                return;
            }
            m_pObject = rhs.m_pCloner->Copy(rhs.m_pObject, m_sbo, SboSize);
            m_pInterface = static_cast<Interface*>((RhsInterface*)(m_pObject + ((char*)rhs.m_pInterface - rhs.m_pObject)));
            m_pCloner = rhs.m_pCloner;
        }

        template <class RhsInterface, size_t RhsSboSize>
        void InitMove_ImplicitCast(ClonePtr<RhsInterface, RhsSboSize>&& rhs)
        {
            if (!rhs.m_pInterface)
            {
                InitNull();
                return;
            }

            if (rhs.IsObjectInSboBuffer())
            {
                // we cannot steal the object pointer; the move-constructor must be invoked dynamically
                m_pObject = rhs.m_pCloner->Move(rhs.m_pObject, m_sbo, SboSize);
                m_pInterface = (RhsInterface*)(m_pObject + ((char*)rhs.m_pInterface - rhs.m_pObject));
                m_pCloner = rhs.m_pCloner;
                rhs.InitNull();
                return;
            }

            // steal the object pointer
            m_pInterface = rhs.m_pInterface;
            m_pObject = rhs.m_pObject;
            m_pCloner = rhs.m_pCloner;
            rhs.InitNull();
        }
        template <class RhsInterface, size_t RhsSboSize>
        void InitMove_StaticCast(ClonePtr<RhsInterface, RhsSboSize>&& rhs)
        {
            if (!rhs.m_pInterface)
            {
                InitNull();
                return;
            }

            if (rhs.IsObjectInSboBuffer())
            {
                // we cannot steal the object pointer; the move-constructor must be invoked dynamically
                m_pObject = rhs.m_pCloner->Move(rhs.m_pObject, m_sbo, SboSize);
                m_pInterface = static_cast<Interface*>((RhsInterface*)(m_pObject + ((char*)rhs.m_pInterface - rhs.m_pObject)));
                m_pCloner = rhs.m_pCloner;
                rhs.InitNull();
                return;
            }

            // steal the object pointer
            m_pInterface = rhs.m_pInterface;
            m_pObject = rhs.m_pObject;
            m_pCloner = rhs.m_pCloner;
            rhs.InitNull();
        }

#if defined(__GNUC__)
// Silence a spurious warning that an object is being placement-new'd into a too-small buffer; the placement-new is appropriately guarded (guard is even statically evaluatable!).
// Actual warning text on:
//      warning: placement new constructing an object of type 'Obj {aka Thing}' and size '24' in a region of type 'char [8]' and size '8'[-Wplacement-new=]
//          Obj* pObject = new (m_sbo) Obj(std::forward<Object>(obj));
//                         ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wplacement-new"
#endif
        // Takes an Object that implements Interface, allocates a second instance of
        // that Object, and either copy or move constructs into the second instance.
        template <class Object, class CastToInterface>
        void AssignObjectValue(Object&& obj, CastToInterface&& castToInterface)
        {
            InitNull(); // reset members here, in case the constructor throws
            typedef typename std::decay<Object>::type Obj;
            if (std::is_nothrow_move_constructible<Obj>::value && sizeof(Obj) <= SboSize)
            {
                Obj* pObject = new (m_sbo) Obj(std::forward<Object>(obj));
                m_pInterface = castToInterface(pObject);
                m_pObject = (char*)pObject;
            }
            else
            {
                Obj* pObject = new Obj(std::forward<Object>(obj));
                m_pInterface = castToInterface(pObject);
                m_pObject = (char*)pObject;
            }
            m_pCloner = &ClonePtrCloner<Obj>::Instance;
        }
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

        bool IsObjectInSboBuffer() const
        {
            bool result = (m_pObject - m_sbo) < SboSize;
            return result;
        }

    public:
        ~ClonePtr() CI0_NOEXCEPT(true)
        {
            Release();
        }

        ClonePtr() CI0_NOEXCEPT(true)
        {
            InitNull();
        }
        ClonePtr(nullptr_t) CI0_NOEXCEPT(true)
        {
            InitNull();
        }

        ClonePtr(const This& rhs)
        {
            InitCopy_ImplicitCast(rhs);
        }
        ClonePtr(const This&& rhs)
        {
            InitCopy_ImplicitCast(rhs);
        }
        ClonePtr(This& rhs)
        {
            InitCopy_ImplicitCast(rhs);
        }
        ClonePtr(This&& rhs) CI0_NOEXCEPT(true)
        {
            InitMove_ImplicitCast(std::move(rhs));
        }
        template <class RhsInterface, size_t RhsSboSize>
        ClonePtr(const ClonePtr<RhsInterface, RhsSboSize>& rhs)
        {
            InitCopy_ImplicitCast(rhs);
        }
        template <class RhsInterface, size_t RhsSboSize>
        ClonePtr(const ClonePtr<RhsInterface, RhsSboSize>&& rhs)
        {
            InitCopy_ImplicitCast(rhs);
        }
        template <class RhsInterface, size_t RhsSboSize>
        ClonePtr(ClonePtr<RhsInterface, RhsSboSize>& rhs)
        {
            InitCopy_ImplicitCast(rhs);
        }
        template <class RhsInterface, size_t RhsSboSize>
        ClonePtr(ClonePtr<RhsInterface, RhsSboSize>&& rhs) CI0_NOEXCEPT(true)
        {
            InitMove_ImplicitCast(std::move(rhs));
        }

        // Copy or move a concrete object in.
        template <class Object>
        explicit ClonePtr(Object&& obj)
        {
            AssignObjectValue(std::forward<Object>(obj), CastImplicit<Object, Interface>());
        }

        This& operator=(const This& rhs)
        {
            if (this != &rhs)
            {
                This other(rhs);
                Release();
                InitMove_ImplicitCast(std::move(other));
            }
            return *this;
        }
        This& operator=(const This&& rhs)
        {
            if (this != &rhs)
            {
                This other(rhs);
                Release();
                InitMove_ImplicitCast(std::move(other));
            }
            return *this;
        }
        This& operator=(This& rhs)
        {
            if (this != &rhs)
            {
                This other(rhs);
                Release();
                InitMove_ImplicitCast(std::move(other));
            }
            return *this;
        }
        This& operator=(This&& rhs) CI0_NOEXCEPT(true)
        {
            if (this != &rhs)
            {
                Release();
                InitMove_ImplicitCast(std::move(rhs));
            }
            return *this;
        }
        template <class RhsInterface, size_t RhsSboSize>
        This& operator=(const ClonePtr<RhsInterface, RhsSboSize>& rhs)
        {
            This other(rhs);
            Release();
            InitMove_ImplicitCast(std::move(other));
            return *this;
        }
        template <class RhsInterface, size_t RhsSboSize>
        This& operator=(const ClonePtr<RhsInterface, RhsSboSize>&& rhs)
        {
            This other(rhs);
            Release();
            InitMove_ImplicitCast(std::move(other));
            return *this;
        }
        template <class RhsInterface, size_t RhsSboSize>
        This& operator=(ClonePtr<RhsInterface, RhsSboSize>& rhs)
        {
            This other(rhs);
            Release();
            InitMove_ImplicitCast(std::move(other));
            return *this;
        }
        template <class RhsInterface, size_t RhsSboSize>
        This& operator=(ClonePtr<RhsInterface, RhsSboSize>&& rhs) CI0_NOEXCEPT(true)
        {
            Release();
            InitMove_ImplicitCast(std::move(rhs));
            return *this;
        }
        This& operator=(nullptr_t) CI0_NOEXCEPT(true)
        {
            Release();
            InitNull();
            return *this;
        }

        // Copy or move a concrete object in.
        template <class Object>
        This& assign(Object&& obj)
        {
            Release();
            AssignObjectValue(std::forward<Object>(obj), CastImplicit<Object, Interface>());
            return *this;
        }

        // Copy or move a concrete object in.
        // The CastToInterface() function-object argument can be used to disambiguate
        //      struct Base1 : CommonBase {};
        //      struct Base2 : CommonBase {};
        //      struct Thing : Base1, Base2 {};
        //      ClonePtr<CommonBase> pCommonBase;
        //      // error: ambiguous, because CommonBase reachable through Base1 and Base2
        //      pCommonBase.Assign( Thing() );
        //      // OK, because CommonBase unambiguously reachable from Base
        //      pCommonBase.Assign( Thing(), [](Thing* pThing) { return (Base1*)pThing; })
        template <class Object, class CastToInterface>
        This& assign(Object&& obj, CastToInterface&& castToInterface)
        {
            Release();
            AssignObjectValue(std::forward<Object>(obj), std::forward<CastToInterface>(castToInterface));
            return *this;
        }

        // Takes ownership of pObject.  Allows initialization without copying.
        //      ClonePtr<Base> pBase = ClonePtr<Base>().attach(new Derived(...));
        template <class Object>
        This& attach(Object* pObject) CI0_NOEXCEPT(true)
        {
            Release();
            m_pObject = (char*)pObject;
            m_pInterface = pObject;
            m_pCloner = &ClonePtrCloner<Object>::Instance;
            return *this;
        }
        // No detach() in the current design because:
        //  (a) if Interface lacks a public virtual destructor, the caller cannot delete the object
        //  (b) if object resides in SBO, detach() would require a new allocation (no longer noexcept)
        // This could be revisited though ...

        // TODO: optimized swap() requires handling all 4 cases of {sbo, !sbo}x{rhsSbo, !rhsSbo}
        This& swap(This& rhs)
        {
            std::swap(*this, rhs);
            return *this;
        }
        This& swap(This&& rhs)
        {
            std::swap(*this, rhs);
            return *this;
        }

        This& reset() CI0_NOEXCEPT(true)
        {
            Release();
            InitNull();
            return *this;
        }
        // note: present for STL/boost compatibility, but you should prefer to call attach() instead
        template <class Object>
        This& reset(Object* pObject) CI0_NOEXCEPT(true)
        {
            return attach(pObject);
        }

        template <class RhsInterface, size_t RhsSboSize = sizeof(void*)>
        ClonePtr<RhsInterface, RhsSboSize> copy_as()
        {
            ClonePtr<RhsInterface, RhsSboSize> pOther;
            pOther.InitCopy_StaticCast(*this);
            return pOther;
        }
        template <class RhsInterface, size_t RhsSboSize = sizeof(void*)>
        ClonePtr<RhsInterface, RhsSboSize> move_as()
        {
            ClonePtr<RhsInterface, RhsSboSize> pOther;
            pOther.InitMove_StaticCast(*this);
            return pOther;
        }

        Interface* const& get() const CI0_NOEXCEPT(true)
        {
            return m_pInterface;
        }

        Interface& operator*() const CI0_NOEXCEPT(true)
        {
            return *m_pInterface;
        }
        Interface* operator->() const CI0_NOEXCEPT(true)
        {
            return m_pInterface;
        }

        operator Interface*() const CI0_NOEXCEPT(true)
        {
            return m_pInterface;
        }
        explicit operator bool() const CI0_NOEXCEPT(true)
        {
            return !!m_pInterface;
        }
        template <class Type>
        explicit operator Type*() const CI0_NOEXCEPT(true)
        {
            return static_cast<Type*>(m_pInterface);
        }
    };

    // Specialization of ClonePtr with SboSize=0.
    template <class Interface>
    class ClonePtr<Interface, 0u>
    {
    public:
        typedef ClonePtr<Interface, 0u> This;

        template <class RhsInterface, size_t RhsSboSize>
        friend class ClonePtr;

    private:
        Interface* m_pInterface;
        char* m_pObject;
        const IClonePtrCloner* m_pCloner;

    private:
        template <class Src, class Dst>
        struct CastImplicit
        {
            typename std::remove_reference<Dst>::type* operator()(typename std::remove_reference<Src>::type* pSrc)
            {
                return pSrc;
            }
        };

    private:
        // prevent naked delete from compiling; http://stackoverflow.com/a/3312507
        struct PreventDelete;
        operator PreventDelete*() const;

    private:
        // NOTE: Release() leaves m_pObject etc. pointing at a destructed object.
        // Callers must subsequently call some Init*() function (except in ~ClonePtr).
        void Release()
        {
            if (m_pInterface)
            {
                m_pCloner->Destruct(m_pObject, nullptr, 0u);
            }
        }

        void InitNull()
        {
            m_pInterface = nullptr;
            m_pObject = nullptr;
            m_pCloner = nullptr;
        }

        // NOTE: InitCopy*() and InitMove*() are written out to produce clear error messages when
        // incompatible types are assigned.
        //
        // DO NOT refactor or collapse these with a template argument like CastToInterface.
        //
        // The difference is: (cryptic)
        //      error: no matching member function for call to 'InitCopy_ImplicitCast'
        //      note: candidate template ignored: substitution failure
        // versus: (clear)
        //      error: assigning to 'Derived *' from incompatible type 'Base *'

        template <class RhsInterface, size_t RhsSboSize>
        void InitCopy_ImplicitCast(const ClonePtr<RhsInterface, RhsSboSize>& rhs)
        {
            InitNull(); // reset members here, in case Copy() throws
            if (!rhs.m_pInterface)
            {
                return;
            }
            m_pObject = rhs.m_pCloner->Copy(rhs.m_pObject, nullptr, 0u);
            m_pInterface = (RhsInterface*)(m_pObject + ((char*)rhs.m_pInterface - rhs.m_pObject));
            m_pCloner = rhs.m_pCloner;
        }
        template <class RhsInterface, size_t RhsSboSize>
        void InitCopy_StaticCast(const ClonePtr<RhsInterface, RhsSboSize>& rhs)
        {
            InitNull(); // reset members here, in case Copy() throws
            if (!rhs.m_pInterface)
            {
                return;
            }
            m_pObject = rhs.m_pCloner->Copy(rhs.m_pObject, nullptr, 0u);
            m_pInterface = static_cast<Interface*>((RhsInterface*)(m_pObject + ((char*)rhs.m_pInterface - rhs.m_pObject)));
            m_pCloner = rhs.m_pCloner;
        }

        template <class RhsInterface, size_t RhsSboSize>
        void InitMove_ImplicitCast(ClonePtr<RhsInterface, RhsSboSize>&& rhs)
        {
            if (!rhs.m_pInterface)
            {
                InitNull();
                return;
            }

            if (rhs.IsObjectInSboBuffer())
            {
                // we cannot steal the object pointer; the move-constructor must be invoked dynamically
                m_pObject = rhs.m_pCloner->Move(rhs.m_pObject, m_sbo, SboSize);
                m_pInterface = (RhsInterface*)(m_pObject + ((char*)rhs.m_pInterface - rhs.m_pObject));
                m_pCloner = rhs.m_pCloner;
                rhs.InitNull();
                return;
            }

            // steal the object pointer
            m_pInterface = rhs.m_pInterface;
            m_pObject = rhs.m_pObject;
            m_pCloner = rhs.m_pCloner;
            rhs.InitNull();
        }
        template <class RhsInterface, size_t RhsSboSize>
        void InitMove_StaticCast(ClonePtr<RhsInterface, RhsSboSize>&& rhs)
        {
            if (!rhs.m_pInterface)
            {
                InitNull();
                return;
            }

            if (rhs.IsObjectInSboBuffer())
            {
                // we cannot steal the object pointer; the move-constructor must be invoked dynamically
                m_pObject = rhs.m_pCloner->Move(rhs.m_pObject, m_sbo, SboSize);
                m_pInterface = (RhsInterface*)(m_pObject + ((char*)rhs.m_pInterface - rhs.m_pObject));
                m_pCloner = rhs.m_pCloner;
                rhs.InitNull();
                return;
            }

            // steal the object pointer
            m_pInterface = static_cast<Interface*>(rhs.m_pInterface);
            m_pObject = rhs.m_pObject;
            m_pCloner = rhs.m_pCloner;
            rhs.InitNull();
        }

        // Takes an Object that implements Interface, allocates a second instance of
        // that Object, and either copy or move constructs into the second instance.
        template <class Object, class CastToInterface>
        void AssignObjectValue(Object&& obj, CastToInterface&& castToInterface)
        {
            InitNull(); // reset members here, in case the constructor throws
            typedef typename std::decay<Object>::type Obj;
            Obj* pObject = new Obj(std::forward<Object>(obj));
            m_pInterface = castToInterface(pObject);
            m_pObject = (char*)pObject;
            m_pCloner = &ClonePtrCloner<Obj>::Instance;
        }

        bool IsObjectInSboBuffer() const
        {
            return false;
        }

    public:
        ~ClonePtr() CI0_NOEXCEPT(true)
        {
            Release();
        }

        ClonePtr() CI0_NOEXCEPT(true)
        {
            InitNull();
        }
        ClonePtr(nullptr_t) CI0_NOEXCEPT(true)
        {
            InitNull();
        }

        ClonePtr(const This& rhs)
        {
            InitCopy_ImplicitCast(rhs);
        }
        ClonePtr(const This&& rhs)
        {
            InitCopy_ImplicitCast(rhs);
        }
        ClonePtr(This& rhs)
        {
            InitCopy_ImplicitCast(rhs);
        }
        ClonePtr(This&& rhs) CI0_NOEXCEPT(true)
        {
            InitMove_ImplicitCast(std::move(rhs));
        }
        template <class RhsInterface, size_t RhsSboSize>
        ClonePtr(const ClonePtr<RhsInterface, RhsSboSize>& rhs)
        {
            InitCopy_ImplicitCast(rhs);
        }
        template <class RhsInterface, size_t RhsSboSize>
        ClonePtr(const ClonePtr<RhsInterface, RhsSboSize>&& rhs)
        {
            InitCopy_ImplicitCast(rhs);
        }
        template <class RhsInterface, size_t RhsSboSize>
        ClonePtr(ClonePtr<RhsInterface, RhsSboSize>& rhs)
        {
            InitCopy_ImplicitCast(rhs);
        }
        template <class RhsInterface, size_t RhsSboSize>
        ClonePtr(ClonePtr<RhsInterface, RhsSboSize>&& rhs) CI0_NOEXCEPT(true)
        {
            InitMove_ImplicitCast(std::move(rhs));
        }

        // Copy or move a concrete object in.
        template <class Object>
        explicit ClonePtr(Object&& obj)
        {
            AssignObjectValue(std::forward<Object>(obj), CastImplicit<Object, Interface>());
        }

        This& operator=(const This& rhs)
        {
            if (this != &rhs)
            {
                This other(rhs);
                Release();
                InitMove_ImplicitCast(std::move(other));
            }
            return *this;
        }
        This& operator=(const This&& rhs)
        {
            if (this != &rhs)
            {
                This other(rhs);
                Release();
                InitMove_ImplicitCast(std::move(other));
            }
            return *this;
        }
        This& operator=(This& rhs)
        {
            if (this != &rhs)
            {
                This other(rhs);
                Release();
                InitMove_ImplicitCast(std::move(other));
            }
            return *this;
        }
        This& operator=(This&& rhs) CI0_NOEXCEPT(true)
        {
            if (this != &rhs)
            {
                Release();
                InitMove_ImplicitCast(std::move(rhs));
            }
            return *this;
        }
        template <class RhsInterface, size_t RhsSboSize>
        This& operator=(const ClonePtr<RhsInterface, RhsSboSize>& rhs)
        {
            This other(rhs);
            Release();
            InitMove_ImplicitCast(std::move(other));
            return *this;
        }
        template <class RhsInterface, size_t RhsSboSize>
        This& operator=(const ClonePtr<RhsInterface, RhsSboSize>&& rhs)
        {
            This other(rhs);
            Release();
            InitMove_ImplicitCast(std::move(other));
            return *this;
        }
        template <class RhsInterface, size_t RhsSboSize>
        This& operator=(ClonePtr<RhsInterface, RhsSboSize>& rhs)
        {
            This other(rhs);
            Release();
            InitMove_ImplicitCast(std::move(other));
            return *this;
        }
        template <class RhsInterface, size_t RhsSboSize>
        This& operator=(ClonePtr<RhsInterface, RhsSboSize>&& rhs) CI0_NOEXCEPT(true)
        {
            Release();
            InitMove_ImplicitCast(std::move(rhs));
            return *this;
        }
        This& operator=(nullptr_t) CI0_NOEXCEPT(true)
        {
            Release();
            InitNull();
            return *this;
        }

        // Copy or move a concrete object in.
        template <class Object>
        This& assign(Object&& obj)
        {
            Release();
            AssignObjectValue(std::forward<Object>(obj), CastImplicit<Object, Interface>());
            return *this;
        }

        // Copy or move a concrete object in.
        // The CastToInterface() function-object argument can be used to disambiguate
        //      struct Base1 : CommonBase {};
        //      struct Base2 : CommonBase {};
        //      struct Thing : Base1, Base2 {};
        //      ClonePtr<CommonBase> pCommonBase;
        //      // error: ambiguous, because CommonBase reachable through Base1 and Base2
        //      pCommonBase.Assign( Thing() );
        //      // OK, because CommonBase unambiguously reachable from Base
        //      pCommonBase.Assign( Thing(), [](Thing* pThing) { return (Base1*)pThing; })
        template <class Object, class CastToInterface>
        This& assign(Object&& obj, CastToInterface&& castToInterface)
        {
            Release();
            AssignObjectValue(std::forward<Object>(obj), std::forward<CastToInterface>(castToInterface));
            return *this;
        }

        // Takes ownership of pObject.  Allows initialization without copying.
        //      ClonePtr<Base> pBase = ClonePtr<Base>().attach(new Derived(...));
        template <class Object>
        This& attach(Object* pObject) CI0_NOEXCEPT(true)
        {
            Release();
            m_pObject = (char*)pObject;
            m_pInterface = pObject;
            m_pCloner = &ClonePtrCloner<Object>::Instance;
            return *this;
        }
        // Relinquishes ownership of the object.
        // note: make sure Interface has a virtual destructor, or that Result is the concrete leaf type
        template <class Result = Interface>
        Result* detach() CI0_NOEXCEPT(true)
        {
            Result* pResult = static_cast<Result*>(m_pInterface);
            InitNull();
            return pResult;
        }

        This& swap(This& rhs) CI0_NOEXCEPT(true)
        {
            std::swap(*this, rhs);
            return *this;
        }
        This& swap(This&& rhs) CI0_NOEXCEPT(true)
        {
            std::swap(*this, rhs);
            return *this;
        }

        This& reset() CI0_NOEXCEPT(true)
        {
            Release();
            InitNull();
            return *this;
        }
        // note: present for STL/boost compatibility, but you should prefer to call attach() instead
        template <class Object>
        This& reset(Object* pObject) CI0_NOEXCEPT(true)
        {
            return attach(pObject);
        }

        template <class RhsInterface, size_t RhsSboSize = sizeof(void*)>
        ClonePtr<RhsInterface, RhsSboSize> copy_as()
        {
            ClonePtr<RhsInterface, RhsSboSize> pOther;
            pOther.InitCopy_StaticCast(*this);
            return pOther;
        }
        template <class RhsInterface, size_t RhsSboSize = sizeof(void*)>
        ClonePtr<RhsInterface, RhsSboSize> move_as() CI0_NOEXCEPT(true)
        {
            ClonePtr<RhsInterface, RhsSboSize> pOther;
            pOther.InitMove_StaticCast(*this);
            return pOther;
        }

        Interface* const& get() const CI0_NOEXCEPT(true)
        {
            return m_pInterface;
        }

        Interface& operator*() const CI0_NOEXCEPT(true)
        {
            return *m_pInterface;
        }
        Interface* operator->() const CI0_NOEXCEPT(true)
        {
            return m_pInterface;
        }

        operator Interface*() const CI0_NOEXCEPT(true)
        {
            return m_pInterface;
        }
        explicit operator bool() const CI0_NOEXCEPT(true)
        {
            return !!m_pInterface;
        }
        template <class Type>
        explicit operator Type*() const CI0_NOEXCEPT(true)
        {
            return static_cast<Type*>(m_pInterface);
        }
    };

    template <class LhsObject, size_t LhsSboSize, class RhsObject, size_t RhsSboSize>
    inline bool operator==(const ClonePtr<LhsObject, LhsSboSize>& lhs, const ClonePtr<RhsObject, RhsSboSize>& rhs)
    {
        return lhs.get() == rhs.get();
    }
    template <class LhsObject, size_t LhsSboSize, class RhsObject, size_t RhsSboSize>
    inline bool operator!=(const ClonePtr<LhsObject, LhsSboSize>& lhs, const ClonePtr<RhsObject, RhsSboSize>& rhs)
    {
        return lhs.get() != rhs.get();
    }
    template <class LhsObject, size_t LhsSboSize, class RhsObject, size_t RhsSboSize>
    inline bool operator>=(const ClonePtr<LhsObject, LhsSboSize>& lhs, const ClonePtr<RhsObject, RhsSboSize>& rhs)
    {
        return lhs.get() >= rhs.get();
    }
    template <class LhsObject, size_t LhsSboSize, class RhsObject, size_t RhsSboSize>
    inline bool operator<=(const ClonePtr<LhsObject, LhsSboSize>& lhs, const ClonePtr<RhsObject, RhsSboSize>& rhs)
    {
        return lhs.get() <= rhs.get();
    }
    template <class LhsObject, size_t LhsSboSize, class RhsObject, size_t RhsSboSize>
    inline bool operator>(const ClonePtr<LhsObject, LhsSboSize>& lhs, const ClonePtr<RhsObject, RhsSboSize>& rhs)
    {
        return lhs.get() > rhs.get();
    }
    template <class LhsObject, size_t LhsSboSize, class RhsObject, size_t RhsSboSize>
    inline bool operator<(const ClonePtr<LhsObject, LhsSboSize>& lhs, const ClonePtr<RhsObject, RhsSboSize>& rhs)
    {
        return lhs.get() < rhs.get();
    }

    template <class Object, size_t SboSize>
    inline bool operator==(const ClonePtr<Object, SboSize>& lhs, std::nullptr_t)
    {
        return lhs.get() == nullptr;
    }
    template <class Object, size_t SboSize>
    inline bool operator==(std::nullptr_t, const ClonePtr<Object, SboSize>& rhs)
    {
        return nullptr == rhs.get();
    }
    template <class Object, size_t SboSize>
    inline bool operator!=(const ClonePtr<Object, SboSize>& lhs, std::nullptr_t)
    {
        return lhs.get() != nullptr;
    }
    template <class Object, size_t SboSize>
    inline bool operator!=(std::nullptr_t, const ClonePtr<Object, SboSize>& rhs)
    {
        return nullptr != rhs.get();
    }

    template <class Object, size_t SboSize>
    void swap(ClonePtr<Object, SboSize>& lhs, ClonePtr<Object, SboSize>& rhs)
    {
        lhs.swap(rhs);
    }
}

#if _MSC_VER
#pragma warning(pop)
#endif
