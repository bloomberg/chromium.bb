//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++98, c++03

// <experimental/simd>
//
// [simd.traits]
// template <class T> struct is_simd;
// template <class T> inline constexpr bool is_simd_v = is_simd<T>::value;

#include <cstdint>
#include <experimental/simd>
#include "test_macros.h"

using namespace std::experimental::parallelism_v2;

struct UserType {};

static_assert( is_simd<native_simd<int8_t>>::value, "");
static_assert( is_simd<native_simd<int16_t>>::value, "");
static_assert( is_simd<native_simd<int32_t>>::value, "");
static_assert( is_simd<native_simd<int64_t>>::value, "");
static_assert( is_simd<native_simd<uint8_t>>::value, "");
static_assert( is_simd<native_simd<uint16_t>>::value, "");
static_assert( is_simd<native_simd<uint32_t>>::value, "");
static_assert( is_simd<native_simd<uint64_t>>::value, "");
static_assert( is_simd<native_simd<float>>::value, "");
static_assert( is_simd<native_simd<double>>::value, "");

static_assert( is_simd<fixed_size_simd<int8_t, 1>>::value, "");
static_assert( is_simd<fixed_size_simd<int16_t, 1>>::value, "");
static_assert( is_simd<fixed_size_simd<int32_t, 1>>::value, "");
static_assert( is_simd<fixed_size_simd<int64_t, 1>>::value, "");
static_assert( is_simd<fixed_size_simd<uint8_t, 1>>::value, "");
static_assert( is_simd<fixed_size_simd<uint16_t, 1>>::value, "");
static_assert( is_simd<fixed_size_simd<uint32_t, 1>>::value, "");
static_assert( is_simd<fixed_size_simd<uint64_t, 1>>::value, "");
static_assert( is_simd<fixed_size_simd<float, 1>>::value, "");
static_assert( is_simd<fixed_size_simd<double, 1>>::value, "");

static_assert( is_simd<fixed_size_simd<int8_t, 3>>::value, "");
static_assert( is_simd<fixed_size_simd<int16_t, 3>>::value, "");
static_assert( is_simd<fixed_size_simd<int32_t, 3>>::value, "");
static_assert( is_simd<fixed_size_simd<int64_t, 3>>::value, "");
static_assert( is_simd<fixed_size_simd<uint8_t, 3>>::value, "");
static_assert( is_simd<fixed_size_simd<uint16_t, 3>>::value, "");
static_assert( is_simd<fixed_size_simd<uint32_t, 3>>::value, "");
static_assert( is_simd<fixed_size_simd<uint64_t, 3>>::value, "");
static_assert( is_simd<fixed_size_simd<float, 3>>::value, "");
static_assert( is_simd<fixed_size_simd<double, 3>>::value, "");

static_assert( is_simd<fixed_size_simd<int8_t, 32>>::value, "");
static_assert( is_simd<fixed_size_simd<int16_t, 32>>::value, "");
static_assert( is_simd<fixed_size_simd<int32_t, 32>>::value, "");
static_assert( is_simd<fixed_size_simd<int64_t, 32>>::value, "");
static_assert( is_simd<fixed_size_simd<uint8_t, 32>>::value, "");
static_assert( is_simd<fixed_size_simd<uint16_t, 32>>::value, "");
static_assert( is_simd<fixed_size_simd<uint32_t, 32>>::value, "");
static_assert( is_simd<fixed_size_simd<uint64_t, 32>>::value, "");
static_assert( is_simd<fixed_size_simd<float, 32>>::value, "");
static_assert( is_simd<fixed_size_simd<double, 32>>::value, "");

static_assert(!is_simd<void>::value, "");
static_assert(!is_simd<int>::value, "");
static_assert(!is_simd<float>::value, "");
static_assert(!is_simd<simd_mask<int>>::value, "");
static_assert(!is_simd<simd_mask<float>>::value, "");
static_assert(!is_simd<UserType>::value, "");

