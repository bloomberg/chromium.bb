// Copyright 2020 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SRC_DAWN_COMMON_ITYP_ARRAY_H_
#define SRC_DAWN_COMMON_ITYP_ARRAY_H_

#include <array>
#include <cstddef>
#include <limits>
#include <type_traits>
#include <utility>

#include "dawn/common/TypedInteger.h"
#include "dawn/common/UnderlyingType.h"

namespace ityp {

// ityp::array is a helper class that wraps std::array with the restriction that
// indices must be a particular type |Index|. Dawn uses multiple flat maps of
// index-->data, and this class helps ensure an indices cannot be passed interchangably
// to a flat map of a different type.
template <typename Index, typename Value, size_t Size>
class array : private std::array<Value, Size> {
    using I = UnderlyingType<Index>;
    using Base = std::array<Value, Size>;

    static_assert(Size <= std::numeric_limits<I>::max());

  public:
    constexpr array() = default;

    template <typename... Values>
    // NOLINTNEXTLINE(runtime/explicit)
    constexpr array(Values&&... values) : Base{std::forward<Values>(values)...} {}

    Value& operator[](Index i) {
        I index = static_cast<I>(i);
        ASSERT(index >= 0 && index < I(Size));
        return Base::operator[](index);
    }

    constexpr const Value& operator[](Index i) const {
        I index = static_cast<I>(i);
        ASSERT(index >= 0 && index < I(Size));
        return Base::operator[](index);
    }

    Value& at(Index i) {
        I index = static_cast<I>(i);
        ASSERT(index >= 0 && index < I(Size));
        return Base::at(index);
    }

    constexpr const Value& at(Index i) const {
        I index = static_cast<I>(i);
        ASSERT(index >= 0 && index < I(Size));
        return Base::at(index);
    }

    typename Base::iterator begin() noexcept { return Base::begin(); }

    typename Base::const_iterator begin() const noexcept { return Base::begin(); }

    typename Base::iterator end() noexcept { return Base::end(); }

    typename Base::const_iterator end() const noexcept { return Base::end(); }

    constexpr Index size() const { return Index(I(Size)); }

    using Base::back;
    using Base::data;
    using Base::empty;
    using Base::fill;
    using Base::front;
};

}  // namespace ityp

#endif  // SRC_DAWN_COMMON_ITYP_ARRAY_H_
