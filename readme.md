# smartptr : Usable Smart Pointers in Modern C++

Modern C++ lacks smart pointers.  There are no types in the standard library,
where you can replace the declaration of a pointer variable, and still have
most of your code compile.

This library aims to fix that.

Feel free to copy & tailor these smart pointers however you wish.


## FAQ

### What about unique_ptr and shared_ptr?

These are not smart pointers, because they do not implement the implicit cast operator.
They should be called "lifetime managers" or "holders".

Without an implicit cast operator, you must sprinkle `.get()` calls everywhere, which
pollutes a codebase and makes otherwise cheap edits expensive.


### But all these experts say implicit cast operators are evil!!

If you put implicit cast operators on ordinary workhorse data types, then
the general consensus is that's evil.  (and I would agree)

But to extend that logic to smart pointers?  Superstition and nonsense.

(Any counter-arguments against this will invariably involve esoteric
examples that never appear in ordinary code.)


### But implicit cast operators allow you to accidentally `delete` twice!

Actually no, they don't.  See here: http://stackoverflow.com/a/3312507

All credit to Johannes Schaub (known as "litb" on SO)
who figured out how to prevent this type of misuse.


### But out-params on UniquePtr require different syntax!  Hypocrite!

Yep, you got me.  For terse syntax you must write `CreateFoo(pFoo.Out())`
rather than the pointer-fungible `CreateFoo(&pFoo)`.  Or you must write
a more long-winded code sequence:

```cpp
IFoo* pFooRaw = nullptr;
CreateFoo(&pFooRaw);
pFoo.Attach(pFooRaw);
```

Why?  It's too error-prone for a lifetime management class
to give out its member variable freely, so I didn't override `operator&`.

Double-pointer out-params only tend to occur in C APIs, from factory functions.
These tend to be low-frequency occurrences, whereas passing an object pointer
as function argument is high-frequency.  But yes, this is equivocating --
ultimately I'm just drawing the line somewhere.

Note that Microsoft's `CComPtr` does implement `operator&`, but the only
safeguard against misuse is a runtime `assert(!p)`.


### If smart pointers were supposed to work like this, they'd be in the standard library already!

Of course!  That's why nobody uses the following libraries:

* [boost](http://boost.org)
* [Unreal Engine](https://docs.unrealengine.com/latest/INT/Programming/UnrealArchitecture/SmartPointerLibrary/index.html)
* [EASTL](https://github.com/electronicarts/EASTL)
