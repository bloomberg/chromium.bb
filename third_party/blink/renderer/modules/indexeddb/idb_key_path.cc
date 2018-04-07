/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/indexeddb/idb_key_path.h"

#include "third_party/blink/public/platform/modules/indexeddb/web_idb_types.h"
#include "third_party/blink/renderer/platform/wtf/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/dtoa.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

namespace {

// The following correspond to grammar in ECMA-262.
const uint32_t kUnicodeLetter =
    WTF::Unicode::kLetter_Uppercase | WTF::Unicode::kLetter_Lowercase |
    WTF::Unicode::kLetter_Titlecase | WTF::Unicode::kLetter_Modifier |
    WTF::Unicode::kLetter_Other | WTF::Unicode::kNumber_Letter;
const uint32_t kUnicodeCombiningMark =
    WTF::Unicode::kMark_NonSpacing | WTF::Unicode::kMark_SpacingCombining;
const uint32_t kUnicodeDigit = WTF::Unicode::kNumber_DecimalDigit;
const uint32_t kUnicodeConnectorPunctuation =
    WTF::Unicode::kPunctuation_Connector;

static inline bool IsIdentifierStartCharacter(UChar c) {
  return (WTF::Unicode::Category(c) & kUnicodeLetter) || (c == '$') ||
         (c == '_');
}

static inline bool IsIdentifierCharacter(UChar c) {
  return (WTF::Unicode::Category(c) &
          (kUnicodeLetter | kUnicodeCombiningMark | kUnicodeDigit |
           kUnicodeConnectorPunctuation)) ||
         (c == '$') || (c == '_') || (c == kZeroWidthNonJoinerCharacter) ||
         (c == kZeroWidthJoinerCharacter);
}

bool IsIdentifier(const String& s) {
  size_t length = s.length();
  if (!length)
    return false;
  if (!IsIdentifierStartCharacter(s[0]))
    return false;
  for (size_t i = 1; i < length; ++i) {
    if (!IsIdentifierCharacter(s[i]))
      return false;
  }
  return true;
}

}  // namespace

bool IDBIsValidKeyPath(const String& key_path) {
  IDBKeyPathParseError error;
  Vector<String> key_path_elements;
  IDBParseKeyPath(key_path, key_path_elements, error);
  return error == kIDBKeyPathParseErrorNone;
}

void IDBParseKeyPath(const String& key_path,
                     Vector<String>& elements,
                     IDBKeyPathParseError& error) {
  // IDBKeyPath ::= EMPTY_STRING | identifier ('.' identifier)*

  if (key_path.IsEmpty()) {
    error = kIDBKeyPathParseErrorNone;
    return;
  }

  key_path.Split('.', /*allow_empty_entries=*/true, elements);
  for (size_t i = 0; i < elements.size(); ++i) {
    if (!IsIdentifier(elements[i])) {
      error = kIDBKeyPathParseErrorIdentifier;
      return;
    }
  }
  error = kIDBKeyPathParseErrorNone;
}

IDBKeyPath::IDBKeyPath(const class String& string)
    : type_(kStringType), string_(string) {
  DCHECK(!string_.IsNull());
}

IDBKeyPath::IDBKeyPath(const Vector<class String>& array)
    : type_(kArrayType), array_(array) {
#if DCHECK_IS_ON()
  for (size_t i = 0; i < array_.size(); ++i)
    DCHECK(!array_[i].IsNull());
#endif
}

IDBKeyPath::IDBKeyPath(const StringOrStringSequence& key_path) {
  if (key_path.IsNull()) {
    type_ = kNullType;
  } else if (key_path.IsString()) {
    type_ = kStringType;
    string_ = key_path.GetAsString();
    DCHECK(!string_.IsNull());
  } else {
    DCHECK(key_path.IsStringSequence());
    type_ = kArrayType;
    array_ = key_path.GetAsStringSequence();
#if DCHECK_IS_ON()
    for (size_t i = 0; i < array_.size(); ++i)
      DCHECK(!array_[i].IsNull());
#endif
  }
}

IDBKeyPath::IDBKeyPath(const WebIDBKeyPath& key_path) {
  switch (key_path.KeyPathType()) {
    case kWebIDBKeyPathTypeNull:
      type_ = kNullType;
      return;

    case kWebIDBKeyPathTypeString:
      type_ = kStringType;
      string_ = key_path.String();
      return;

    case kWebIDBKeyPathTypeArray:
      type_ = kArrayType;
      for (size_t i = 0, size = key_path.Array().size(); i < size; ++i)
        array_.push_back(key_path.Array()[i]);
      return;
  }
  NOTREACHED();
}

IDBKeyPath::operator WebIDBKeyPath() const {
  switch (type_) {
    case kNullType:
      return WebIDBKeyPath();
    case kStringType:
      return WebIDBKeyPath(WebString(string_));
    case kArrayType:
      return WebIDBKeyPath(array_);
  }
  NOTREACHED();
  return WebIDBKeyPath();
}

bool IDBKeyPath::IsValid() const {
  switch (type_) {
    case kNullType:
      return false;

    case kStringType:
      return IDBIsValidKeyPath(string_);

    case kArrayType:
      if (array_.IsEmpty())
        return false;
      for (size_t i = 0; i < array_.size(); ++i) {
        if (!IDBIsValidKeyPath(array_[i]))
          return false;
      }
      return true;
  }
  NOTREACHED();
  return false;
}

bool IDBKeyPath::operator==(const IDBKeyPath& other) const {
  if (type_ != other.type_)
    return false;

  switch (type_) {
    case kNullType:
      return true;
    case kStringType:
      return string_ == other.string_;
    case kArrayType:
      return array_ == other.array_;
  }
  NOTREACHED();
  return false;
}

STATIC_ASSERT_ENUM(kWebIDBKeyPathTypeNull, IDBKeyPath::kNullType);
STATIC_ASSERT_ENUM(kWebIDBKeyPathTypeString, IDBKeyPath::kStringType);
STATIC_ASSERT_ENUM(kWebIDBKeyPathTypeArray, IDBKeyPath::kArrayType);

}  // namespace blink
