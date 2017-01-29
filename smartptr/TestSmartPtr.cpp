#include "UniquePtr.h"
#include <stdio.h>

using namespace neu;

void UseIntPtr(int* pInt)
{
    printf("UseIntPtr(%d)\n", *pInt);
}

void OutputIntPtr(int** ppInt)
{
    printf("OutputIntPtr old=%d\n", **ppInt);
    *ppInt = new int(5);
}

void TestUniquePtr()
{
    {
        UniquePtr<int> pInt(new int(3));
        if (pInt)
        {
            UseIntPtr(pInt);
        }
        OutputIntPtr(&pInt);
    }
}

int main()
{
    TestUniquePtr();
    return 0;
}