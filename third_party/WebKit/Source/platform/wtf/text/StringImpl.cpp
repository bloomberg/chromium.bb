/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2013 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
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

#include "platform/wtf/text/StringImpl.h"

#include "platform/wtf/DynamicAnnotations.h"
#include "platform/wtf/LeakAnnotations.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/StaticConstructors.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/allocator/Partitions.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/AtomicStringTable.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/CharacterNames.h"
#include "platform/wtf/text/StringBuffer.h"
#include "platform/wtf/text/StringHash.h"
#include "platform/wtf/text/StringToNumber.h"
#include <algorithm>
#include <memory>

using namespace std;

namespace WTF {

using namespace Unicode;

// As of Jan 2017, StringImpl needs 2 * sizeof(int) + 29 bits of data, and
// sizeof(ThreadRestrictionVerifier) is 16 bytes. Thus, in DCHECK mode the
// class may be padded to 32 bytes.
#if DCHECK_IS_ON()
static_assert(sizeof(StringImpl) <= 8 * sizeof(int),
              "StringImpl should stay small");
#else
static_assert(sizeof(StringImpl) <= 3 * sizeof(int),
              "StringImpl should stay small");
#endif

void* StringImpl::operator new(size_t size) {
  DCHECK_EQ(size, sizeof(StringImpl));
  return Partitions::BufferMalloc(size, "WTF::StringImpl");
}

void StringImpl::operator delete(void* ptr) {
  Partitions::BufferFree(ptr);
}

inline StringImpl::~StringImpl() {
  DCHECK(!IsStatic());

  if (IsAtomic())
    AtomicStringTable::Instance().Remove(this);
}

void StringImpl::DestroyIfNotStatic() const {
  if (!IsStatic())
    delete this;
}

void StringImpl::UpdateContainsOnlyASCII() const {
  contains_only_ascii_ = Is8Bit()
                             ? CharactersAreAllASCII(Characters8(), length())
                             : CharactersAreAllASCII(Characters16(), length());
  needs_ascii_check_ = false;
}

bool StringImpl::IsSafeToSendToAnotherThread() const {
  if (IsStatic())
    return true;
  // AtomicStrings are not safe to send between threads as ~StringImpl()
  // will try to remove them from the wrong AtomicStringTable.
  if (IsAtomic())
    return false;
  if (HasOneRef())
    return true;
  return false;
}

#if DCHECK_IS_ON()
std::string StringImpl::AsciiForDebugging() const {
  CString ascii = String(IsolatedCopy()->Substring(0, 128)).Ascii();
  return std::string(ascii.data(), ascii.length());
}
#endif

RefPtr<StringImpl> StringImpl::CreateUninitialized(unsigned length,
                                                   LChar*& data) {
  if (!length) {
    data = 0;
    return empty_;
  }

  // Allocate a single buffer large enough to contain the StringImpl
  // struct as well as the data which it contains. This removes one
  // heap allocation from this call.
  StringImpl* string = static_cast<StringImpl*>(Partitions::BufferMalloc(
      AllocationSize<LChar>(length), "WTF::StringImpl"));

  data = reinterpret_cast<LChar*>(string + 1);
  return AdoptRef(new (string) StringImpl(length, kForce8BitConstructor));
}

RefPtr<StringImpl> StringImpl::CreateUninitialized(unsigned length,
                                                   UChar*& data) {
  if (!length) {
    data = 0;
    return empty_;
  }

  // Allocate a single buffer large enough to contain the StringImpl
  // struct as well as the data which it contains. This removes one
  // heap allocation from this call.
  StringImpl* string = static_cast<StringImpl*>(Partitions::BufferMalloc(
      AllocationSize<UChar>(length), "WTF::StringImpl"));

  data = reinterpret_cast<UChar*>(string + 1);
  return AdoptRef(new (string) StringImpl(length));
}

static StaticStringsTable& StaticStrings() {
  DEFINE_STATIC_LOCAL(StaticStringsTable, static_strings, ());
  return static_strings;
}

#if DCHECK_IS_ON()
static bool g_allow_creation_of_static_strings = true;
#endif

const StaticStringsTable& StringImpl::AllStaticStrings() {
  return StaticStrings();
}

void StringImpl::FreezeStaticStrings() {
  DCHECK(IsMainThread());

#if DCHECK_IS_ON()
  g_allow_creation_of_static_strings = false;
#endif
}

unsigned StringImpl::highest_static_string_length_ = 0;

DEFINE_GLOBAL(StringImpl, g_global_empty);
DEFINE_GLOBAL(StringImpl, g_global_empty16_bit);
// Callers need the global empty strings to be non-const.
StringImpl* StringImpl::empty_ = const_cast<StringImpl*>(&g_global_empty);
StringImpl* StringImpl::empty16_bit_ =
    const_cast<StringImpl*>(&g_global_empty16_bit);
void StringImpl::InitStatics() {
  new ((void*)empty_) StringImpl(kConstructEmptyString);
  new ((void*)empty16_bit_) StringImpl(kConstructEmptyString16Bit);
  WTF_ANNOTATE_BENIGN_RACE(StringImpl::empty_,
                           "Benign race on the reference counter of a static "
                           "string created by StringImpl::empty");
  WTF_ANNOTATE_BENIGN_RACE(StringImpl::empty16_bit_,
                           "Benign race on the reference counter of a static "
                           "string created by StringImpl::empty16Bit");
}

StringImpl* StringImpl::CreateStatic(const char* string,
                                     unsigned length,
                                     unsigned hash) {
#if DCHECK_IS_ON()
  DCHECK(g_allow_creation_of_static_strings);
#endif
  DCHECK(string);
  DCHECK(length);

  StaticStringsTable::const_iterator it = StaticStrings().find(hash);
  if (it != StaticStrings().end()) {
    DCHECK(!memcmp(string, it->value + 1, length * sizeof(LChar)));
    return it->value;
  }

  // Allocate a single buffer large enough to contain the StringImpl
  // struct as well as the data which it contains. This removes one
  // heap allocation from this call.
  CHECK_LE(length,
           ((std::numeric_limits<unsigned>::max() - sizeof(StringImpl)) /
            sizeof(LChar)));
  size_t size = sizeof(StringImpl) + length * sizeof(LChar);

  WTF_INTERNAL_LEAK_SANITIZER_DISABLED_SCOPE;
  StringImpl* impl = static_cast<StringImpl*>(
      Partitions::BufferMalloc(size, "WTF::StringImpl"));

  LChar* data = reinterpret_cast<LChar*>(impl + 1);
  impl = new (impl) StringImpl(length, hash, kStaticString);
  memcpy(data, string, length * sizeof(LChar));
#if DCHECK_IS_ON()
  impl->AssertHashIsCorrect();
#endif

  DCHECK(IsMainThread());
  highest_static_string_length_ =
      std::max(highest_static_string_length_, length);
  StaticStrings().insert(hash, impl);
  WTF_ANNOTATE_BENIGN_RACE(impl,
                           "Benign race on the reference counter of a static "
                           "string created by StringImpl::createStatic");

  return impl;
}

void StringImpl::ReserveStaticStringsCapacityForSize(unsigned size) {
#if DCHECK_IS_ON()
  DCHECK(g_allow_creation_of_static_strings);
#endif
  StaticStrings().ReserveCapacityForSize(size);
}

RefPtr<StringImpl> StringImpl::Create(const UChar* characters,
                                      unsigned length) {
  if (!characters || !length)
    return empty_;

  UChar* data;
  RefPtr<StringImpl> string = CreateUninitialized(length, data);
  memcpy(data, characters, length * sizeof(UChar));
  return string;
}

RefPtr<StringImpl> StringImpl::Create(const LChar* characters,
                                      unsigned length) {
  if (!characters || !length)
    return empty_;

  LChar* data;
  RefPtr<StringImpl> string = CreateUninitialized(length, data);
  memcpy(data, characters, length * sizeof(LChar));
  return string;
}

RefPtr<StringImpl> StringImpl::Create8BitIfPossible(const UChar* characters,
                                                    unsigned length) {
  if (!characters || !length)
    return empty_;

  LChar* data;
  RefPtr<StringImpl> string = CreateUninitialized(length, data);

  for (size_t i = 0; i < length; ++i) {
    if (characters[i] & 0xff00)
      return Create(characters, length);
    data[i] = static_cast<LChar>(characters[i]);
  }

  return string;
}

RefPtr<StringImpl> StringImpl::Create(const LChar* string) {
  if (!string)
    return empty_;
  size_t length = strlen(reinterpret_cast<const char*>(string));
  CHECK_LE(length, numeric_limits<unsigned>::max());
  return Create(string, length);
}

bool StringImpl::ContainsOnlyWhitespace() {
  // FIXME: The definition of whitespace here includes a number of characters
  // that are not whitespace from the point of view of LayoutText; I wonder if
  // that's a problem in practice.
  if (Is8Bit()) {
    for (unsigned i = 0; i < length_; ++i) {
      UChar c = Characters8()[i];
      if (!IsASCIISpace(c))
        return false;
    }

    return true;
  }

  for (unsigned i = 0; i < length_; ++i) {
    UChar c = Characters16()[i];
    if (!IsASCIISpace(c))
      return false;
  }
  return true;
}

RefPtr<StringImpl> StringImpl::Substring(unsigned start,
                                         unsigned length) const {
  if (start >= length_)
    return empty_;
  unsigned max_length = length_ - start;
  if (length >= max_length) {
    // RefPtr has trouble dealing with const arguments. It should be updated
    // so this const_cast is not necessary.
    if (!start)
      return const_cast<StringImpl*>(this);
    length = max_length;
  }
  if (Is8Bit())
    return Create(Characters8() + start, length);

  return Create(Characters16() + start, length);
}

UChar32 StringImpl::CharacterStartingAt(unsigned i) {
  if (Is8Bit())
    return Characters8()[i];
  if (U16_IS_SINGLE(Characters16()[i]))
    return Characters16()[i];
  if (i + 1 < length_ && U16_IS_LEAD(Characters16()[i]) &&
      U16_IS_TRAIL(Characters16()[i + 1]))
    return U16_GET_SUPPLEMENTARY(Characters16()[i], Characters16()[i + 1]);
  return 0;
}

unsigned StringImpl::CopyTo(UChar* buffer,
                            unsigned start,
                            unsigned max_length) const {
  unsigned number_of_characters_to_copy =
      std::min(length() - start, max_length);
  if (!number_of_characters_to_copy)
    return 0;
  if (Is8Bit())
    CopyChars(buffer, Characters8() + start, number_of_characters_to_copy);
  else
    CopyChars(buffer, Characters16() + start, number_of_characters_to_copy);
  return number_of_characters_to_copy;
}

RefPtr<StringImpl> StringImpl::LowerASCII() {
  // First scan the string for uppercase and non-ASCII characters:
  if (Is8Bit()) {
    unsigned first_index_to_be_lowered = length_;
    for (unsigned i = 0; i < length_; ++i) {
      LChar ch = Characters8()[i];
      if (IsASCIIUpper(ch)) {
        first_index_to_be_lowered = i;
        break;
      }
    }

    // Nothing to do if the string is all ASCII with no uppercase.
    if (first_index_to_be_lowered == length_) {
      return this;
    }

    LChar* data8;
    RefPtr<StringImpl> new_impl = CreateUninitialized(length_, data8);
    memcpy(data8, Characters8(), first_index_to_be_lowered);

    for (unsigned i = first_index_to_be_lowered; i < length_; ++i) {
      LChar ch = Characters8()[i];
      data8[i] = IsASCIIUpper(ch) ? ToASCIILower(ch) : ch;
    }
    return new_impl;
  }
  bool no_upper = true;
  UChar ored = 0;

  const UChar* end = Characters16() + length_;
  for (const UChar* chp = Characters16(); chp != end; ++chp) {
    if (IsASCIIUpper(*chp))
      no_upper = false;
    ored |= *chp;
  }
  // Nothing to do if the string is all ASCII with no uppercase.
  if (no_upper && !(ored & ~0x7F))
    return this;

  CHECK_LE(length_, static_cast<unsigned>(numeric_limits<unsigned>::max()));
  unsigned length = length_;

  UChar* data16;
  RefPtr<StringImpl> new_impl = CreateUninitialized(length_, data16);

  for (unsigned i = 0; i < length; ++i) {
    UChar c = Characters16()[i];
    data16[i] = IsASCIIUpper(c) ? ToASCIILower(c) : c;
  }
  return new_impl;
}

RefPtr<StringImpl> StringImpl::LowerUnicode() {
  // Note: This is a hot function in the Dromaeo benchmark, specifically the
  // no-op code path up through the first 'return' statement.

  // First scan the string for uppercase and non-ASCII characters:
  if (Is8Bit()) {
    unsigned first_index_to_be_lowered = length_;
    for (unsigned i = 0; i < length_; ++i) {
      LChar ch = Characters8()[i];
      if (UNLIKELY(IsASCIIUpper(ch) || ch & ~0x7F)) {
        first_index_to_be_lowered = i;
        break;
      }
    }

    // Nothing to do if the string is all ASCII with no uppercase.
    if (first_index_to_be_lowered == length_)
      return this;

    LChar* data8;
    RefPtr<StringImpl> new_impl = CreateUninitialized(length_, data8);
    memcpy(data8, Characters8(), first_index_to_be_lowered);

    for (unsigned i = first_index_to_be_lowered; i < length_; ++i) {
      LChar ch = Characters8()[i];
      data8[i] = UNLIKELY(ch & ~0x7F) ? static_cast<LChar>(Unicode::ToLower(ch))
                                      : ToASCIILower(ch);
    }

    return new_impl;
  }

  bool no_upper = true;
  UChar ored = 0;

  const UChar* end = Characters16() + length_;
  for (const UChar* chp = Characters16(); chp != end; ++chp) {
    if (UNLIKELY(IsASCIIUpper(*chp)))
      no_upper = false;
    ored |= *chp;
  }
  // Nothing to do if the string is all ASCII with no uppercase.
  if (no_upper && !(ored & ~0x7F))
    return this;

  CHECK_LE(length_, static_cast<unsigned>(numeric_limits<int32_t>::max()));
  int32_t length = length_;

  if (!(ored & ~0x7F)) {
    UChar* data16;
    RefPtr<StringImpl> new_impl = CreateUninitialized(length_, data16);

    for (int32_t i = 0; i < length; ++i) {
      UChar c = Characters16()[i];
      data16[i] = ToASCIILower(c);
    }
    return new_impl;
  }

  // Do a slower implementation for cases that include non-ASCII characters.
  UChar* data16;
  RefPtr<StringImpl> new_impl = CreateUninitialized(length_, data16);

  bool error;
  int32_t real_length =
      Unicode::ToLower(data16, length, Characters16(), length_, &error);
  if (!error && real_length == length)
    return new_impl;

  new_impl = CreateUninitialized(real_length, data16);
  Unicode::ToLower(data16, real_length, Characters16(), length_, &error);
  if (error)
    return this;
  return new_impl;
}

RefPtr<StringImpl> StringImpl::UpperUnicode() {
  // This function could be optimized for no-op cases the way LowerUnicode() is,
  // but in empirical testing, few actual calls to UpperUnicode() are no-ops, so
  // it wouldn't be worth the extra time for pre-scanning.

  CHECK_LE(length_, static_cast<unsigned>(numeric_limits<int32_t>::max()));
  int32_t length = length_;

  if (Is8Bit()) {
    LChar* data8;
    RefPtr<StringImpl> new_impl = CreateUninitialized(length_, data8);

    // Do a faster loop for the case where all the characters are ASCII.
    LChar ored = 0;
    for (int i = 0; i < length; ++i) {
      LChar c = Characters8()[i];
      ored |= c;
      data8[i] = ToASCIIUpper(c);
    }
    if (!(ored & ~0x7F))
      return new_impl;

    // Do a slower implementation for cases that include non-ASCII Latin-1
    // characters.
    int number_sharp_s_characters = 0;

    // There are two special cases.
    //  1. latin-1 characters when converted to upper case are 16 bit
    //     characters.
    //  2. Lower case sharp-S converts to "SS" (two characters)
    for (int32_t i = 0; i < length; ++i) {
      LChar c = Characters8()[i];
      if (UNLIKELY(c == kSmallLetterSharpSCharacter))
        ++number_sharp_s_characters;
      UChar upper = static_cast<UChar>(Unicode::ToUpper(c));
      if (UNLIKELY(upper > 0xff)) {
        // Since this upper-cased character does not fit in an 8-bit string, we
        // need to take the 16-bit path.
        goto upconvert;
      }
      data8[i] = static_cast<LChar>(upper);
    }

    if (!number_sharp_s_characters)
      return new_impl;

    // We have numberSSCharacters sharp-s characters, but none of the other
    // special characters.
    new_impl = CreateUninitialized(length_ + number_sharp_s_characters, data8);

    LChar* dest = data8;

    for (int32_t i = 0; i < length; ++i) {
      LChar c = Characters8()[i];
      if (c == kSmallLetterSharpSCharacter) {
        *dest++ = 'S';
        *dest++ = 'S';
      } else {
        *dest++ = static_cast<LChar>(Unicode::ToUpper(c));
      }
    }

    return new_impl;
  }

upconvert:
  RefPtr<StringImpl> upconverted = UpconvertedString();
  const UChar* source16 = upconverted->Characters16();

  UChar* data16;
  RefPtr<StringImpl> new_impl = CreateUninitialized(length_, data16);

  // Do a faster loop for the case where all the characters are ASCII.
  UChar ored = 0;
  for (int i = 0; i < length; ++i) {
    UChar c = source16[i];
    ored |= c;
    data16[i] = ToASCIIUpper(c);
  }
  if (!(ored & ~0x7F))
    return new_impl;

  // Do a slower implementation for cases that include non-ASCII characters.
  bool error;
  int32_t real_length =
      Unicode::ToUpper(data16, length, source16, length_, &error);
  if (!error && real_length == length)
    return new_impl;
  new_impl = CreateUninitialized(real_length, data16);
  Unicode::ToUpper(data16, real_length, source16, length_, &error);
  if (error)
    return this;
  return new_impl;
}

RefPtr<StringImpl> StringImpl::UpperASCII() {
  if (Is8Bit()) {
    LChar* data8;
    RefPtr<StringImpl> new_impl = CreateUninitialized(length_, data8);

    for (unsigned i = 0; i < length_; ++i) {
      LChar c = Characters8()[i];
      data8[i] = IsASCIILower(c) ? ToASCIIUpper(c) : c;
    }
    return new_impl;
  }

  UChar* data16;
  RefPtr<StringImpl> new_impl = CreateUninitialized(length_, data16);

  for (unsigned i = 0; i < length_; ++i) {
    UChar c = Characters16()[i];
    data16[i] = IsASCIILower(c) ? ToASCIIUpper(c) : c;
  }
  return new_impl;
}

static inline bool LocaleIdMatchesLang(const AtomicString& locale_id,
                                       const StringView& lang) {
  CHECK_GE(lang.length(), 2u);
  CHECK_LE(lang.length(), 3u);
  if (!locale_id.Impl() || !locale_id.Impl()->StartsWithIgnoringCase(lang))
    return false;
  if (locale_id.Impl()->length() == lang.length())
    return true;
  const UChar maybe_delimiter = (*locale_id.Impl())[lang.length()];
  return maybe_delimiter == '-' || maybe_delimiter == '_' ||
         maybe_delimiter == '@';
}

typedef int32_t (*icuCaseConverter)(UChar*,
                                    int32_t,
                                    const UChar*,
                                    int32_t,
                                    const char*,
                                    UErrorCode*);

static RefPtr<StringImpl> CaseConvert(const UChar* source16,
                                      size_t length,
                                      icuCaseConverter converter,
                                      const char* locale,
                                      StringImpl* original_string) {
  UChar* data16;
  size_t target_length = length;
  RefPtr<StringImpl> output = StringImpl::CreateUninitialized(length, data16);
  do {
    UErrorCode status = U_ZERO_ERROR;
    target_length =
        converter(data16, target_length, source16, length, locale, &status);
    if (U_SUCCESS(status)) {
      if (length > 0)
        return output->Substring(0, target_length);
      return output;
    }
    if (status != U_BUFFER_OVERFLOW_ERROR)
      return original_string;
    // Expand the buffer.
    output = StringImpl::CreateUninitialized(target_length, data16);
  } while (true);
}

RefPtr<StringImpl> StringImpl::LowerUnicode(
    const AtomicString& locale_identifier) {
  // Use the more optimized code path most of the time.
  // Only Turkic (tr and az) languages and Lithuanian requires
  // locale-specific lowercasing rules. Even though CLDR has el-Lower,
  // it's identical to the locale-agnostic lowercasing. Context-dependent
  // handling of Greek capital sigma is built into the common lowercasing
  // function in ICU.
  const char* locale_for_conversion = 0;
  if (LocaleIdMatchesLang(locale_identifier, "tr") ||
      LocaleIdMatchesLang(locale_identifier, "az"))
    locale_for_conversion = "tr";
  else if (LocaleIdMatchesLang(locale_identifier, "lt"))
    locale_for_conversion = "lt";
  else
    return LowerUnicode();

  CHECK_LE(length_, static_cast<unsigned>(numeric_limits<int32_t>::max()));
  int length = length_;

  RefPtr<StringImpl> upconverted = UpconvertedString();
  const UChar* source16 = upconverted->Characters16();
  return CaseConvert(source16, length, u_strToLower, locale_for_conversion,
                     this);
}

RefPtr<StringImpl> StringImpl::UpperUnicode(
    const AtomicString& locale_identifier) {
  // Use the more-optimized code path most of the time.
  // Only Turkic (tr and az) languages, Greek and Lithuanian require
  // locale-specific uppercasing rules.
  const char* locale_for_conversion = 0;
  if (LocaleIdMatchesLang(locale_identifier, "tr") ||
      LocaleIdMatchesLang(locale_identifier, "az"))
    locale_for_conversion = "tr";
  else if (LocaleIdMatchesLang(locale_identifier, "el"))
    locale_for_conversion = "el";
  else if (LocaleIdMatchesLang(locale_identifier, "lt"))
    locale_for_conversion = "lt";
  else
    return UpperUnicode();

  CHECK_LE(length_, static_cast<unsigned>(numeric_limits<int32_t>::max()));
  int length = length_;

  RefPtr<StringImpl> upconverted = UpconvertedString();
  const UChar* source16 = upconverted->Characters16();

  return CaseConvert(source16, length, u_strToUpper, locale_for_conversion,
                     this);
}

RefPtr<StringImpl> StringImpl::Fill(UChar character) {
  if (!(character & ~0x7F)) {
    LChar* data;
    RefPtr<StringImpl> new_impl = CreateUninitialized(length_, data);
    for (unsigned i = 0; i < length_; ++i)
      data[i] = static_cast<LChar>(character);
    return new_impl;
  }
  UChar* data;
  RefPtr<StringImpl> new_impl = CreateUninitialized(length_, data);
  for (unsigned i = 0; i < length_; ++i)
    data[i] = character;
  return new_impl;
}

RefPtr<StringImpl> StringImpl::FoldCase() {
  CHECK_LE(length_, static_cast<unsigned>(numeric_limits<int32_t>::max()));
  int32_t length = length_;

  if (Is8Bit()) {
    // Do a faster loop for the case where all the characters are ASCII.
    LChar* data;
    RefPtr<StringImpl> new_impl = CreateUninitialized(length_, data);
    LChar ored = 0;

    for (int32_t i = 0; i < length; ++i) {
      LChar c = Characters8()[i];
      data[i] = ToASCIILower(c);
      ored |= c;
    }

    if (!(ored & ~0x7F))
      return new_impl;

    // Do a slower implementation for cases that include non-ASCII Latin-1
    // characters.
    for (int32_t i = 0; i < length; ++i)
      data[i] = static_cast<LChar>(Unicode::ToLower(Characters8()[i]));

    return new_impl;
  }

  // Do a faster loop for the case where all the characters are ASCII.
  UChar* data;
  RefPtr<StringImpl> new_impl = CreateUninitialized(length_, data);
  UChar ored = 0;
  for (int32_t i = 0; i < length; ++i) {
    UChar c = Characters16()[i];
    ored |= c;
    data[i] = ToASCIILower(c);
  }
  if (!(ored & ~0x7F))
    return new_impl;

  // Do a slower implementation for cases that include non-ASCII characters.
  bool error;
  int32_t real_length =
      Unicode::FoldCase(data, length, Characters16(), length_, &error);
  if (!error && real_length == length)
    return new_impl;
  new_impl = CreateUninitialized(real_length, data);
  Unicode::FoldCase(data, real_length, Characters16(), length_, &error);
  if (error)
    return this;
  return new_impl;
}

RefPtr<StringImpl> StringImpl::Truncate(unsigned length) {
  if (length >= length_)
    return this;
  if (Is8Bit())
    return Create(Characters8(), length);
  return Create(Characters16(), length);
}

template <class UCharPredicate>
inline RefPtr<StringImpl> StringImpl::StripMatchedCharacters(
    UCharPredicate predicate) {
  if (!length_)
    return empty_;

  unsigned start = 0;
  unsigned end = length_ - 1;

  // skip white space from start
  while (start <= end &&
         predicate(Is8Bit() ? Characters8()[start] : Characters16()[start]))
    ++start;

  // only white space
  if (start > end)
    return empty_;

  // skip white space from end
  while (end && predicate(Is8Bit() ? Characters8()[end] : Characters16()[end]))
    --end;

  if (!start && end == length_ - 1)
    return this;
  if (Is8Bit())
    return Create(Characters8() + start, end + 1 - start);
  return Create(Characters16() + start, end + 1 - start);
}

class UCharPredicate final {
  STACK_ALLOCATED();

