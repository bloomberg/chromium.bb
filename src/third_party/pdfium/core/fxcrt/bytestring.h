// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCRT_BYTESTRING_H_
#define CORE_FXCRT_BYTESTRING_H_

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <functional>
#include <iosfwd>
#include <iterator>
#include <utility>

#include "core/fxcrt/fx_string_wrappers.h"
#include "core/fxcrt/retain_ptr.h"
#include "core/fxcrt/string_data_template.h"
#include "core/fxcrt/string_view_template.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/base/check.h"
#include "third_party/base/span.h"

namespace fxcrt {

// A mutable string with shared buffers using copy-on-write semantics that
// avoids the cost of std::string's iterator stability guarantees.
class ByteString {
 public:
  using CharType = char;
  using const_iterator = const CharType*;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  [[nodiscard]] static ByteString FormatInteger(int i);
  [[nodiscard]] static ByteString FormatFloat(float f);
  [[nodiscard]] static ByteString Format(const char* pFormat, ...);
  [[nodiscard]] static ByteString FormatV(const char* pFormat, va_list argList);

  ByteString();
  ByteString(const ByteString& other);

  // Move-construct a ByteString. After construction, |other| is empty.
  ByteString(ByteString&& other) noexcept;

  // Make a one-character string from a char.
  explicit ByteString(char ch);

  // Deliberately implicit to avoid calling on every string literal.
  // NOLINTNEXTLINE(runtime/explicit)
  ByteString(const char* ptr);

  // No implicit conversions from wide strings.
  // NOLINTNEXTLINE(runtime/explicit)
  ByteString(wchar_t) = delete;

  ByteString(const char* pStr, size_t len);
  ByteString(const uint8_t* pStr, size_t len);

  explicit ByteString(ByteStringView bstrc);
  ByteString(ByteStringView str1, ByteStringView str2);
  ByteString(const std::initializer_list<ByteStringView>& list);
  explicit ByteString(const fxcrt::ostringstream& outStream);

  ~ByteString();

  // Holds on to buffer if possible for later re-use. Assign ByteString()
  // to force immediate release if desired.
  void clear();

  // Explicit conversion to C-style string.
  // Note: Any subsequent modification of |this| will invalidate the result.
  const char* c_str() const { return m_pData ? m_pData->m_String : ""; }

  // Explicit conversion to uint8_t*.
  // Note: Any subsequent modification of |this| will invalidate the result.
  const uint8_t* raw_str() const {
    return m_pData ? reinterpret_cast<const uint8_t*>(m_pData->m_String)
                   : nullptr;
  }

  // Explicit conversion to ByteStringView.
  // Note: Any subsequent modification of |this| will invalidate the result.
  ByteStringView AsStringView() const {
    return ByteStringView(raw_str(), GetLength());
  }

  // Explicit conversion to span.
  // Note: Any subsequent modification of |this| will invalidate the result.
  pdfium::span<const char> span() const {
    return pdfium::make_span(m_pData ? m_pData->m_String : nullptr,
                             GetLength());
  }
  pdfium::span<const uint8_t> raw_span() const {
    return pdfium::make_span(raw_str(), GetLength());
  }

  // Note: Any subsequent modification of |this| will invalidate iterators.
  const_iterator begin() const { return m_pData ? m_pData->m_String : nullptr; }
  const_iterator end() const {
    return m_pData ? m_pData->m_String + m_pData->m_nDataLength : nullptr;
  }

  // Note: Any subsequent modification of |this| will invalidate iterators.
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }

  size_t GetLength() const { return m_pData ? m_pData->m_nDataLength : 0; }
  size_t GetStringLength() const {
    return m_pData ? strlen(m_pData->m_String) : 0;
  }
  bool IsEmpty() const { return !GetLength(); }
  bool IsValidIndex(size_t index) const { return index < GetLength(); }
  bool IsValidLength(size_t length) const { return length <= GetLength(); }

  int Compare(ByteStringView str) const;
  bool EqualNoCase(ByteStringView str) const;

  bool operator==(const char* ptr) const;
  bool operator==(ByteStringView str) const;
  bool operator==(const ByteString& other) const;

  bool operator!=(const char* ptr) const { return !(*this == ptr); }
  bool operator!=(ByteStringView str) const { return !(*this == str); }
  bool operator!=(const ByteString& other) const { return !(*this == other); }

  bool operator<(const char* ptr) const;
  bool operator<(ByteStringView str) const;
  bool operator<(const ByteString& other) const;

  ByteString& operator=(const char* str);
  ByteString& operator=(ByteStringView str);
  ByteString& operator=(const ByteString& that);

  // Move-assign a ByteString. After assignment, |that| is empty.
  ByteString& operator=(ByteString&& that) noexcept;

  ByteString& operator+=(char ch);
  ByteString& operator+=(const char* str);
  ByteString& operator+=(const ByteString& str);
  ByteString& operator+=(ByteStringView str);

  CharType operator[](const size_t index) const {
    CHECK(IsValidIndex(index));
    return m_pData->m_String[index];
  }

  CharType Front() const { return GetLength() ? (*this)[0] : 0; }
  CharType Back() const { return GetLength() ? (*this)[GetLength() - 1] : 0; }

  void SetAt(size_t index, char c);

