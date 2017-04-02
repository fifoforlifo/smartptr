#include "UniquePtr.h"
#include "ClonePtr.h"
#include "IntrusivePtr.h"
#include "Function.h"
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
        OutputIntPtr(pInt.out());
        pInt.swap(ci0::UniquePtr<int>(new int(6)));
        ClearIntPtr(pInt.out());
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
        ci0::UniquePtr<Base> pBase = pDerived.move_as<Base>();
        Derived* pDerived2 = (Derived*)pBase;
        pBase.attach(new Derived(4, 5));
        ci0::UniquePtr<Base> pBase2(std::move(pDerived));
        pBase = std::move(pBase2);
        pBase2 = std::move(pDerived);
        pBase2 = ci0::MakeUnique<Derived>(6, 7);

        bool testComparisons = false;
        testComparisons = (pBase == pBase2);
        testComparisons = (pBase != pBase2);
        testComparisons = (pBase <= pBase2);
        testComparisons = (pBase >= pBase2);
        testComparisons = (pBase <  pBase2);
        testComparisons = (pBase >  pBase2);
        testComparisons = (pBase == (Base*)pBase2); // implicit cast handled this
        testComparisons = (pBase != (Base*)pBase2); // implicit cast handled this
        testComparisons = (pBase <= (Base*)pBase2); // implicit cast handled this
        testComparisons = (pBase >= (Base*)pBase2); // implicit cast handled this
        testComparisons = (pBase <  (Base*)pBase2); // implicit cast handled this
        testComparisons = (pBase >  (Base*)pBase2); // implicit cast handled this
        testComparisons = (pBase == nullptr);
        testComparisons = (nullptr == pBase);
        testComparisons = (pBase != nullptr);
        testComparisons = (nullptr != pBase);

#if ENABLE_MISUSE
        pDerived = pDerived2;       // misuse causes compile error: binary '=': no operator found which takes a right-hand operand of type 'Derived *' (or there is no acceptable conversion)
        pDerived = ci0::UniquePtr<Derived>(pDerived);       // misuse causes compile error: 'ci0::UniquePtr<Derived,void ci0::DeleteObjectWithGlobalDelete<Object>(Object *)>::UniquePtr': cannot access private member declared in class 'ci0::UniquePtr<Derived,void ci0::DeleteObjectWithGlobalDelete<Object>(Object *)>'
#endif
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
        ci0::ClonePtr<Base, 0> pBase4(der);
        pBase2 = pBase4;
        pBase2 = std::move(pBase4);
        pBase3 = pBase2;

        UseBase(pBase2);
        UseDerived((Derived*)pBase2);

        pBase2 = nullptr;
        pBase2.attach(new Derived(7, 8));

        bool testComparisons = false;
        testComparisons = (pBase1 == pBase2);
        testComparisons = (pBase1 != pBase2);
        testComparisons = (pBase1 <= pBase2);
        testComparisons = (pBase1 >= pBase2);
        testComparisons = (pBase1 <  pBase2);
        testComparisons = (pBase1 >  pBase2);
        testComparisons = (pBase1 == (Base*)pBase2); // implicit cast handled this
        testComparisons = (pBase1 != (Base*)pBase2); // implicit cast handled this
        testComparisons = (pBase1 <= (Base*)pBase2); // implicit cast handled this
        testComparisons = (pBase1 >= (Base*)pBase2); // implicit cast handled this
        testComparisons = (pBase1 <  (Base*)pBase2); // implicit cast handled this
        testComparisons = (pBase1 >  (Base*)pBase2); // implicit cast handled this
        testComparisons = (pBase1 == nullptr);
        testComparisons = (nullptr == pBase1);
        testComparisons = (pBase1 != nullptr);
        testComparisons = (nullptr != pBase1);
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
        printf("%s %p\n", __FUNCTION__, this);
    }
    RcDerived(int foo_, int bar_)
        : RcBase(foo_)
        , bar(bar_)
    {
        printf("%s %p\n", __FUNCTION__, this);
    }
};