 public:
  inline UCharPredicate(CharacterMatchFunctionPtr function)
      : function_(function) {}

  inline bool operator()(UChar ch) const { return function_(ch); }

 private:
  const CharacterMatchFunctionPtr function_;
};

class SpaceOrNewlinePredicate final {
  STACK_ALLOCATED();

 public:
  inline bool operator()(UChar ch) const { return IsSpaceOrNewline(ch); }
};

RefPtr<StringImpl> StringImpl::StripWhiteSpace() {
  return StripMatchedCharacters(SpaceOrNewlinePredicate());
}

RefPtr<StringImpl> StringImpl::StripWhiteSpace(
    IsWhiteSpaceFunctionPtr is_white_space) {
  return StripMatchedCharacters(UCharPredicate(is_white_space));
}

template <typename CharType>
ALWAYS_INLINE RefPtr<StringImpl> StringImpl::RemoveCharacters(
    const CharType* characters,
    CharacterMatchFunctionPtr find_match) {
  const CharType* from = characters;
  const CharType* fromend = from + length_;

  // Assume the common case will not remove any characters
  while (from != fromend && !find_match(*from))
    ++from;
  if (from == fromend)
    return this;

  StringBuffer<CharType> data(length_);
  CharType* to = data.Characters();
  unsigned outc = from - characters;

  if (outc)
    memcpy(to, characters, outc * sizeof(CharType));

  while (true) {
    while (from != fromend && find_match(*from))
      ++from;
    while (from != fromend && !find_match(*from))
      to[outc++] = *from++;
    if (from == fromend)
      break;
  }

  data.Shrink(outc);

  return data.Release();
}

RefPtr<StringImpl> StringImpl::RemoveCharacters(
    CharacterMatchFunctionPtr find_match) {
  if (Is8Bit())
    return RemoveCharacters(Characters8(), find_match);
  return RemoveCharacters(Characters16(), find_match);
}

RefPtr<StringImpl> StringImpl::Remove(unsigned start,
                                      unsigned length_to_remove) {
  if (length_to_remove <= 0)
    return this;
  if (start >= length_)
    return this;

  length_to_remove = std::min(length_ - start, length_to_remove);
  unsigned removed_end = start + length_to_remove;

  if (Is8Bit()) {
    StringBuffer<LChar> buffer(length_ - length_to_remove);
    CopyChars(buffer.Characters(), Characters8(), start);
    CopyChars(buffer.Characters() + start, Characters8() + removed_end,
              length_ - removed_end);
    return buffer.Release();
  }
  StringBuffer<UChar> buffer(length_ - length_to_remove);
  CopyChars(buffer.Characters(), Characters16(), start);
  CopyChars(buffer.Characters() + start, Characters16() + removed_end,
            length_ - removed_end);
  return buffer.Release();
}

template <typename CharType, class UCharPredicate>
inline RefPtr<StringImpl> StringImpl::SimplifyMatchedCharactersToSpace(
    UCharPredicate predicate,
    StripBehavior strip_behavior) {
  StringBuffer<CharType> data(length_);

  const CharType* from = GetCharacters<CharType>();
  const CharType* fromend = from + length_;
  int outc = 0;
  bool changed_to_space = false;

  CharType* to = data.Characters();

  if (strip_behavior == kStripExtraWhiteSpace) {
    while (true) {
      while (from != fromend && predicate(*from)) {
        if (*from != ' ')
          changed_to_space = true;
        ++from;
      }
      while (from != fromend && !predicate(*from))
        to[outc++] = *from++;
      if (from != fromend)
        to[outc++] = ' ';
      else
        break;
    }

    if (outc > 0 && to[outc - 1] == ' ')
      --outc;
  } else {
    for (; from != fromend; ++from) {
      if (predicate(*from)) {
        if (*from != ' ')
          changed_to_space = true;
        to[outc++] = ' ';
      } else {
        to[outc++] = *from;
      }
    }
  }

  if (static_cast<unsigned>(outc) == length_ && !changed_to_space)
    return this;

  data.Shrink(outc);

  return data.Release();
}

RefPtr<StringImpl> StringImpl::SimplifyWhiteSpace(
    StripBehavior strip_behavior) {
  if (Is8Bit())
    return StringImpl::SimplifyMatchedCharactersToSpace<LChar>(
        SpaceOrNewlinePredicate(), strip_behavior);
  return StringImpl::SimplifyMatchedCharactersToSpace<UChar>(
      SpaceOrNewlinePredicate(), strip_behavior);
}

RefPtr<StringImpl> StringImpl::SimplifyWhiteSpace(
    IsWhiteSpaceFunctionPtr is_white_space,
    StripBehavior strip_behavior) {
  if (Is8Bit())
    return StringImpl::SimplifyMatchedCharactersToSpace<LChar>(
        UCharPredicate(is_white_space), strip_behavior);
  return StringImpl::SimplifyMatchedCharactersToSpace<UChar>(
      UCharPredicate(is_white_space), strip_behavior);
}

int StringImpl::ToIntStrict(bool* ok) {
  if (Is8Bit())
    return CharactersToIntStrict(Characters8(), length_, ok);
  return CharactersToIntStrict(Characters16(), length_, ok);
}

unsigned StringImpl::ToUIntStrict(bool* ok) {
  if (Is8Bit())
    return CharactersToUIntStrict(Characters8(), length_, ok);
  return CharactersToUIntStrict(Characters16(), length_, ok);
}

unsigned StringImpl::HexToUIntStrict(bool* ok) {
  if (Is8Bit())
    return HexCharactersToUIntStrict(Characters8(), length_, ok);
  return HexCharactersToUIntStrict(Characters16(), length_, ok);
}

int64_t StringImpl::ToInt64Strict(bool* ok) {
  if (Is8Bit())
    return CharactersToInt64Strict(Characters8(), length_, ok);
  return CharactersToInt64Strict(Characters16(), length_, ok);
}

uint64_t StringImpl::ToUInt64Strict(bool* ok) {
  if (Is8Bit())
    return CharactersToUInt64Strict(Characters8(), length_, ok);
  return CharactersToUInt64Strict(Characters16(), length_, ok);
}

int StringImpl::ToInt(bool* ok) {
  if (Is8Bit())
    return CharactersToInt(Characters8(), length_, ok);
  return CharactersToInt(Characters16(), length_, ok);
}

unsigned StringImpl::ToUInt(bool* ok) {
  if (Is8Bit())
    return CharactersToUInt(Characters8(), length_, ok);
  return CharactersToUInt(Characters16(), length_, ok);
}

int64_t StringImpl::ToInt64(bool* ok) {
  if (Is8Bit())
    return CharactersToInt64(Characters8(), length_, ok);
  return CharactersToInt64(Characters16(), length_, ok);
}

uint64_t StringImpl::ToUInt64(bool* ok) {
  if (Is8Bit())
    return CharactersToUInt64(Characters8(), length_, ok);
  return CharactersToUInt64(Characters16(), length_, ok);
}

double StringImpl::ToDouble(bool* ok) {
  if (Is8Bit())
    return CharactersToDouble(Characters8(), length_, ok);
  return CharactersToDouble(Characters16(), length_, ok);
}

float StringImpl::ToFloat(bool* ok) {
  if (Is8Bit())
    return CharactersToFloat(Characters8(), length_, ok);
  return CharactersToFloat(Characters16(), length_, ok);
}

// Table is based on ftp://ftp.unicode.org/Public/UNIDATA/CaseFolding.txt
const UChar StringImpl::kLatin1CaseFoldTable[256] = {
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008,
    0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e, 0x000f, 0x0010, 0x0011,
    0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x001a,
    0x001b, 0x001c, 0x001d, 0x001e, 0x001f, 0x0020, 0x0021, 0x0022, 0x0023,
    0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c,
    0x002d, 0x002e, 0x002f, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035,
    0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e,
    0x003f, 0x0040, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
    0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f, 0x0070,
    0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079,
    0x007a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f, 0x0060, 0x0061, 0x0062,
    0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006a, 0x006b,
    0x006c, 0x006d, 0x006e, 0x006f, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074,
    0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d,
    0x007e, 0x007f, 0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086,
    0x0087, 0x0088, 0x0089, 0x008a, 0x008b, 0x008c, 0x008d, 0x008e, 0x008f,
    0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097, 0x0098,
    0x0099, 0x009a, 0x009b, 0x009c, 0x009d, 0x009e, 0x009f, 0x00a0, 0x00a1,
    0x00a2, 0x00a3, 0x00a4, 0x00a5, 0x00a6, 0x00a7, 0x00a8, 0x00a9, 0x00aa,
    0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x00af, 0x00b0, 0x00b1, 0x00b2, 0x00b3,
    0x00b4, 0x03bc, 0x00b6, 0x00b7, 0x00b8, 0x00b9, 0x00ba, 0x00bb, 0x00bc,
    0x00bd, 0x00be, 0x00bf, 0x00e0, 0x00e1, 0x00e2, 0x00e3, 0x00e4, 0x00e5,
    0x00e6, 0x00e7, 0x00e8, 0x00e9, 0x00ea, 0x00eb, 0x00ec, 0x00ed, 0x00ee,
    0x00ef, 0x00f0, 0x00f1, 0x00f2, 0x00f3, 0x00f4, 0x00f5, 0x00f6, 0x00d7,
    0x00f8, 0x00f9, 0x00fa, 0x00fb, 0x00fc, 0x00fd, 0x00fe, 0x00df, 0x00e0,
    0x00e1, 0x00e2, 0x00e3, 0x00e4, 0x00e5, 0x00e6, 0x00e7, 0x00e8, 0x00e9,
    0x00ea, 0x00eb, 0x00ec, 0x00ed, 0x00ee, 0x00ef, 0x00f0, 0x00f1, 0x00f2,
    0x00f3, 0x00f4, 0x00f5, 0x00f6, 0x00f7, 0x00f8, 0x00f9, 0x00fa, 0x00fb,
    0x00fc, 0x00fd, 0x00fe, 0x00ff,
};

bool DeprecatedEqualIgnoringCase(const LChar* a,
                                 const LChar* b,
                                 unsigned length) {
  DCHECK_GE(length, 0u);
  if (a == b)
    return true;
  while (length--) {
    if (StringImpl::kLatin1CaseFoldTable[*a++] !=
        StringImpl::kLatin1CaseFoldTable[*b++])
      return false;
  }
  return true;
}

bool DeprecatedEqualIgnoringCase(const UChar* a,
                                 const UChar* b,
                                 unsigned length) {
  DCHECK_GE(length, 0u);
  if (a == b)
    return true;
  return !Unicode::Umemcasecmp(a, b, length);
}

bool DeprecatedEqualIgnoringCase(const UChar* a,
                                 const LChar* b,
                                 unsigned length) {
  while (length--) {
    if (FoldCase(*a++) != StringImpl::kLatin1CaseFoldTable[*b++])
      return false;
  }
  return true;
}

size_t StringImpl::Find(CharacterMatchFunctionPtr match_function,
                        unsigned start) {
  if (Is8Bit())
    return WTF::Find(Characters8(), length_, match_function, start);
  return WTF::Find(Characters16(), length_, match_function, start);
}

template <typename SearchCharacterType, typename MatchCharacterType>
ALWAYS_INLINE static size_t FindInternal(
    const SearchCharacterType* search_characters,
    const MatchCharacterType* match_characters,
    unsigned index,
    unsigned search_length,
    unsigned match_length) {
  // Optimization: keep a running hash of the strings,
  // only call equal() if the hashes match.

  // delta is the number of additional times to test; delta == 0 means test only
  // once.
  unsigned delta = search_length - match_length;

  unsigned search_hash = 0;
  unsigned match_hash = 0;

  for (unsigned i = 0; i < match_length; ++i) {
    search_hash += search_characters[i];
    match_hash += match_characters[i];
  }

  unsigned i = 0;
  // keep looping until we match
  while (search_hash != match_hash ||
         !Equal(search_characters + i, match_characters, match_length)) {
    if (i == delta)
      return kNotFound;
    search_hash += search_characters[i + match_length];
    search_hash -= search_characters[i];
    ++i;
  }
  return index + i;
}

size_t StringImpl::Find(const StringView& match_string, unsigned index) {
  if (UNLIKELY(match_string.IsNull()))
    return kNotFound;

  unsigned match_length = match_string.length();

  // Optimization 1: fast case for strings of length 1.
  if (match_length == 1) {
    if (Is8Bit())
      return WTF::Find(Characters8(), length(), match_string[0], index);
    return WTF::Find(Characters16(), length(), match_string[0], index);
  }

  if (UNLIKELY(!match_length))
    return min(index, length());

  // Check index & matchLength are in range.
  if (index > length())
    return kNotFound;
  unsigned search_length = length() - index;
  if (match_length > search_length)
    return kNotFound;

  if (Is8Bit()) {
    if (match_string.Is8Bit())
      return FindInternal(Characters8() + index, match_string.Characters8(),
                          index, search_length, match_length);
    return FindInternal(Characters8() + index, match_string.Characters16(),
                        index, search_length, match_length);
  }
  if (match_string.Is8Bit())
    return FindInternal(Characters16() + index, match_string.Characters8(),
                        index, search_length, match_length);
  return FindInternal(Characters16() + index, match_string.Characters16(),
                      index, search_length, match_length);
}

template <typename SearchCharacterType, typename MatchCharacterType>
ALWAYS_INLINE static size_t FindIgnoringCaseInternal(
    const SearchCharacterType* search_characters,
    const MatchCharacterType* match_characters,
    unsigned index,
    unsigned search_length,
    unsigned match_length) {
  // delta is the number of additional times to test; delta == 0 means test only
  // once.
  unsigned delta = search_length - match_length;

  unsigned i = 0;
  // keep looping until we match
  while (!DeprecatedEqualIgnoringCase(search_characters + i, match_characters,
                                      match_length)) {
    if (i == delta)
      return kNotFound;
    ++i;
  }
  return index + i;
}

size_t StringImpl::FindIgnoringCase(const StringView& match_string,
                                    unsigned index) {
  if (UNLIKELY(match_string.IsNull()))
    return kNotFound;

  unsigned match_length = match_string.length();
  if (!match_length)
    return min(index, length());

  // Check index & matchLength are in range.
  if (index > length())
    return kNotFound;
  unsigned search_length = length() - index;
  if (match_length > search_length)
    return kNotFound;

  if (Is8Bit()) {
    if (match_string.Is8Bit())
      return FindIgnoringCaseInternal(Characters8() + index,
                                      match_string.Characters8(), index,
                                      search_length, match_length);
    return FindIgnoringCaseInternal(Characters8() + index,
                                    match_string.Characters16(), index,
                                    search_length, match_length);
  }
  if (match_string.Is8Bit())
    return FindIgnoringCaseInternal(Characters16() + index,
                                    match_string.Characters8(), index,
                                    search_length, match_length);
  return FindIgnoringCaseInternal(Characters16() + index,
                                  match_string.Characters16(), index,
                                  search_length, match_length);
}

template <typename SearchCharacterType, typename MatchCharacterType>
ALWAYS_INLINE static size_t FindIgnoringASCIICaseInternal(
    const SearchCharacterType* search_characters,
    const MatchCharacterType* match_characters,
    unsigned index,
    unsigned search_length,
    unsigned match_length) {
  // delta is the number of additional times to test; delta == 0 means test only
  // once.
  unsigned delta = search_length - match_length;

  unsigned i = 0;
  // keep looping until we match
  while (!EqualIgnoringASCIICase(search_characters + i, match_characters,
                                 match_length)) {
    if (i == delta)
      return kNotFound;
    ++i;
  }
  return index + i;
}

size_t StringImpl::FindIgnoringASCIICase(const StringView& match_string,
                                         unsigned index) {
  if (UNLIKELY(match_string.IsNull()))
    return kNotFound;

  unsigned match_length = match_string.length();
  if (!match_length)
    return min(index, length());

  // Check index & matchLength are in range.
  if (index > length())
    return kNotFound;
  unsigned search_length = length() - index;
  if (match_length > search_length)
    return kNotFound;

  if (Is8Bit()) {
    if (match_string.Is8Bit())
      return FindIgnoringASCIICaseInternal(Characters8() + index,
                                           match_string.Characters8(), index,
                                           search_length, match_length);
    return FindIgnoringASCIICaseInternal(Characters8() + index,
                                         match_string.Characters16(), index,
                                         search_length, match_length);
  }
  if (match_string.Is8Bit())
    return FindIgnoringASCIICaseInternal(Characters16() + index,
                                         match_string.Characters8(), index,
                                         search_length, match_length);
  return FindIgnoringASCIICaseInternal(Characters16() + index,
                                       match_string.Characters16(), index,
                                       search_length, match_length);
}

size_t StringImpl::ReverseFind(UChar c, unsigned index) {
  if (Is8Bit())
    return WTF::ReverseFind(Characters8(), length_, c, index);
  return WTF::ReverseFind(Characters16(), length_, c, index);
}

template <typename SearchCharacterType, typename MatchCharacterType>
ALWAYS_INLINE static size_t ReverseFindInternal(
    const SearchCharacterType* search_characters,
    const MatchCharacterType* match_characters,
    unsigned index,
    unsigned length,
    unsigned match_length) {
  // Optimization: keep a running hash of the strings,
  // only call equal if the hashes match.

  // delta is the number of additional times to test; delta == 0 means test only
  // once.
  unsigned delta = min(index, length - match_length);

  unsigned search_hash = 0;
  unsigned match_hash = 0;
  for (unsigned i = 0; i < match_length; ++i) {
    search_hash += search_characters[delta + i];
    match_hash += match_characters[i];
  }

  // keep looping until we match
  while (search_hash != match_hash ||
         !Equal(search_characters + delta, match_characters, match_length)) {
    if (!delta)
      return kNotFound;
    --delta;
    search_hash -= search_characters[delta + match_length];
    search_hash += search_characters[delta];
  }
  return delta;
}

size_t StringImpl::ReverseFind(const StringView& match_string, unsigned index) {
  if (UNLIKELY(match_string.IsNull()))
    return kNotFound;

  unsigned match_length = match_string.length();
  unsigned our_length = length();
  if (!match_length)
    return min(index, our_length);

  // Optimization 1: fast case for strings of length 1.
  if (match_length == 1) {
    if (Is8Bit())
      return WTF::ReverseFind(Characters8(), our_length, match_string[0],
                              index);
    return WTF::ReverseFind(Characters16(), our_length, match_string[0], index);
  }

  // Check index & matchLength are in range.
  if (match_length > our_length)
    return kNotFound;

  if (Is8Bit()) {
    if (match_string.Is8Bit())
      return ReverseFindInternal(Characters8(), match_string.Characters8(),
                                 index, our_length, match_length);
    return ReverseFindInternal(Characters8(), match_string.Characters16(),
                               index, our_length, match_length);
  }
  if (match_string.Is8Bit())
    return ReverseFindInternal(Characters16(), match_string.Characters8(),
                               index, our_length, match_length);
  return ReverseFindInternal(Characters16(), match_string.Characters16(), index,
                             our_length, match_length);
}

bool StringImpl::StartsWith(UChar character) const {
  return length_ && (*this)[0] == character;
}

bool StringImpl::StartsWith(const StringView& prefix) const {
  if (prefix.length() > length())
    return false;
  if (Is8Bit()) {
    if (prefix.Is8Bit())
      return Equal(Characters8(), prefix.Characters8(), prefix.length());
    return Equal(Characters8(), prefix.Characters16(), prefix.length());
  }
  if (prefix.Is8Bit())
    return Equal(Characters16(), prefix.Characters8(), prefix.length());
  return Equal(Characters16(), prefix.Characters16(), prefix.length());
}

bool StringImpl::StartsWithIgnoringCase(const StringView& prefix) const {
  if (prefix.length() > length())
    return false;
  if (Is8Bit()) {
    if (prefix.Is8Bit()) {
      return DeprecatedEqualIgnoringCase(Characters8(), prefix.Characters8(),
                                         prefix.length());
    }
    return DeprecatedEqualIgnoringCase(Characters8(), prefix.Characters16(),
                                       prefix.length());
  }
  if (prefix.Is8Bit()) {
    return DeprecatedEqualIgnoringCase(Characters16(), prefix.Characters8(),
                                       prefix.length());
  }
  return DeprecatedEqualIgnoringCase(Characters16(), prefix.Characters16(),
                                     prefix.length());
}

bool StringImpl::StartsWithIgnoringASCIICase(const StringView& prefix) const {
  if (prefix.length() > length())
    return false;
  if (Is8Bit()) {
    if (prefix.Is8Bit())
      return EqualIgnoringASCIICase(Characters8(), prefix.Characters8(),
                                    prefix.length());
    return EqualIgnoringASCIICase(Characters8(), prefix.Characters16(),
                                  prefix.length());
  }
  if (prefix.Is8Bit())
    return EqualIgnoringASCIICase(Characters16(), prefix.Characters8(),
                                  prefix.length());
  return EqualIgnoringASCIICase(Characters16(), prefix.Characters16(),
                                prefix.length());
}

bool StringImpl::EndsWith(UChar character) const {
  return length_ && (*this)[length_ - 1] == character;
}

bool StringImpl::EndsWith(const StringView& suffix) const {
  if (suffix.length() > length())
    return false;
  unsigned start_offset = length() - suffix.length();
  if (Is8Bit()) {
    if (suffix.Is8Bit())
      return Equal(Characters8() + start_offset, suffix.Characters8(),
                   suffix.length());
    return Equal(Characters8() + start_offset, suffix.Characters16(),
                 suffix.length());
  }
  if (suffix.Is8Bit())
    return Equal(Characters16() + start_offset, suffix.Characters8(),
                 suffix.length());
  return Equal(Characters16() + start_offset, suffix.Characters16(),
               suffix.length());
}

bool StringImpl::EndsWithIgnoringCase(const StringView& suffix) const {
  if (suffix.length() > length())
    return false;
  unsigned start_offset = length() - suffix.length();
  if (Is8Bit()) {
    if (suffix.Is8Bit()) {
      return DeprecatedEqualIgnoringCase(Characters8() + start_offset,
                                         suffix.Characters8(), suffix.length());
    }
    return DeprecatedEqualIgnoringCase(Characters8() + start_offset,
                                       suffix.Characters16(), suffix.length());
  }
  if (suffix.Is8Bit()) {
    return DeprecatedEqualIgnoringCase(Characters16() + start_offset,
                                       suffix.Characters8(), suffix.length());
  }
  return DeprecatedEqualIgnoringCase(Characters16() + start_offset,
                                     suffix.Characters16(), suffix.length());
}

bool StringImpl::EndsWithIgnoringASCIICase(const StringView& suffix) const {
  if (suffix.length() > length())
    return false;
  unsigned start_offset = length() - suffix.length();
  if (Is8Bit()) {
    if (suffix.Is8Bit())
      return EqualIgnoringASCIICase(Characters8() + start_offset,
                                    suffix.Characters8(), suffix.length());
    return EqualIgnoringASCIICase(Characters8() + start_offset,
                                  suffix.Characters16(), suffix.length());
  }
  if (suffix.Is8Bit())
    return EqualIgnoringASCIICase(Characters16() + start_offset,
                                  suffix.Characters8(), suffix.length());
  return EqualIgnoringASCIICase(Characters16() + start_offset,
                                suffix.Characters16(), suffix.length());
}

RefPtr<StringImpl> StringImpl::Replace(UChar old_c, UChar new_c) {
  if (old_c == new_c)
    return this;

  if (Find(old_c) == kNotFound)
    return this;

  unsigned i;
  if (Is8Bit()) {
    if (new_c <= 0xff) {
      LChar* data;
      LChar old_char = static_cast<LChar>(old_c);
      LChar new_char = static_cast<LChar>(new_c);

      RefPtr<StringImpl> new_impl = CreateUninitialized(length_, data);

      for (i = 0; i != length_; ++i) {
        LChar ch = Characters8()[i];
        if (ch == old_char)
          ch = new_char;
        data[i] = ch;
      }
      return new_impl;
    }

    // There is the possibility we need to up convert from 8 to 16 bit,
    // create a 16 bit string for the result.
    UChar* data;
    RefPtr<StringImpl> new_impl = CreateUninitialized(length_, data);

    for (i = 0; i != length_; ++i) {
      UChar ch = Characters8()[i];
      if (ch == old_c)
        ch = new_c;
      data[i] = ch;
    }

    return new_impl;
  }

  UChar* data;
  RefPtr<StringImpl> new_impl = CreateUninitialized(length_, data);

  for (i = 0; i != length_; ++i) {
    UChar ch = Characters16()[i];
    if (ch == old_c)
      ch = new_c;
    data[i] = ch;
  }
  return new_impl;
}

// TODO(esprehn): Passing a null replacement is the same as empty string for
// this method but all others treat null as a no-op. We should choose one
// behavior.
RefPtr<StringImpl> StringImpl::Replace(unsigned position,
                                       unsigned length_to_replace,
                                       const StringView& string) {
  position = min(position, length());
  length_to_replace = min(length_to_replace, length() - position);
  unsigned length_to_insert = string.length();
  if (!length_to_replace && !length_to_insert)
    return this;

  CHECK_LT((length() - length_to_replace),
           (numeric_limits<unsigned>::max() - length_to_insert));

  if (Is8Bit() && (string.IsNull() || string.Is8Bit())) {
    LChar* data;
    RefPtr<StringImpl> new_impl = CreateUninitialized(
        length() - length_to_replace + length_to_insert, data);
    memcpy(data, Characters8(), position * sizeof(LChar));
    if (!string.IsNull())
      memcpy(data + position, string.Characters8(),
             length_to_insert * sizeof(LChar));
    memcpy(data + position + length_to_insert,
           Characters8() + position + length_to_replace,
           (length() - position - length_to_replace) * sizeof(LChar));
    return new_impl;
  }
  UChar* data;
  RefPtr<StringImpl> new_impl = CreateUninitialized(
      length() - length_to_replace + length_to_insert, data);
  if (Is8Bit())
    for (unsigned i = 0; i < position; ++i)
      data[i] = Characters8()[i];
  else
    memcpy(data, Characters16(), position * sizeof(UChar));
  if (!string.IsNull()) {
    if (string.Is8Bit())
      for (unsigned i = 0; i < length_to_insert; ++i)
        data[i + position] = string.Characters8()[i];
    else
      memcpy(data + position, string.Characters16(),
             length_to_insert * sizeof(UChar));
  }
  if (Is8Bit()) {
    for (unsigned i = 0; i < length() - position - length_to_replace; ++i)
      data[i + position + length_to_insert] =
          Characters8()[i + position + length_to_replace];
  } else {
    memcpy(data + position + length_to_insert,
           Characters16() + position + length_to_replace,
           (length() - position - length_to_replace) * sizeof(UChar));
  }
  return new_impl;
}

RefPtr<StringImpl> StringImpl::Replace(UChar pattern,
                                       const StringView& replacement) {
  if (replacement.IsNull())
    return this;
  if (replacement.Is8Bit())
    return Replace(pattern, replacement.Characters8(), replacement.length());
  return Replace(pattern, replacement.Characters16(), replacement.length());
}

RefPtr<StringImpl> StringImpl::Replace(UChar pattern,
                                       const LChar* replacement,
                                       unsigned rep_str_length) {
  DCHECK(replacement);

  size_t src_segment_start = 0;
  unsigned match_count = 0;

  // Count the matches.
  while ((src_segment_start = Find(pattern, src_segment_start)) != kNotFound) {
    ++match_count;
    ++src_segment_start;
  }

  // If we have 0 matches then we don't have to do any more work.
  if (!match_count)
    return this;

  CHECK(!rep_str_length ||
        match_count <= numeric_limits<unsigned>::max() / rep_str_length);

  unsigned replace_size = match_count * rep_str_length;
  unsigned new_size = length_ - match_count;
  CHECK_LT(new_size, (numeric_limits<unsigned>::max() - replace_size));

  new_size += replace_size;

  // Construct the new data.
  size_t src_segment_end;
  unsigned src_segment_length;
  src_segment_start = 0;
  unsigned dst_offset = 0;

  if (Is8Bit()) {
    LChar* data;
    RefPtr<StringImpl> new_impl = CreateUninitialized(new_size, data);

    while ((src_segment_end = Find(pattern, src_segment_start)) != kNotFound) {
      src_segment_length = src_segment_end - src_segment_start;
      memcpy(data + dst_offset, Characters8() + src_segment_start,
             src_segment_length * sizeof(LChar));
      dst_offset += src_segment_length;
      memcpy(data + dst_offset, replacement, rep_str_length * sizeof(LChar));
      dst_offset += rep_str_length;
      src_segment_start = src_segment_end + 1;
    }

    src_segment_length = length_ - src_segment_start;
    memcpy(data + dst_offset, Characters8() + src_segment_start,
           src_segment_length * sizeof(LChar));

    DCHECK_EQ(dst_offset + src_segment_length, new_impl->length());

    return new_impl;
  }

  UChar* data;
  RefPtr<StringImpl> new_impl = CreateUninitialized(new_size, data);

  while ((src_segment_end = Find(pattern, src_segment_start)) != kNotFound) {
    src_segment_length = src_segment_end - src_segment_start;
    memcpy(data + dst_offset, Characters16() + src_segment_start,
           src_segment_length * sizeof(UChar));

    dst_offset += src_segment_length;
    for (unsigned i = 0; i < rep_str_length; ++i)
      data[i + dst_offset] = replacement[i];

    dst_offset += rep_str_length;
    src_segment_start = src_segment_end + 1;
  }

  src_segment_length = length_ - src_segment_start;
  memcpy(data + dst_offset, Characters16() + src_segment_start,
         src_segment_length * sizeof(UChar));

  DCHECK_EQ(dst_offset + src_segment_length, new_impl->length());

  return new_impl;
}

RefPtr<StringImpl> StringImpl::Replace(UChar pattern,
                                       const UChar* replacement,
                                       unsigned rep_str_length) {
  DCHECK(replacement);

  size_t src_segment_start = 0;
  unsigned match_count = 0;

  // Count the matches.
  while ((src_segment_start = Find(pattern, src_segment_start)) != kNotFound) {
    ++match_count;
    ++src_segment_start;
  }

  // If we have 0 matches then we don't have to do any more work.
  if (!match_count)
    return this;

  CHECK(!rep_str_length ||
        match_count <= numeric_limits<unsigned>::max() / rep_str_length);

  unsigned replace_size = match_count * rep_str_length;
  unsigned new_size = length_ - match_count;
  CHECK_LT(new_size, (numeric_limits<unsigned>::max() - replace_size));

  new_size += replace_size;

  // Construct the new data.
  size_t src_segment_end;
  unsigned src_segment_length;
  src_segment_start = 0;
  unsigned dst_offset = 0;

  if (Is8Bit()) {
    UChar* data;
    RefPtr<StringImpl> new_impl = CreateUninitialized(new_size, data);

    while ((src_segment_end = Find(pattern, src_segment_start)) != kNotFound) {
      src_segment_length = src_segment_end - src_segment_start;
      for (unsigned i = 0; i < src_segment_length; ++i)
        data[i + dst_offset] = Characters8()[i + src_segment_start];

      dst_offset += src_segment_length;
      memcpy(data + dst_offset, replacement, rep_str_length * sizeof(UChar));

      dst_offset += rep_str_length;
      src_segment_start = src_segment_end + 1;
    }

    src_segment_length = length_ - src_segment_start;
    for (unsigned i = 0; i < src_segment_length; ++i)
      data[i + dst_offset] = Characters8()[i + src_segment_start];

    DCHECK_EQ(dst_offset + src_segment_length, new_impl->length());

    return new_impl;
  }

  UChar* data;
  RefPtr<StringImpl> new_impl = CreateUninitialized(new_size, data);

  while ((src_segment_end = Find(pattern, src_segment_start)) != kNotFound) {
    src_segment_length = src_segment_end - src_segment_start;
    memcpy(data + dst_offset, Characters16() + src_segment_start,
           src_segment_length * sizeof(UChar));

    dst_offset += src_segment_length;
    memcpy(data + dst_offset, replacement, rep_str_length * sizeof(UChar));

    dst_offset += rep_str_length;
    src_segment_start = src_segment_end + 1;
  }

  src_segment_length = length_ - src_segment_start;
  memcpy(data + dst_offset, Characters16() + src_segment_start,
         src_segment_length * sizeof(UChar));

  DCHECK_EQ(dst_offset + src_segment_length, new_impl->length());

  return new_impl;
}

RefPtr<StringImpl> StringImpl::Replace(const StringView& pattern,
                                       const StringView& replacement) {
  if (pattern.IsNull() || replacement.IsNull())
    return this;

  unsigned pattern_length = pattern.length();
  if (!pattern_length)
    return this;

  unsigned rep_str_length = replacement.length();
  size_t src_segment_start = 0;
  unsigned match_count = 0;

  // Count the matches.
  while ((src_segment_start = Find(pattern, src_segment_start)) != kNotFound) {
    ++match_count;
    src_segment_start += pattern_length;
  }

  // If we have 0 matches, we don't have to do any more work
  if (!match_count)
    return this;

  unsigned new_size = length_ - match_count * pattern_length;
  CHECK(!rep_str_length ||
        match_count <= numeric_limits<unsigned>::max() / rep_str_length);

  CHECK_LE(new_size,
           (numeric_limits<unsigned>::max() - match_count * rep_str_length));

  new_size += match_count * rep_str_length;

  // Construct the new data
  size_t src_segment_end;
  unsigned src_segment_length;
  src_segment_start = 0;
  unsigned dst_offset = 0;
  bool src_is8_bit = Is8Bit();
  bool replacement_is8_bit = replacement.Is8Bit();

  // There are 4 cases:
  // 1. This and replacement are both 8 bit.
  // 2. This and replacement are both 16 bit.
  // 3. This is 8 bit and replacement is 16 bit.
  // 4. This is 16 bit and replacement is 8 bit.
  if (src_is8_bit && replacement_is8_bit) {
    // Case 1
    LChar* data;
    RefPtr<StringImpl> new_impl = CreateUninitialized(new_size, data);
    while ((src_segment_end = Find(pattern, src_segment_start)) != kNotFound) {
      src_segment_length = src_segment_end - src_segment_start;
      memcpy(data + dst_offset, Characters8() + src_segment_start,
             src_segment_length * sizeof(LChar));
      dst_offset += src_segment_length;
      memcpy(data + dst_offset, replacement.Characters8(),
             rep_str_length * sizeof(LChar));
      dst_offset += rep_str_length;
      src_segment_start = src_segment_end + pattern_length;
    }

    src_segment_length = length_ - src_segment_start;
    memcpy(data + dst_offset, Characters8() + src_segment_start,
           src_segment_length * sizeof(LChar));

    DCHECK_EQ(dst_offset + src_segment_length, new_impl->length());

    return new_impl;
  }

  UChar* data;
  RefPtr<StringImpl> new_impl = CreateUninitialized(new_size, data);
  while ((src_segment_end = Find(pattern, src_segment_start)) != kNotFound) {
    src_segment_length = src_segment_end - src_segment_start;
    if (src_is8_bit) {
      // Case 3.
      for (unsigned i = 0; i < src_segment_length; ++i)
        data[i + dst_offset] = Characters8()[i + src_segment_start];
    } else {
      // Case 2 & 4.
      memcpy(data + dst_offset, Characters16() + src_segment_start,
             src_segment_length * sizeof(UChar));
    }
    dst_offset += src_segment_length;
    if (replacement_is8_bit) {
      // Cases 2 & 3.
      for (unsigned i = 0; i < rep_str_length; ++i)
        data[i + dst_offset] = replacement.Characters8()[i];
    } else {
      // Case 4
      memcpy(data + dst_offset, replacement.Characters16(),
             rep_str_length * sizeof(UChar));
    }
    dst_offset += rep_str_length;
    src_segment_start = src_segment_end + pattern_length;
  }

  src_segment_length = length_ - src_segment_start;
  if (src_is8_bit) {
    // Case 3.
    for (unsigned i = 0; i < src_segment_length; ++i)
      data[i + dst_offset] = Characters8()[i + src_segment_start];
  } else {
    // Cases 2 & 4.
    memcpy(data + dst_offset, Characters16() + src_segment_start,
           src_segment_length * sizeof(UChar));
  }

  DCHECK_EQ(dst_offset + src_segment_length, new_impl->length());

  return new_impl;
}

RefPtr<StringImpl> StringImpl::UpconvertedString() {
  if (Is8Bit())
    return String::Make16BitFrom8BitSource(Characters8(), length_)
        .ReleaseImpl();
  return this;
}

static inline bool StringImplContentEqual(const StringImpl* a,
                                          const StringImpl* b) {
  unsigned a_length = a->length();
  unsigned b_length = b->length();
  if (a_length != b_length)
    return false;

  if (a->Is8Bit()) {
    if (b->Is8Bit())
      return Equal(a->Characters8(), b->Characters8(), a_length);

    return Equal(a->Characters8(), b->Characters16(), a_length);
  }

  if (b->Is8Bit())
    return Equal(a->Characters16(), b->Characters8(), a_length);

  return Equal(a->Characters16(), b->Characters16(), a_length);
}

bool Equal(const StringImpl* a, const StringImpl* b) {
  if (a == b)
    return true;
  if (!a || !b)
    return false;
  if (a->IsAtomic() && b->IsAtomic())
    return false;

  return StringImplContentEqual(a, b);
}

template <typename CharType>
inline bool EqualInternal(const StringImpl* a,
                          const CharType* b,
                          unsigned length) {
  if (!a)
    return !b;
  if (!b)
    return false;

  if (a->length() != length)
    return false;
  if (a->Is8Bit())
    return Equal(a->Characters8(), b, length);
  return Equal(a->Characters16(), b, length);
}

bool Equal(const StringImpl* a, const LChar* b, unsigned length) {
  return EqualInternal(a, b, length);
}

bool Equal(const StringImpl* a, const UChar* b, unsigned length) {
  return EqualInternal(a, b, length);
}

bool Equal(const StringImpl* a, const LChar* b) {
  if (!a)
    return !b;
  if (!b)
    return !a;

  unsigned length = a->length();

  if (a->Is8Bit()) {
    const LChar* a_ptr = a->Characters8();
    for (unsigned i = 0; i != length; ++i) {
      LChar bc = b[i];
      LChar ac = a_ptr[i];
      if (!bc)
        return false;
      if (ac != bc)
        return false;
    }

    return !b[length];
  }

  const UChar* a_ptr = a->Characters16();
  for (unsigned i = 0; i != length; ++i) {
    LChar bc = b[i];
    if (!bc)
      return false;
    if (a_ptr[i] != bc)
      return false;
  }

  return !b[length];
}

bool EqualNonNull(const StringImpl* a, const StringImpl* b) {
  DCHECK(a);
  DCHECK(b);
  if (a == b)
    return true;

  return StringImplContentEqual(a, b);
}

bool EqualIgnoringNullity(StringImpl* a, StringImpl* b) {
  if (!a && b && !b->length())
    return true;
  if (!b && a && !a->length())
    return true;
  return Equal(a, b);
}

template <typename CharacterType1, typename CharacterType2>
int CodePointCompareIgnoringASCIICase(unsigned l1,
                                      unsigned l2,
                                      const CharacterType1* c1,
                                      const CharacterType2* c2) {
  const unsigned lmin = l1 < l2 ? l1 : l2;
  unsigned pos = 0;
  while (pos < lmin && ToASCIILower(*c1) == ToASCIILower(*c2)) {
    ++c1;
    ++c2;
    ++pos;
  }

  if (pos < lmin)
    return (ToASCIILower(c1[0]) > ToASCIILower(c2[0])) ? 1 : -1;

  if (l1 == l2)
    return 0;

  return (l1 > l2) ? 1 : -1;
}

int CodePointCompareIgnoringASCIICase(const StringImpl* string1,
                                      const LChar* string2) {
  unsigned length1 = string1 ? string1->length() : 0;
  size_t length2 = string2 ? strlen(reinterpret_cast<const char*>(string2)) : 0;

  if (!string1)
    return length2 > 0 ? -1 : 0;

  if (!string2)
    return length1 > 0 ? 1 : 0;

  if (string1->Is8Bit())
    return CodePointCompareIgnoringASCIICase(length1, length2,
                                             string1->Characters8(), string2);
  return CodePointCompareIgnoringASCIICase(length1, length2,
                                           string1->Characters16(), string2);
}

UChar32 ToUpper(UChar32 c, const AtomicString& locale_identifier) {
  if (!locale_identifier.IsNull()) {
    if (LocaleIdMatchesLang(locale_identifier, "tr") ||
        LocaleIdMatchesLang(locale_identifier, "az")) {
      if (c == 'i')
        return kLatinCapitalLetterIWithDotAbove;
      if (c == kLatinSmallLetterDotlessI)
        return 'I';
    } else if (LocaleIdMatchesLang(locale_identifier, "lt")) {
      // TODO(rob.buis) implement upper-casing rules for lt
      // like in StringImpl::upper(locale).
    }
  }

  return ToUpper(c);
}

}  // namespace WTF
