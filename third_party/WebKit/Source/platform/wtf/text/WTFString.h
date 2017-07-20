/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012, 2013 Apple Inc.
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef WTFString_h
#define WTFString_h

// This file would be called String.h, but that conflicts with <string.h>
// on systems without case-sensitive file systems.

#include "platform/wtf/Allocator.h"
#include "platform/wtf/Compiler.h"
#include "platform/wtf/HashTableDeletedValueType.h"
#include "platform/wtf/WTFExport.h"
#include "platform/wtf/text/ASCIIFastPath.h"
#include "platform/wtf/text/StringImpl.h"
#include "platform/wtf/text/StringView.h"
#include <algorithm>
#include <iosfwd>

#ifdef __OBJC__
#include <objc/objc.h>
#endif

namespace WTF {

class CString;
struct StringHash;

enum UTF8ConversionMode {
  kLenientUTF8Conversion,
  kStrictUTF8Conversion,
  kStrictUTF8ConversionReplacingUnpairedSurrogatesWithFFFD
};

#define DISPATCH_CASE_OP(caseSensitivity, op, args)     \
  ((caseSensitivity == kTextCaseSensitive)              \
       ? op args                                        \
       : (caseSensitivity == kTextCaseASCIIInsensitive) \
             ? op##IgnoringASCIICase args               \
             : op##IgnoringCase args)

// You can find documentation about this class in this doc:
// https://docs.google.com/document/d/1kOCUlJdh2WJMJGDf-WoEQhmnjKLaOYRbiHz5TiGJl14/edit?usp=sharing
class WTF_EXPORT String {
  USING_FAST_MALLOC(String);

 public:
  // Construct a null string, distinguishable from an empty string.
  String() {}

  // Construct a string with UTF-16 data.
  String(const UChar* characters, unsigned length);

  // Construct a string by copying the contents of a vector.
  // This method will never create a null string. Vectors with size() == 0
  // will return the empty string.
  // NOTE: This is different from String(vector.data(), vector.size())
  // which will sometimes return a null string when vector.data() is null
  // which can only occur for vectors without inline capacity.
  // See: https://bugs.webkit.org/show_bug.cgi?id=109792
  template <size_t inlineCapacity>
  explicit String(const Vector<UChar, inlineCapacity>&);

  // Construct a string with UTF-16 data, from a null-terminated source.
  String(const UChar*);
  String(const char16_t* chars)
      : String(reinterpret_cast<const UChar*>(chars)) {}

  // Construct a string with latin1 data.
  String(const LChar* characters, unsigned length);
  String(const char* characters, unsigned length);

  // Construct a string with latin1 data, from a null-terminated source.
  String(const LChar* characters)
      : String(reinterpret_cast<const char*>(characters)) {}
  String(const char* characters)
      : String(characters, characters ? strlen(characters) : 0) {}

  // Construct a string referencing an existing StringImpl.
  String(StringImpl* impl) : impl_(impl) {}
  String(RefPtr<StringImpl> impl) : impl_(std::move(impl)) {}

  void swap(String& o) { impl_.Swap(o.impl_); }

  template <typename CharType>
  static String Adopt(StringBuffer<CharType>& buffer) {
    if (!buffer.length())
      return StringImpl::empty_;
    return String(buffer.Release());
  }

  explicit operator bool() const { return !IsNull(); }
  bool IsNull() const { return !impl_; }
  bool IsEmpty() const { return !impl_ || !impl_->length(); }

  StringImpl* Impl() const { return impl_.Get(); }
  RefPtr<StringImpl> ReleaseImpl() { return std::move(impl_); }

  unsigned length() const {
    if (!impl_)
      return 0;
    return impl_->length();
  }

  const LChar* Characters8() const {
    if (!impl_)
      return 0;
    DCHECK(impl_->Is8Bit());
    return impl_->Characters8();
  }

  const UChar* Characters16() const {
    if (!impl_)
      return 0;
    DCHECK(!impl_->Is8Bit());
    return impl_->Characters16();
  }

  // Return characters8() or characters16() depending on CharacterType.
  template <typename CharacterType>
  inline const CharacterType* GetCharacters() const;

  bool Is8Bit() const { return impl_->Is8Bit(); }

  CString Ascii() const;
  CString Latin1() const;
  CString Utf8(UTF8ConversionMode = kLenientUTF8Conversion) const;

  UChar operator[](unsigned index) const {
    if (!impl_ || index >= impl_->length())
      return 0;
    return (*impl_)[index];
  }

  static String Number(int);
  static String Number(unsigned);
  static String Number(long);
  static String Number(unsigned long);
  static String Number(long long);
  static String Number(unsigned long long);

  static String Number(double, unsigned precision = 6);

  // Number to String conversion following the ECMAScript definition.
  static String NumberToStringECMAScript(double);
  static String NumberToStringFixedWidth(double, unsigned decimal_places);

  // Find characters.
  size_t find(UChar c, unsigned start = 0) const {
    return impl_ ? impl_->Find(c, start) : kNotFound;
  }
  size_t find(LChar c, unsigned start = 0) const {
    return impl_ ? impl_->Find(c, start) : kNotFound;
  }
  size_t find(char c, unsigned start = 0) const {
    return find(static_cast<LChar>(c), start);
  }
  size_t Find(CharacterMatchFunctionPtr match_function,
              unsigned start = 0) const {
    return impl_ ? impl_->Find(match_function, start) : kNotFound;
  }

  // Find substrings.
  size_t Find(const StringView& value,
              unsigned start = 0,
              TextCaseSensitivity case_sensitivity = kTextCaseSensitive) const {
    return impl_
               ? DISPATCH_CASE_OP(case_sensitivity, impl_->Find, (value, start))
               : kNotFound;
  }

  // Unicode aware case insensitive string matching. Non-ASCII characters might
  // match to ASCII characters. This function is rarely used to implement web
  // platform features.
  size_t FindIgnoringCase(const StringView& value, unsigned start = 0) const {
    return impl_ ? impl_->FindIgnoringCase(value, start) : kNotFound;
  }

  // ASCII case insensitive string matching.
  size_t FindIgnoringASCIICase(const StringView& value,
                               unsigned start = 0) const {
    return impl_ ? impl_->FindIgnoringASCIICase(value, start) : kNotFound;
  }

  bool Contains(char c) const { return find(c) != kNotFound; }
  bool Contains(
      const StringView& value,
      TextCaseSensitivity case_sensitivity = kTextCaseSensitive) const {
    return Find(value, 0, case_sensitivity) != kNotFound;
  }

  // Find the last instance of a single character or string.
  size_t ReverseFind(UChar c, unsigned start = UINT_MAX) const {
    return impl_ ? impl_->ReverseFind(c, start) : kNotFound;
  }
  size_t ReverseFind(const StringView& value, unsigned start = UINT_MAX) const {
    return impl_ ? impl_->ReverseFind(value, start) : kNotFound;
  }

  UChar32 CharacterStartingAt(unsigned) const;

  bool StartsWith(
      const StringView& prefix,
      TextCaseSensitivity case_sensitivity = kTextCaseSensitive) const {
    return impl_
               ? DISPATCH_CASE_OP(case_sensitivity, impl_->StartsWith, (prefix))
               : prefix.IsEmpty();
  }
  bool StartsWithIgnoringCase(const StringView& prefix) const {
    return impl_ ? impl_->StartsWithIgnoringCase(prefix) : prefix.IsEmpty();
  }
  bool StartsWithIgnoringASCIICase(const StringView& prefix) const {
    return impl_ ? impl_->StartsWithIgnoringASCIICase(prefix)
                 : prefix.IsEmpty();
  }
  bool StartsWith(UChar character) const {
    return impl_ ? impl_->StartsWith(character) : false;
  }

  bool EndsWith(
      const StringView& suffix,
      TextCaseSensitivity case_sensitivity = kTextCaseSensitive) const {
    return impl_ ? DISPATCH_CASE_OP(case_sensitivity, impl_->EndsWith, (suffix))
                 : suffix.IsEmpty();
  }
  bool EndsWithIgnoringCase(const StringView& prefix) const {
    return impl_ ? impl_->EndsWithIgnoringCase(prefix) : prefix.IsEmpty();
  }
  bool EndsWithIgnoringASCIICase(const StringView& prefix) const {
    return impl_ ? impl_->EndsWithIgnoringASCIICase(prefix) : prefix.IsEmpty();
  }
  bool EndsWith(UChar character) const {
    return impl_ ? impl_->EndsWith(character) : false;
  }

  void append(const StringView&);
  void append(LChar);
  void append(char c) { append(static_cast<LChar>(c)); }
  void append(UChar);
  void insert(const StringView&, unsigned pos);

  // TODO(esprehn): replace strangely both modifies this String *and* return a
  // value. It should only do one of those.
  String& Replace(UChar pattern, UChar replacement) {
    if (impl_)
      impl_ = impl_->Replace(pattern, replacement);
    return *this;
  }
  String& Replace(UChar pattern, const StringView& replacement) {
    if (impl_)
      impl_ = impl_->Replace(pattern, replacement);
    return *this;
  }
  String& Replace(const StringView& pattern, const StringView& replacement) {
    if (impl_)
      impl_ = impl_->Replace(pattern, replacement);
    return *this;
  }
  String& replace(unsigned index,
                  unsigned length_to_replace,
                  const StringView& replacement) {
    if (impl_)
      impl_ = impl_->Replace(index, length_to_replace, replacement);
    return *this;
  }

  void Fill(UChar c) {
    if (impl_)
      impl_ = impl_->Fill(c);
  }

  void Ensure16Bit();

  void Truncate(unsigned length);
  void Remove(unsigned start, unsigned length = 1);

  String Substring(unsigned pos, unsigned len = UINT_MAX) const;
  String Left(unsigned len) const { return Substring(0, len); }
  String Right(unsigned len) const { return Substring(length() - len, len); }

  // Returns a lowercase version of the string. This function might convert
  // non-ASCII characters to ASCII characters. For example, DeprecatedLower()
  // for U+212A is 'k'.
  // This function is rarely used to implement web platform features. See
  // crbug.com/627682.
  // This function is deprecated. We should use LowerASCII() or introduce
  // LowerUnicode().
  String DeprecatedLower() const;

  // |locale_identifier| is case-insensitive, and accepts either of "aa_aa" or
  // "aa-aa". Empty/null |locale_identifier| indicates locale-independent
  // Unicode case conversion.
  String LowerUnicode(const AtomicString& locale_identifier) const;
  String UpperUnicode(const AtomicString& locale_identifier) const;

  // Returns a lowercase version of the string.
  // This function converts ASCII characters only.
  String LowerASCII() const;
  // Returns a uppercase version of the string.
  // This function converts ASCII characters only.
  String UpperASCII() const;

  String StripWhiteSpace() const;
  String StripWhiteSpace(IsWhiteSpaceFunctionPtr) const;
  String SimplifyWhiteSpace(StripBehavior = kStripExtraWhiteSpace) const;
  String SimplifyWhiteSpace(IsWhiteSpaceFunctionPtr,
                            StripBehavior = kStripExtraWhiteSpace) const;

  String RemoveCharacters(CharacterMatchFunctionPtr) const;
  template <bool isSpecialCharacter(UChar)>
  bool IsAllSpecialCharacters() const;

  // Return the string with case folded for case insensitive comparison.
  String FoldCase() const;

  // Takes a printf format and args and prints into a String.
  PRINTF_FORMAT(1, 2) static String Format(const char* format, ...);

  // Returns an uninitialized string. The characters needs to be written
  // into the buffer returned in data before the returned string is used.
  // Failure to do this will have unpredictable results.
  static String CreateUninitialized(unsigned length, UChar*& data) {
    return StringImpl::CreateUninitialized(length, data);
  }
  static String CreateUninitialized(unsigned length, LChar*& data) {
    return StringImpl::CreateUninitialized(length, data);
  }

  void Split(const StringView& separator,
             bool allow_empty_entries,
             Vector<String>& result) const;
  void Split(const StringView& separator, Vector<String>& result) const {
    Split(separator, false, result);
  }
  void Split(UChar separator,
             bool allow_empty_entries,
             Vector<String>& result) const;
  void Split(UChar separator, Vector<String>& result) const {
    Split(separator, false, result);
  }

  // Copy characters out of the string. See StringImpl.h for detailed docs.
  unsigned CopyTo(UChar* buffer, unsigned start, unsigned max_length) const {
    return impl_ ? impl_->CopyTo(buffer, start, max_length) : 0;
  }
  template <typename BufferType>
  void AppendTo(BufferType&,
                unsigned start = 0,
                unsigned length = UINT_MAX) const;
  template <typename BufferType>
  void PrependTo(BufferType&,
                 unsigned start = 0,
                 unsigned length = UINT_MAX) const;

  // Convert the string into a number.

  int ToIntStrict(bool* ok = 0) const;
  unsigned ToUIntStrict(bool* ok = 0) const;
  unsigned HexToUIntStrict(bool* ok) const;
  int64_t ToInt64Strict(bool* ok = 0) const;
  uint64_t ToUInt64Strict(bool* ok = 0) const;

  int ToInt(bool* ok = 0) const;
  unsigned ToUInt(bool* ok = 0) const;
  int64_t ToInt64(bool* ok = 0) const;
  uint64_t ToUInt64(bool* ok = 0) const;

  // FIXME: Like the strict functions above, these give false for "ok" when
  // there is trailing garbage.  Like the non-strict functions above, these
  // return the value when there is trailing garbage.  It would be better if
  // these were more consistent with the above functions instead.
  double ToDouble(bool* ok = 0) const;
  float ToFloat(bool* ok = 0) const;

  String IsolatedCopy() const;
  bool IsSafeToSendToAnotherThread() const;

#ifdef __OBJC__
  String(NSString*);

  // This conversion maps null string to "", which loses the meaning of null
  // string, but we need this mapping because AppKit crashes when passed nil
  // NSStrings.
  operator NSString*() const {
    if (!impl_)
      return @"";
    return *impl_;
  }
#endif

  static String Make8BitFrom16BitSource(const UChar*, size_t);
  template <size_t inlineCapacity>
  static String Make8BitFrom16BitSource(
      const Vector<UChar, inlineCapacity>& buffer) {
    return Make8BitFrom16BitSource(buffer.data(), buffer.size());
  }

  static String Make16BitFrom8BitSource(const LChar*, size_t);

  // String::fromUTF8 will return a null string if
  // the input data contains invalid UTF-8 sequences.
  static String FromUTF8(const LChar*, size_t);
  static String FromUTF8(const LChar*);
  static String FromUTF8(const char* s, size_t length) {
    return FromUTF8(reinterpret_cast<const LChar*>(s), length);
  }
  static String FromUTF8(const char* s) {
    return FromUTF8(reinterpret_cast<const LChar*>(s));
  }
  static String FromUTF8(const CString&);

  // Tries to convert the passed in string to UTF-8, but will fall back to
  // Latin-1 if the string is not valid UTF-8.
  static String FromUTF8WithLatin1Fallback(const LChar*, size_t);
  static String FromUTF8WithLatin1Fallback(const char* s, size_t length) {
    return FromUTF8WithLatin1Fallback(reinterpret_cast<const LChar*>(s),
                                      length);
  }

  bool ContainsOnlyASCII() const {
    return !impl_ || impl_->ContainsOnlyASCII();
  }
  bool ContainsOnlyLatin1() const;
  bool ContainsOnlyWhitespace() const {
    return !impl_ || impl_->ContainsOnlyWhitespace();
  }

  size_t CharactersSizeInBytes() const {
    return impl_ ? impl_->CharactersSizeInBytes() : 0;
  }

  // Hash table deleted values, which are only constructed and never copied or
  // destroyed.
  String(WTF::HashTableDeletedValueType) : impl_(WTF::kHashTableDeletedValue) {}
  bool IsHashTableDeletedValue() const {
    return impl_.IsHashTableDeletedValue();
  }

#ifndef NDEBUG
  // For use in the debugger.
  void Show() const;
#endif

 private:
  template <typename CharacterType>
  void AppendInternal(CharacterType);

  RefPtr<StringImpl> impl_;
};

#undef DISPATCH_CASE_OP

inline bool operator==(const String& a, const String& b) {
  // We don't use equalStringView here since we want the isAtomic() fast path
  // inside WTF::equal.
  return Equal(a.Impl(), b.Impl());
}
inline bool operator==(const String& a, const char* b) {
  return EqualStringView(a, b);
}
inline bool operator==(const char* a, const String& b) {
  return b == a;
}

inline bool operator!=(const String& a, const String& b) {
  return !(a == b);
}
inline bool operator!=(const String& a, const char* b) {
  return !(a == b);
}
inline bool operator!=(const char* a, const String& b) {
  return !(a == b);
}

inline bool EqualIgnoringNullity(const String& a, const String& b) {
  return EqualIgnoringNullity(a.Impl(), b.Impl());
}

template <size_t inlineCapacity>
inline bool EqualIgnoringNullity(const Vector<UChar, inlineCapacity>& a,
                                 const String& b) {
  return EqualIgnoringNullity(a, b.Impl());
}

inline void swap(String& a, String& b) {
  a.swap(b);
}

// Definitions of string operations

template <size_t inlineCapacity>
String::String(const Vector<UChar, inlineCapacity>& vector)
    : impl_(vector.size() ? StringImpl::Create(vector.data(), vector.size())
                          : StringImpl::empty_) {}

template <>
inline const LChar* String::GetCharacters<LChar>() const {
  DCHECK(Is8Bit());
  return Characters8();
}

template <>
inline const UChar* String::GetCharacters<UChar>() const {
  DCHECK(!Is8Bit());
  return Characters16();
}

inline bool String::ContainsOnlyLatin1() const {
  if (IsEmpty())
    return true;

  if (Is8Bit())
    return true;

  const UChar* characters = Characters16();
  UChar ored = 0;
  for (size_t i = 0; i < impl_->length(); ++i)
    ored |= characters[i];
  return !(ored & 0xFF00);
}

#ifdef __OBJC__
// This is for situations in WebKit where the long standing behavior has been
// "nil if empty", so we try to maintain longstanding behavior for the sake of
// entrenched clients
inline NSString* NsStringNilIfEmpty(const String& str) {
  return str.IsEmpty() ? nil : (NSString*)str;
}
#endif

WTF_EXPORT int CodePointCompare(const String&, const String&);

inline bool CodePointCompareLessThan(const String& a, const String& b) {
  return CodePointCompare(a.Impl(), b.Impl()) < 0;
}

WTF_EXPORT int CodePointCompareIgnoringASCIICase(const String&, const char*);

template <bool isSpecialCharacter(UChar)>
inline bool String::IsAllSpecialCharacters() const {
  return StringView(*this).IsAllSpecialCharacters<isSpecialCharacter>();
}

template <typename BufferType>
void String::AppendTo(BufferType& result,
                      unsigned position,
                      unsigned length) const {
  if (!impl_)
    return;
  impl_->AppendTo(result, position, length);
}

template <typename BufferType>
void String::PrependTo(BufferType& result,
                       unsigned position,
                       unsigned length) const {
  if (!impl_)
    return;
  impl_->PrependTo(result, position, length);
}

// StringHash is the default hash for String
template <typename T>
struct DefaultHash;
template <>
struct DefaultHash<String> {
  typedef StringHash Hash;
};

// Shared global empty string.
WTF_EXPORT extern const String& g_empty_string;
WTF_EXPORT extern const String& g_empty_string16_bit;
WTF_EXPORT extern const String& g_xmlns_with_colon;

// Pretty printer for gtest and base/logging.*.  It prepends and appends
// double-quotes, and escapes chracters other than ASCII printables.
WTF_EXPORT std::ostream& operator<<(std::ostream&, const String&);

inline StringView::StringView(const String& string,
                              unsigned offset,
                              unsigned length)
    : StringView(string.Impl(), offset, length) {}
inline StringView::StringView(const String& string, unsigned offset)
    : StringView(string.Impl(), offset) {}
inline StringView::StringView(const String& string)
    : StringView(string.Impl()) {}

}  // namespace WTF

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(String);

using WTF::CString;
using WTF::kStrictUTF8Conversion;
using WTF::kStrictUTF8ConversionReplacingUnpairedSurrogatesWithFFFD;
using WTF::String;
using WTF::g_empty_string;
using WTF::g_empty_string16_bit;
using WTF::CharactersAreAllASCII;
using WTF::Equal;
using WTF::Find;
using WTF::IsSpaceOrNewline;

#include "platform/wtf/text/AtomicString.h"
#endif  // WTFString_h
