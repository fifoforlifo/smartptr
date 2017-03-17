#pragma once

#if defined(_MSC_VER) && _MSC_VER < 1900
#define CI0_NOEXCEPT(expr_)
#else
#define CI0_NOEXCEPT(expr_) noexcept(expr_)
#endif
