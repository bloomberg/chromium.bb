// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copied from strings/stringpiece.h with modifications
//
// A string-like object that points to a sized piece of memory.
//
// You can use StringPiece as a function or method parameter.  A StringPiece
// parameter can receive a double-quoted string literal argument, a "const
// char*" argument, a string argument, or a StringPiece argument with no data
// copying.  Systematic use of StringPiece for arguments reduces data
// copies and strlen() calls.
//
// Prefer passing StringPieces by value:
//   void MyFunction(StringPiece arg);
// If circumstances require, you may also pass by const reference:
//   void MyFunction(const StringPiece& arg);  // not preferred
// Both of these have the same lifetime semantics.  Passing by value
// generates slightly smaller code.  For more discussion, Googlers can see
// the thread go/stringpiecebyvalue on c-users.

#ifndef BASE_STRINGS_STRING_PIECE_H_
#define BASE_STRINGS_STRING_PIECE_H_

#include <stddef.h>

#include <iosfwd>
#include <ostream>
#include <string>
#include <type_traits>

#include "base/base_export.h"
#include "base/check_op.h"
#include "base/strings/char_traits.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece_forward.h"
#include "build/build_config.h"

namespace base {

// internal --------------------------------------------------------------------

// Many of the StringPiece functions use different implementations for the
// 8-bit and 16-bit versions, and we don't want lots of template expansions in
// this (very common) header that will slow down compilation.
//
// So here we define overloaded functions called by the StringPiece template.
// For those that share an implementation, the two versions will expand to a
// template internal to the .cc file.
namespace internal {

BASE_EXPORT size_t find(StringPiece self, StringPiece s, size_t pos);
BASE_EXPORT size_t find(StringPiece16 self, StringPiece16 s, size_t pos);

BASE_EXPORT size_t rfind(StringPiece self, StringPiece s, size_t pos);
BASE_EXPORT size_t rfind(StringPiece16 self, StringPiece16 s, size_t pos);

BASE_EXPORT size_t find_first_of(StringPiece self, StringPiece s, size_t pos);
BASE_EXPORT size_t find_first_of(StringPiece16 self,
                                 StringPiece16 s,
                                 size_t pos);

BASE_EXPORT size_t find_first_not_of(StringPiece self,
                                     StringPiece s,
                                     size_t pos);
BASE_EXPORT size_t find_first_not_of(StringPiece16 self,
                                     StringPiece16 s,
                                     size_t pos);

BASE_EXPORT size_t find_last_of(StringPiece self, StringPiece s, size_t pos);
BASE_EXPORT size_t find_last_of(StringPiece16 self,
                                StringPiece16 s,
                                size_t pos);

BASE_EXPORT size_t find_last_not_of(StringPiece self,
                                    StringPiece s,
                                    size_t pos);
BASE_EXPORT size_t find_last_not_of(StringPiece16 self,
                                    StringPiece16 s,
                                    size_t pos);

// Overloads for WStringPiece in case it is not the same type as StringPiece16.
#if !defined(WCHAR_T_IS_UTF16)
BASE_EXPORT size_t find(WStringPiece self, WStringPiece s, size_t pos);
BASE_EXPORT size_t rfind(WStringPiece self, WStringPiece s, size_t pos);
BASE_EXPORT size_t find_first_of(WStringPiece self, WStringPiece s, size_t pos);
BASE_EXPORT size_t find_first_not_of(WStringPiece self,
                                     WStringPiece s,
                                     size_t pos);
BASE_EXPORT size_t find_last_of(WStringPiece self, WStringPiece s, size_t pos);
BASE_EXPORT size_t find_last_not_of(WStringPiece self,
                                    WStringPiece s,
                                    size_t pos);
#endif

}  // namespace internal

// BasicStringPiece ------------------------------------------------------------

// Defines the types, methods, operators, and data members common to both
// StringPiece and StringPiece16.
//
// This is templatized by string class type rather than character type, so
// BasicStringPiece<std::string> or BasicStringPiece<base::string16>.
template <typename STRING_TYPE> class BasicStringPiece {
 public:
  // Standard STL container boilerplate.
  typedef size_t size_type;
  typedef typename STRING_TYPE::traits_type traits_type;
  typedef typename STRING_TYPE::value_type value_type;
  typedef const value_type* pointer;
  typedef const value_type& reference;
  typedef const value_type& const_reference;
  typedef ptrdiff_t difference_type;
  typedef const value_type* const_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

  static const size_type npos;

 public:
  // We provide non-explicit singleton constructors so users can pass
  // in a "const char*" or a "string" wherever a "StringPiece" is
  // expected (likewise for char16, string16, StringPiece16).
  constexpr BasicStringPiece() : ptr_(nullptr), length_(0) {}
  // TODO(crbug.com/1049498): Construction from nullptr is not allowed for
  // std::basic_string_view, so remove the special handling for it.
  // Note: This doesn't just use STRING_TYPE::traits_type::length(), since that
  // isn't constexpr until C++17.
  constexpr BasicStringPiece(const value_type* str)
      : ptr_(str), length_(!str ? 0 : CharTraits<value_type>::length(str)) {}
  // Explicitly disallow construction from nullptr. Note that this does not
  // catch construction from runtime strings that might be null.
  // Note: The following is just a more elaborate way of spelling
  // `BasicStringPiece(nullptr_t) = delete`, but unfortunately the terse form is
  // not supported by the PNaCl toolchain.
  // TODO(crbug.com/1049498): Remove once we CHECK(str) in the constructor
  // above.
  template <class T, class = std::enable_if_t<std::is_null_pointer<T>::value>>
  BasicStringPiece(T) {
    static_assert(sizeof(T) == 0,  // Always false.
                  "StringPiece does not support construction from nullptr, use "
                  "the default constructor instead.");
  }
  BasicStringPiece(const STRING_TYPE& str)
      : ptr_(str.data()), length_(str.size()) {}
  constexpr BasicStringPiece(const value_type* offset, size_type len)
      : ptr_(offset), length_(len) {}

  // data() may return a pointer to a buffer with embedded NULs, and the
  // returned buffer may or may not be null terminated.  Therefore it is
  // typically a mistake to pass data() to a routine that expects a NUL
  // terminated string.
  constexpr const value_type* data() const { return ptr_; }
  constexpr size_type size() const noexcept { return length_; }
  constexpr size_type length() const noexcept { return length_; }
  constexpr bool empty() const noexcept { return length_ == 0; }

  constexpr value_type operator[](size_type i) const {
    CHECK(i < length_);
    return ptr_[i];
  }

  constexpr value_type front() const {
    CHECK_NE(0UL, length_);
    return ptr_[0];
  }

  constexpr value_type back() const {
    CHECK_NE(0UL, length_);
    return ptr_[length_ - 1];
  }

  constexpr void remove_prefix(size_type n) {
    CHECK(n <= length_);
    ptr_ += n;
    length_ -= n;
  }

  constexpr void remove_suffix(size_type n) {
    CHECK(n <= length_);
    length_ -= n;
  }

  // This is the style of conversion preferred by std::string_view in C++17.
  explicit operator STRING_TYPE() const {
    return empty() ? STRING_TYPE() : STRING_TYPE(data(), size());
  }

  // Deprecated, use operator STRING_TYPE() instead.
  // TODO(crbug.com/1049498): Remove for all STRING_TYPEs.
  template <typename StrT = STRING_TYPE,
            typename = std::enable_if_t<std::is_same<StrT, std::string>::value>>
  STRING_TYPE as_string() const {
    return STRING_TYPE(*this);
  }

  constexpr const_iterator begin() const noexcept { return ptr_; }
  constexpr const_iterator end() const noexcept { return ptr_ + length_; }
  constexpr const_reverse_iterator rbegin() const noexcept {
    return const_reverse_iterator(ptr_ + length_);
  }
  constexpr const_reverse_iterator rend() const noexcept {
    return const_reverse_iterator(ptr_);
  }

  size_type max_size() const { return length_; }
  size_type capacity() const { return length_; }

  // String operations, see https://wg21.link/string.view.ops.
  constexpr size_type copy(value_type* s,
                           size_type n,
                           size_type pos = 0) const {
    CHECK_LE(pos, size());
    size_type rlen = std::min(n, size() - pos);
    traits_type::copy(s, data() + pos, rlen);
    return rlen;
  }

  constexpr BasicStringPiece substr(size_type pos = 0,
                                    size_type n = npos) const {
    CHECK_LE(pos, size());
    return {data() + pos, std::min(n, size() - pos)};
  }

  constexpr int compare(BasicStringPiece str) const noexcept {
    size_type rlen = std::min(size(), str.size());
    int result = CharTraits<value_type>::compare(data(), str.data(), rlen);
    if (result == 0)
      result = size() == str.size() ? 0 : (size() < str.size() ? -1 : 1);
    return result;
  }

  constexpr int compare(size_type pos,
                        size_type n,
                        BasicStringPiece str) const {
    return substr(pos, n).compare(str);
  }

  constexpr int compare(size_type pos1,
                        size_type n1,
                        BasicStringPiece str,
                        size_type pos2,
                        size_type n2) const {
    return substr(pos1, n1).compare(str.substr(pos2, n2));
  }

  constexpr int compare(const value_type* s) const {
    return compare(BasicStringPiece(s));
  }

  constexpr int compare(size_type pos, size_type n, const value_type* s) const {
    return substr(pos, n).compare(BasicStringPiece(s));
  }

  constexpr int compare(size_type pos,
                        size_type n1,
                        const value_type* s,
                        size_type n2) const {
    return substr(pos, n1).compare(BasicStringPiece(s, n2));
  }

  // Searching, see https://wg21.link/string.view.find.

  // find: Search for a character or substring at a given offset.
  constexpr size_type find(BasicStringPiece s,
                           size_type pos = 0) const noexcept {
    return internal::find(*this, s, pos);
  }

  constexpr size_type find(value_type c, size_type pos = 0) const noexcept {
    if (pos >= size())
      return npos;

    const value_type* result =
        base::CharTraits<value_type>::find(data() + pos, size() - pos, c);
    return result ? static_cast<size_type>(result - data()) : npos;
  }

  constexpr size_type find(const value_type* s,
                           size_type pos,
                           size_type n) const {
    return find(BasicStringPiece(s, n), pos);
  }

  constexpr size_type find(const value_type* s, size_type pos = 0) const {
    return find(BasicStringPiece(s), pos);
  }

  // rfind: Reverse find.
  constexpr size_type rfind(BasicStringPiece s,
                            size_type pos = npos) const noexcept {
    return internal::rfind(*this, s, pos);
  }

  constexpr size_type rfind(value_type c, size_type pos = npos) const noexcept {
    if (empty())
      return npos;

    for (size_t i = std::min(pos, size() - 1);; --i) {
      if (data()[i] == c)
        return i;

      if (i == 0)
        break;
    }
    return npos;
  }

  constexpr size_type rfind(const value_type* s,
                            size_type pos,
                            size_type n) const {
    return rfind(BasicStringPiece(s, n), pos);
  }

  constexpr size_type rfind(const value_type* s, size_type pos = npos) const {
    return rfind(BasicStringPiece(s), pos);
  }

  // find_first_of: Find the first occurrence of one of a set of characters.
  constexpr size_type find_first_of(BasicStringPiece s,
                                    size_type pos = 0) const noexcept {
    return internal::find_first_of(*this, s, pos);
  }

  constexpr size_type find_first_of(value_type c,
                                    size_type pos = 0) const noexcept {
    return find(c, pos);
  }

  constexpr size_type find_first_of(const value_type* s,
                                    size_type pos,
                                    size_type n) const {
    return find_first_of(BasicStringPiece(s, n), pos);
  }

  constexpr size_type find_first_of(const value_type* s,
                                    size_type pos = 0) const {
    return find_first_of(BasicStringPiece(s), pos);
  }

  // find_last_of: Find the last occurrence of one of a set of characters.
  constexpr size_type find_last_of(BasicStringPiece s,
                                   size_type pos = npos) const noexcept {
    return internal::find_last_of(*this, s, pos);
  }

  constexpr size_type find_last_of(value_type c,
                                   size_type pos = npos) const noexcept {
    return rfind(c, pos);
  }

  constexpr size_type find_last_of(const value_type* s,
                                   size_type pos,
                                   size_type n) const {
    return find_last_of(BasicStringPiece(s, n), pos);
  }

  constexpr size_type find_last_of(const value_type* s,
                                   size_type pos = npos) const {
    return find_last_of(BasicStringPiece(s), pos);
  }

  // find_first_not_of: Find the first occurrence not of a set of characters.
  constexpr size_type find_first_not_of(BasicStringPiece s,
                                        size_type pos = 0) const noexcept {
    return internal::find_first_not_of(*this, s, pos);
  }

  constexpr size_type find_first_not_of(value_type c,
                                        size_type pos = 0) const noexcept {
    if (empty())
      return npos;

    for (; pos < size(); ++pos) {
      if (data()[pos] != c)
        return pos;
    }
    return npos;
  }

  constexpr size_type find_first_not_of(const value_type* s,
                                        size_type pos,
                                        size_type n) const {
    return find_first_not_of(BasicStringPiece(s, n), pos);
  }

  constexpr size_type find_first_not_of(const value_type* s,
                                        size_type pos = 0) const {
    return find_first_not_of(BasicStringPiece(s), pos);
  }

  // find_last_not_of: Find the last occurrence not of a set of characters.
  constexpr size_type find_last_not_of(BasicStringPiece s,
                                       size_type pos = npos) const noexcept {
    return internal::find_last_not_of(*this, s, pos);
  }

  constexpr size_type find_last_not_of(value_type c,
                                       size_type pos = npos) const noexcept {
    if (empty())
      return npos;

    for (size_t i = std::min(pos, size() - 1);; --i) {
      if (data()[i] != c)
        return i;
      if (i == 0)
        break;
    }
    return npos;
  }

  constexpr size_type find_last_not_of(const value_type* s,
                                       size_type pos,
                                       size_type n) const {
    return find_last_not_of(BasicStringPiece(s, n), pos);
  }

  constexpr size_type find_last_not_of(const value_type* s,
                                       size_type pos = npos) const {
    return find_last_not_of(BasicStringPiece(s), pos);
  }

 protected:
  const value_type* ptr_;
  size_type length_;
};

template <typename STRING_TYPE>
const typename BasicStringPiece<STRING_TYPE>::size_type
BasicStringPiece<STRING_TYPE>::npos =
    typename BasicStringPiece<STRING_TYPE>::size_type(-1);

// MSVC doesn't like complex extern templates and DLLs.
#if !defined(COMPILER_MSVC)
extern template class BASE_EXPORT BasicStringPiece<std::string>;
extern template class BASE_EXPORT BasicStringPiece<string16>;
#endif

// Comparison operators --------------------------------------------------------
// operator ==
template <typename StringT>
constexpr bool operator==(BasicStringPiece<StringT> lhs,
                          BasicStringPiece<StringT> rhs) noexcept {
  return lhs.size() == rhs.size() && lhs.compare(rhs) == 0;
}

// Here and below we make use of std::common_type_t to emulate an identity type
// transformation. This creates a non-deduced context, so that we can compare
// StringPieces with types that implicitly convert to StringPieces. See
// https://wg21.link/n3766 for details.
// Furthermore, we require dummy template parameters for these overloads to work
// around a name mangling issue on Windows.
template <typename StringT, int = 1>
constexpr bool operator==(
    BasicStringPiece<StringT> lhs,
    std::common_type_t<BasicStringPiece<StringT>> rhs) noexcept {
  return lhs.size() == rhs.size() && lhs.compare(rhs) == 0;
}

template <typename StringT, int = 2>
constexpr bool operator==(std::common_type_t<BasicStringPiece<StringT>> lhs,
                          BasicStringPiece<StringT> rhs) noexcept {
  return lhs.size() == rhs.size() && lhs.compare(rhs) == 0;
}

// operator !=
template <typename StringT>
constexpr bool operator!=(BasicStringPiece<StringT> lhs,
                          BasicStringPiece<StringT> rhs) noexcept {
  return !(lhs == rhs);
}

template <typename StringT, int = 1>
constexpr bool operator!=(
    BasicStringPiece<StringT> lhs,
    std::common_type_t<BasicStringPiece<StringT>> rhs) noexcept {
  return !(lhs == rhs);
}

template <typename StringT, int = 2>
constexpr bool operator!=(std::common_type_t<BasicStringPiece<StringT>> lhs,
                          BasicStringPiece<StringT> rhs) noexcept {
  return !(lhs == rhs);
}

// operator <
template <typename StringT>
constexpr bool operator<(BasicStringPiece<StringT> lhs,
                         BasicStringPiece<StringT> rhs) noexcept {
  return lhs.compare(rhs) < 0;
}

template <typename StringT, int = 1>
constexpr bool operator<(
    BasicStringPiece<StringT> lhs,
    std::common_type_t<BasicStringPiece<StringT>> rhs) noexcept {
  return lhs.compare(rhs) < 0;
}

template <typename StringT, int = 2>
constexpr bool operator<(std::common_type_t<BasicStringPiece<StringT>> lhs,
                         BasicStringPiece<StringT> rhs) noexcept {
  return lhs.compare(rhs) < 0;
}

// operator >
template <typename StringT>
constexpr bool operator>(BasicStringPiece<StringT> lhs,
                         BasicStringPiece<StringT> rhs) noexcept {
  return rhs < lhs;
}

template <typename StringT, int = 1>
constexpr bool operator>(
    BasicStringPiece<StringT> lhs,
    std::common_type_t<BasicStringPiece<StringT>> rhs) noexcept {
  return rhs < lhs;
}

template <typename StringT, int = 2>
constexpr bool operator>(std::common_type_t<BasicStringPiece<StringT>> lhs,
                         BasicStringPiece<StringT> rhs) noexcept {
  return rhs < lhs;
}

// operator <=
template <typename StringT>
constexpr bool operator<=(BasicStringPiece<StringT> lhs,
                          BasicStringPiece<StringT> rhs) noexcept {
  return !(rhs < lhs);
}

template <typename StringT, int = 1>
constexpr bool operator<=(
    BasicStringPiece<StringT> lhs,
    std::common_type_t<BasicStringPiece<StringT>> rhs) noexcept {
  return !(rhs < lhs);
}

template <typename StringT, int = 2>
constexpr bool operator<=(std::common_type_t<BasicStringPiece<StringT>> lhs,
                          BasicStringPiece<StringT> rhs) noexcept {
  return !(rhs < lhs);
}

// operator >=
template <typename StringT>
constexpr bool operator>=(BasicStringPiece<StringT> lhs,
                          BasicStringPiece<StringT> rhs) noexcept {
  return !(lhs < rhs);
}

template <typename StringT, int = 1>
constexpr bool operator>=(
    BasicStringPiece<StringT> lhs,
    std::common_type_t<BasicStringPiece<StringT>> rhs) noexcept {
  return !(lhs < rhs);
}

template <typename StringT, int = 2>
constexpr bool operator>=(std::common_type_t<BasicStringPiece<StringT>> lhs,
                          BasicStringPiece<StringT> rhs) noexcept {
  return !(lhs < rhs);
}

BASE_EXPORT std::ostream& operator<<(std::ostream& o, StringPiece piece);
BASE_EXPORT std::ostream& operator<<(std::ostream& o, StringPiece16 piece);

#if !defined(WCHAR_T_IS_UTF16)
BASE_EXPORT std::ostream& operator<<(std::ostream& o, WStringPiece piece);
#endif

// Hashing ---------------------------------------------------------------------

// We provide appropriate hash functions so StringPiece and StringPiece16 can
// be used as keys in hash sets and maps.

// This hash function is copied from base/strings/string16.h. We don't use the
// ones already defined for string and string16 directly because it would
// require the string constructors to be called, which we don't want.

template <typename StringPieceType>
struct StringPieceHashImpl {
  std::size_t operator()(StringPieceType sp) const {
    std::size_t result = 0;
    for (auto c : sp)
      result = (result * 131) + c;
    return result;
  }
};

using StringPieceHash = StringPieceHashImpl<StringPiece>;
using StringPiece16Hash = StringPieceHashImpl<StringPiece16>;
using WStringPieceHash = StringPieceHashImpl<WStringPiece>;

}  // namespace base

#endif  // BASE_STRINGS_STRING_PIECE_H_