  size_t Insert(size_t index, char ch);
  size_t InsertAtFront(char ch) { return Insert(0, ch); }
  size_t InsertAtBack(char ch) { return Insert(GetLength(), ch); }
  size_t Delete(size_t index, size_t count = 1);

  void Reserve(size_t len);

  // Note: any modification of the string (including ReleaseBuffer()) may
  // invalidate the span, which must not outlive its buffer.
  pdfium::span<char> GetBuffer(size_t nMinBufLength);
  void ReleaseBuffer(size_t nNewLength);

  ByteString Substr(size_t offset) const;
  ByteString Substr(size_t first, size_t count) const;
  ByteString First(size_t count) const;
  ByteString Last(size_t count) const;

  absl::optional<size_t> Find(ByteStringView subStr, size_t start = 0) const;
  absl::optional<size_t> Find(char ch, size_t start = 0) const;
  absl::optional<size_t> ReverseFind(char ch) const;

  bool Contains(ByteStringView lpszSub, size_t start = 0) const {
    return Find(lpszSub, start).has_value();
  }

  bool Contains(char ch, size_t start = 0) const {
    return Find(ch, start).has_value();
  }

  void MakeLower();
  void MakeUpper();

  void Trim();
  void Trim(char target);
  void Trim(ByteStringView targets);

  void TrimLeft();
  void TrimLeft(char target);
  void TrimLeft(ByteStringView targets);

  void TrimRight();
  void TrimRight(char target);
  void TrimRight(ByteStringView targets);

  size_t Replace(ByteStringView pOld, ByteStringView pNew);
  size_t Remove(char ch);

  uint32_t GetID() const { return AsStringView().GetID(); }

 protected:
  using StringData = StringDataTemplate<char>;

  void ReallocBeforeWrite(size_t nNewLen);
  void AllocBeforeWrite(size_t nNewLen);
  void AllocCopy(ByteString& dest, size_t nCopyLen, size_t nCopyIndex) const;
  void AssignCopy(const char* pSrcData, size_t nSrcLen);
  void Concat(const char* pSrcData, size_t nSrcLen);
  intptr_t ReferenceCountForTesting() const;

  RetainPtr<StringData> m_pData;

  friend class ByteString_Assign_Test;
  friend class ByteString_Concat_Test;
  friend class ByteString_Construct_Test;
  friend class StringPool_ByteString_Test;
};

inline bool operator==(const char* lhs, const ByteString& rhs) {
  return rhs == lhs;
}
inline bool operator==(ByteStringView lhs, const ByteString& rhs) {
  return rhs == lhs;
}
inline bool operator!=(const char* lhs, const ByteString& rhs) {
  return rhs != lhs;
}
inline bool operator!=(ByteStringView lhs, const ByteString& rhs) {
  return rhs != lhs;
}
inline bool operator<(const char* lhs, const ByteString& rhs) {
  return rhs.Compare(lhs) > 0;
}
inline bool operator<(const ByteStringView& lhs, const ByteString& rhs) {
  return rhs.Compare(lhs) > 0;
}
inline bool operator<(const ByteStringView& lhs, const char* rhs) {
  return lhs < ByteStringView(rhs);
}

inline ByteString operator+(ByteStringView str1, ByteStringView str2) {
  return ByteString(str1, str2);
}
inline ByteString operator+(ByteStringView str1, const char* str2) {
  return ByteString(str1, str2);
}
inline ByteString operator+(const char* str1, ByteStringView str2) {
  return ByteString(str1, str2);
}
inline ByteString operator+(ByteStringView str1, char ch) {
  return ByteString(str1, ByteStringView(ch));
}
inline ByteString operator+(char ch, ByteStringView str2) {
  return ByteString(ByteStringView(ch), str2);
}
inline ByteString operator+(const ByteString& str1, const ByteString& str2) {
  return ByteString(str1.AsStringView(), str2.AsStringView());
}
inline ByteString operator+(const ByteString& str1, char ch) {
  return ByteString(str1.AsStringView(), ByteStringView(ch));
}
inline ByteString operator+(char ch, const ByteString& str2) {
  return ByteString(ByteStringView(ch), str2.AsStringView());
}
inline ByteString operator+(const ByteString& str1, const char* str2) {
  return ByteString(str1.AsStringView(), str2);
}
inline ByteString operator+(const char* str1, const ByteString& str2) {
  return ByteString(str1, str2.AsStringView());
}
inline ByteString operator+(const ByteString& str1, ByteStringView str2) {
  return ByteString(str1.AsStringView(), str2);
}
inline ByteString operator+(ByteStringView str1, const ByteString& str2) {
  return ByteString(str1, str2.AsStringView());
}

std::ostream& operator<<(std::ostream& os, const ByteString& str);
std::ostream& operator<<(std::ostream& os, ByteStringView str);

}  // namespace fxcrt

using ByteString = fxcrt::ByteString;

uint32_t FX_HashCode_GetA(ByteStringView str);
uint32_t FX_HashCode_GetLoweredA(ByteStringView str);
uint32_t FX_HashCode_GetAsIfW(ByteStringView str);
uint32_t FX_HashCode_GetLoweredAsIfW(ByteStringView str);

namespace std {

template <>
struct hash<ByteString> {
  size_t operator()(const ByteString& str) const {
    return FX_HashCode_GetA(str.AsStringView());
  }
};

}  // namespace std

extern template struct std::hash<ByteString>;

#endif  // CORE_FXCRT_BYTESTRING_H_
