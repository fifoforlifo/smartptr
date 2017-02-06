#include "UniquePtr.h"
#include "ClonePtr.h"
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

struct Base
{
    int x;
};
struct Derived : Base
{
    int y;

    Derived(int x_, int y_) { x = x_; y = y_; }
};

void UseBase(Base* pBase)
{
    printf("UseBase {%d}\n", pBase->x);
}
void UseDerived(Derived* pDerived)
{
    printf("UseDerived {%d, %d}\n", pDerived->x, pDerived->y);
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
}

int main()
{
    TestUniquePtr();
    TestClonePtr();
    return 0;
}