void intrusive_ptr_add_ref(RcBase* pBase)
{
    pBase->refcount += 1;
    printf("%s %p refcount=%d\n", __FUNCTION__, pBase, pBase->refcount);
}
int intrusive_ptr_release(RcBase* pBase)
{
    printf("%s %p refcount=%d\n", __FUNCTION__, pBase, pBase->refcount);
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
        CreateRcDerived(pBase3.out());
        pBase2 = nullptr;
        pBase = pBase3;
        pBase3 = nullptr;
        pBase = nullptr;
        if (!pBase)
        {
            printf("cleared\n");
        }

        pBase.attach(new RcDerived(1, 2), false);
        ci0::IntrusivePtr<RcDerived> pDerived(new RcDerived(4, 3), false);
        pBase = pDerived;
        pDerived = (RcDerived*)pBase;
        pDerived.attach(new RcDerived(5, 6), false);

        bool testComparisons = false;
        testComparisons = (pBase == pBase2);
        testComparisons = (pBase != pBase2);
        testComparisons = (pBase <= pBase2);
        testComparisons = (pBase >= pBase2);
        testComparisons = (pBase <  pBase2);
        testComparisons = (pBase >  pBase2);
        testComparisons = (pBase == (RcBase*)pBase2); // implicit cast handled this
        testComparisons = (pBase != (RcBase*)pBase2); // implicit cast handled this
        testComparisons = (pBase <= (RcBase*)pBase2); // implicit cast handled this
        testComparisons = (pBase >= (RcBase*)pBase2); // implicit cast handled this
        testComparisons = (pBase <  (RcBase*)pBase2); // implicit cast handled this
        testComparisons = (pBase >  (RcBase*)pBase2); // implicit cast handled this
        testComparisons = (pBase == nullptr);
        testComparisons = (nullptr == pBase);
        testComparisons = (pBase != nullptr);
        testComparisons = (nullptr != pBase);

#if ENABLE_MISUSE
        delete pBase;   // misuse causes compile error: 'delete': cannot convert from 'ci0::UniquePtr<int,void ci0::GlobalDeleteObject<Object>(Object *)>' to 'void*'
#endif
    }
}

template <class Foo>
void ForceNoOpt(int argc, Foo&& foo)
{
    if (argc > 10)
    {
        foo = nullptr;
    }
}

void TestFuncRef(int argc)
{
    {
        typedef ci0::FuncRef<int(int, int)> CombineFnRef;

        CombineFnRef combineFnA = [](int x, int y) { return x + y; };
        printf("%d = combineFnA(%d, %d)\n", combineFnA(1, 2), 1, 2);
        CombineFnRef combineFnB;
        combineFnB = combineFnA;
        printf("%d = combineFnB(%d, %d)\n", combineFnB(1, 2), 1, 2);
        int z = 3;
        auto funcC = [&](int x, int y) { return x + y + z; };
        CombineFnRef combineFnC = funcC;
        ForceNoOpt(argc, combineFnC);
        printf("%d = combineFnC(%d, %d)\n", combineFnC(1, 2), 1, 2);
#if ENABLE_MISUSE
        combineFnC = [&](int x, int y) { return x + y + z + 1; };
        printf("%d = combineFnC(%d, %d)\n", combineFnC(1, 2), 1, 2);
#endif
    }
}

void TestFunction(int argc)
{
    {
        typedef ci0::Function<int(int, int)> CombineFnRef;

        CombineFnRef combineFnA =
            [](int x, int y)
            {
                return x + y;
            };
        printf("%d = combineFnA(%d, %d)\n", combineFnA(1, 2), 1, 2);
        CombineFnRef combineFnB;
        combineFnB = combineFnA;
        printf("%d = combineFnB(%d, %d)\n", combineFnB(1, 2), 1, 2);
        int z = 3;
        auto funcC = [&](int x, int y) { return x + y + z; };
        CombineFnRef combineFnC = funcC;
        ForceNoOpt(argc, combineFnC);
        printf("%d = combineFnC(%d, %d)\n", combineFnC(1, 2), 1, 2);
#if ENABLE_MISUSE
        combineFnC = [&](int x, int y) { return x + y + z + 1; };
        printf("%d = combineFnC(%d, %d)\n", combineFnC(1, 2), 1, 2);
#endif
    }
}


int main(int argc, char** argv)
{
    TestUniquePtr();
    TestClonePtr();
    TestIntrusivePtr();
    TestFuncRef(argc);
    TestFunction(argc);
    return 0;
}
