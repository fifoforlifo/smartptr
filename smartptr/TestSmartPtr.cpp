#include "UniquePtr.h"
#include <stdio.h>
#include <utility>

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

void TestUniquePtr()
{
    {
        ci0::UniquePtr<int> pInt(new int(3));
        if (pInt)
        {
            UseIntPtr(pInt);
        }
        OutputIntPtr(pInt.Out());
        ClearIntPtr(pInt.Out());
        if (!pInt)
        {
            printf("cleared\n");
        }

        //delete pInt;
    }
}

int main()
{
    TestUniquePtr();
    return 0;
}