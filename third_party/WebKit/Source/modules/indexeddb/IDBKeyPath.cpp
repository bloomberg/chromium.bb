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

#include "modules/indexeddb/IDBKeyPath.h"

#include "platform/wtf/ASCIICType.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/dtoa.h"
#include "platform/wtf/text/CharacterNames.h"
#include "platform/wtf/text/Unicode.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"

namespace blink {

namespace {

using namespace WTF::Unicode;

// The following correspond to grammar in ECMA-262.
const uint32_t kUnicodeLetter = kLetter_Uppercase | kLetter_Lowercase |
                                kLetter_Titlecase | kLetter_Modifier |
                                kLetter_Other | kNumber_Letter;
const uint32_t kUnicodeCombiningMark =
    kMark_NonSpacing | kMark_SpacingCombining;
const uint32_t kUnicodeDigit = kNumber_DecimalDigit;
const uint32_t kUnicodeConnectorPunctuation = kPunctuation_Connector;

static inline bool IsIdentifierStartCharacter(UChar c) {
  return (Category(c) & kUnicodeLetter) || (c == '$') || (c == '_');
}

static inline bool IsIdentifierCharacter(UChar c) {
  return (Category(c) & (kUnicodeLetter | kUnicodeCombiningMark |
                         kUnicodeDigit | kUnicodeConnectorPunctuation)) ||
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

IDBKeyPath::IDBKeyPath(const String& string)
    : type_(kStringType), string_(string) {
  DCHECK(!string_.IsNull());
}

IDBKeyPath::IDBKeyPath(const Vector<String>& array)
    : type_(kArrayType), array_(array) {
#if DCHECK_IS_ON()
  for (size_t i = 0; i < array_.size(); ++i)
    DCHECK(!array_[i].IsNull());
#endif
}

IDBKeyPath::IDBKeyPath(const StringOrStringSequence& key_path) {
  if (key_path.isNull()) {
    type_ = kNullType;
  } else if (key_path.isString()) {
    type_ = kStringType;
    string_ = key_path.getAsString();
    DCHECK(!string_.IsNull());
  } else {
    DCHECK(key_path.isStringSequence());
    type_ = kArrayType;
    array_ = key_path.getAsStringSequence();
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
      string_ = key_path.GetString();
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