#if TEST_STD_VER > 14 && !defined(_LIBCPP_HAS_NO_VARIABLE_TEMPLATES) &&        \
    !defined(_LIBCPP_HAS_NO_INLINE_VARIABLES)

static_assert( is_simd_v<native_simd<int8_t>>, "");
static_assert( is_simd_v<native_simd<int16_t>>, "");
static_assert( is_simd_v<native_simd<int32_t>>, "");
static_assert( is_simd_v<native_simd<int64_t>>, "");
static_assert( is_simd_v<native_simd<uint8_t>>, "");
static_assert( is_simd_v<native_simd<uint16_t>>, "");
static_assert( is_simd_v<native_simd<uint32_t>>, "");
static_assert( is_simd_v<native_simd<uint64_t>>, "");
static_assert( is_simd_v<native_simd<float>>, "");
static_assert( is_simd_v<native_simd<double>>, "");

static_assert( is_simd_v<fixed_size_simd<int8_t, 1>>, "");
static_assert( is_simd_v<fixed_size_simd<int16_t, 1>>, "");
static_assert( is_simd_v<fixed_size_simd<int32_t, 1>>, "");
static_assert( is_simd_v<fixed_size_simd<int64_t, 1>>, "");
static_assert( is_simd_v<fixed_size_simd<uint8_t, 1>>, "");
static_assert( is_simd_v<fixed_size_simd<uint16_t, 1>>, "");
static_assert( is_simd_v<fixed_size_simd<uint32_t, 1>>, "");
static_assert( is_simd_v<fixed_size_simd<uint64_t, 1>>, "");
static_assert( is_simd_v<fixed_size_simd<float, 1>>, "");
static_assert( is_simd_v<fixed_size_simd<double, 1>>, "");

static_assert( is_simd_v<fixed_size_simd<int8_t, 3>>, "");
static_assert( is_simd_v<fixed_size_simd<int16_t, 3>>, "");
static_assert( is_simd_v<fixed_size_simd<int32_t, 3>>, "");
static_assert( is_simd_v<fixed_size_simd<int64_t, 3>>, "");
static_assert( is_simd_v<fixed_size_simd<uint8_t, 3>>, "");
static_assert( is_simd_v<fixed_size_simd<uint16_t, 3>>, "");
static_assert( is_simd_v<fixed_size_simd<uint32_t, 3>>, "");
static_assert( is_simd_v<fixed_size_simd<uint64_t, 3>>, "");
static_assert( is_simd_v<fixed_size_simd<float, 3>>, "");
static_assert( is_simd_v<fixed_size_simd<double, 3>>, "");

static_assert( is_simd_v<fixed_size_simd<int8_t, 32>>, "");
static_assert( is_simd_v<fixed_size_simd<int16_t, 32>>, "");
static_assert( is_simd_v<fixed_size_simd<int32_t, 32>>, "");
static_assert( is_simd_v<fixed_size_simd<int64_t, 32>>, "");
static_assert( is_simd_v<fixed_size_simd<uint8_t, 32>>, "");
static_assert( is_simd_v<fixed_size_simd<uint16_t, 32>>, "");
static_assert( is_simd_v<fixed_size_simd<uint32_t, 32>>, "");
static_assert( is_simd_v<fixed_size_simd<uint64_t, 32>>, "");
static_assert( is_simd_v<fixed_size_simd<float, 32>>, "");
static_assert( is_simd_v<fixed_size_simd<double, 32>>, "");

static_assert(!is_simd_v<void>, "");
static_assert(!is_simd_v<int>, "");
static_assert(!is_simd_v<float>, "");
static_assert(!is_simd_v<simd_mask<int>>, "");
static_assert(!is_simd_v<simd_mask<float>>, "");
static_assert(!is_simd_v<UserType>, "");

#endif

int main() {}
