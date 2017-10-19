/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef IDBKey_h
#define IDBKey_h

#include "modules/ModulesExport.h"
#include "platform/SharedBuffer.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class MODULES_EXPORT IDBKey : public GarbageCollectedFinalized<IDBKey> {
 public:
  typedef HeapVector<Member<IDBKey>> KeyArray;

  static IDBKey* CreateInvalid() { return new IDBKey(); }

  static IDBKey* CreateNumber(double number) {
    return new IDBKey(kNumberType, number);
  }

  static IDBKey* CreateBinary(RefPtr<SharedBuffer> binary) {
    return new IDBKey(std::move(binary));
  }

  static IDBKey* CreateString(const String& string) {
    return new IDBKey(string);
  }

  static IDBKey* CreateDate(double date) { return new IDBKey(kDateType, date); }

  static IDBKey* CreateArray(const KeyArray& array) {
    return new IDBKey(array);
  }

  ~IDBKey();
  void Trace(blink::Visitor*);

  // In order of the least to the highest precedent in terms of sort order.
  // These values are written to logs. New enum values can be added, but
  // existing enums must never be renumbered or deleted and reused.
  enum Type {
    kInvalidType = 0,
    kArrayType = 1,
    kBinaryType = 2,
    kStringType = 3,
    kDateType = 4,
    kNumberType = 5,
    kTypeEnumMax,
  };

  Type GetType() const { return type_; }
  bool IsValid() const;

  const KeyArray& Array() const {
    DCHECK_EQ(type_, kArrayType);
    return array_;
  }

  RefPtr<SharedBuffer> Binary() const {
    DCHECK_EQ(type_, kBinaryType);
    return binary_;
  }

  const String& GetString() const {
    DCHECK_EQ(type_, kStringType);
    return string_;
  }

  double Date() const {
    DCHECK_EQ(type_, kDateType);
    return number_;
  }

  double Number() const {
    DCHECK_EQ(type_, kNumberType);
    return number_;
  }

  int Compare(const IDBKey* other) const;
  bool IsLessThan(const IDBKey* other) const;
  bool IsEqual(const IDBKey* other) const;

  // Returns a new key array with invalid keys and duplicates removed.
  KeyArray ToMultiEntryArray() const;

 private:
  IDBKey() : type_(kInvalidType) {}
  IDBKey(Type type, double number) : type_(type), number_(number) {}
  explicit IDBKey(const String& value) : type_(kStringType), string_(value) {}
  explicit IDBKey(RefPtr<SharedBuffer> value)
      : type_(kBinaryType), binary_(std::move(value)) {}
  explicit IDBKey(const KeyArray& key_array)
      : type_(kArrayType), array_(key_array) {}

  const Type type_;
  const KeyArray array_;
  RefPtr<SharedBuffer> binary_;
  const String string_;
  const double number_ = 0;
};

}  // namespace blink

#endif  // IDBKey_h
