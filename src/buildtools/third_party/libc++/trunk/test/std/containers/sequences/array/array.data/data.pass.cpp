//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <array>

// T *data();

#include <array>
#include <cassert>
#include "test_macros.h"

// std::array is explicitly allowed to be initialized with A a = { init-list };.
// Disable the missing braces warning for this reason.
#include "disable_missing_braces_warning.h"

int main()
{
    {
        typedef double T;
        typedef std::array<T, 3> C;
        C c = {1, 2, 3.5};
        T* p = c.data();
        assert(p[0] == 1);
        assert(p[1] == 2);
        assert(p[2] == 3.5);
    }
    {
        typedef double T;
        typedef std::array<T, 0> C;
        C c = {};
        T* p = c.data();
        assert(p != nullptr);
    }
    {
      typedef double T;
      typedef std::array<const T, 0> C;
      C c = {{}};
      const T* p = c.data();
      static_assert((std::is_same<decltype(c.data()), const T*>::value), "");
      assert(p != nullptr);
    }
  {
      typedef std::max_align_t T;
      typedef std::array<T, 0> C;
      const C c = {};
      const T* p = c.data();
      assert(p != nullptr);
      std::uintptr_t pint = reinterpret_cast<std::uintptr_t>(p);
      assert(pint % TEST_ALIGNOF(std::max_align_t) == 0);
    }
    {
      struct NoDefault {
        NoDefault(int) {}
      };
      typedef NoDefault T;
      typedef std::array<T, 0> C;
      C c = {};
      T* p = c.data();
      assert(p != nullptr);
    }
}
