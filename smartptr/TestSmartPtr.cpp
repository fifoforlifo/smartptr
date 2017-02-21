#include "UniquePtr.h"
#include "ClonePtr.h"
#include "IntrusivePtr.h"
#include <stdio.h>
#include <utility>

#define ENABLE_MISUSE 0

void UseIntPtr(int* pInt)
{
    printf("UseIntPtr(%d)\n", *pInt);
}

void OutputIntPtr(int** ppInt)
{
    printf("OutputIntPtr old=%d\n", **ppInt);
    *ppInt = new int(5);
}
void ClearIntPtr(int** ppInt)
{
    *ppInt = nullptr;
}

struct EarlierBase
{
    virtual ~EarlierBase() {}
};
struct Base
{
    virtual ~Base() {}
    int foo;
};
struct Derived : EarlierBase, Base
{
    int bar;

    Derived(int foo_, int bar_) { foo = foo_; bar = bar_; }
};

void UseBase(Base* pBase)
{
    printf("UseBase {%d}\n", pBase->foo);
}
void UseDerived(Derived* pDerived)
{
    printf("UseDerived {%d, %d}\n", pDerived->foo, pDerived->bar);
}

void TestUniquePtr()
{
    {
        ci0::UniquePtr<int> pInt(new int(3));
        if (pInt)
        {
            UseIntPtr(pInt);
        }
        OutputIntPtr(pInt.Out());
        pInt.Swap(ci0::UniquePtr<int>(new int(6)));
        ClearIntPtr(pInt.Out());
        if (!pInt)
        {
            printf("cleared\n");
        }

#if ENABLE_MISUSE
        delete pInt;    // misuse causes compile error: 'delete': cannot convert from 'ci0::UniquePtr<int,void ci0::GlobalDeleteObject<Object>(Object *)>' to 'void*'
#endif
    }
    {
        ci0::UniquePtr<Derived> pDerived(new Derived(2, 3));
        ci0::UniquePtr<Base> pBase = pDerived.MoveAs<Base>();
    }
}

void TestClonePtr()
{
    {
        Derived der(3, 4);
        ci0::ClonePtr<Base> pBase1(der);
        ci0::ClonePtr<Base> pBase2 = pBase1;
        ci0::ClonePtr<Base> pBase3 = std::move(pBase2);
        ci0::ClonePtr<Base> pBase4(der);
        pBase2 = pBase4;
        pBase2 = std::move(pBase4);

        UseBase(pBase2);
        UseDerived((Derived*)pBase2);

#if ENABLE_MISUSE
        delete pBase2;
#endif

#if ENABLE_MISUSE
        ci0::ClonePtr<int> pInt(3);
        UseDerived((Derived*)pInt);   // misuse causes compile error: cannot convert from 'int *const ' to 'Derived *'
#endif
    }
    {
        typedef ci0::ClonePtr<Base, 16> BasePtr16;
        Derived der(3, 4);
        ci0::ClonePtr<Base> pBase1(der);
        BasePtr16 pBase2 = pBase1;
        ci0::ClonePtr<Base> pBase3 = std::move(pBase2);
        ci0::ClonePtr<Base> pBase4(der);
        pBase2 = pBase4;
        pBase2 = std::move(pBase4);
        pBase3 = pBase2;

        UseBase(pBase2);
        UseDerived((Derived*)pBase2);
    }
}


struct RcBase
{
    int refcount;
    int foo;

    virtual ~RcBase()
    {
    }
    RcBase(int foo_)
        : refcount(1)
        , foo(foo_)
    {
    }
};
struct RcDerived : RcBase
{
    int bar;

    ~RcDerived()
    {
        printf("%s\n", __FUNCTION__);
    }
    RcDerived(int foo_, int bar_)
        : RcBase(foo_)
        , bar(bar_)
    {
        printf("%s\n", __FUNCTION__);
    }
};

void intrusive_ptr_add_ref(RcBase* pBase)
{
    pBase->refcount += 1;
    printf("%s refcount=%d\n", __FUNCTION__, pBase->refcount);
}
int intrusive_ptr_release(RcBase* pBase)
{
    printf("%s refcount=%d\n", __FUNCTION__, pBase->refcount);
    int refcount = (pBase->refcount -= 1);
    if (!refcount)
    {
        delete pBase;
    }
    return refcount;
}

void UseRcBase(RcBase* pBase)
{
    printf("UseRcBase foo=%d\n", pBase->foo);
}
void CreateRcDerived(RcBase** ppBase)
{
    *ppBase = new RcDerived(6, 7);
}

void TestIntrusivePtr()
{
    {
        typedef ci0::IntrusivePtr<RcBase> RcBasePtr;

        RcBasePtr pBase(new RcDerived(3, 4), false);
        RcBasePtr pBase2 = pBase;
        RcBasePtr pBase3 = std::move(pBase2);
        if (!pBase2)
        {
            printf("pBase2 cleared\n");
        }
        if (pBase3)
        {
            printf("pBase3 set\n");
        }
        CreateRcDerived(pBase3.Out());
        pBase2 = nullptr;
        pBase = pBase3;
        pBase3 = nullptr;
        pBase = nullptr;
        if (!pBase)
        {
            printf("cleared\n");
        }

        pBase.Attach(new RcDerived(1, 2), false);
        ci0::IntrusivePtr<RcDerived> pDerived = new RcDerived(4, 3);
        pBase = pDerived;
        pDerived = (RcDerived*)(RcBase*)pBase;

#if ENABLE_MISUSE
        delete pBase;   // misuse causes compile error: 'delete': cannot convert from 'ci0::UniquePtr<int,void ci0::GlobalDeleteObject<Object>(Object *)>' to 'void*'
#endif
    }
}


int main()
{
    TestUniquePtr();
    TestClonePtr();
    TestIntrusivePtr();
    return 0;
}