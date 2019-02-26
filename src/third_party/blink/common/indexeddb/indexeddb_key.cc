// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"

#include <string>
#include <utility>

namespace blink {

using blink::WebIDBKeyType;
using blink::kWebIDBKeyTypeArray;
using blink::kWebIDBKeyTypeBinary;
using blink::kWebIDBKeyTypeDate;
using blink::kWebIDBKeyTypeInvalid;
using blink::kWebIDBKeyTypeMin;
using blink::kWebIDBKeyTypeNull;
using blink::kWebIDBKeyTypeNumber;
using blink::kWebIDBKeyTypeString;

namespace {

// Very rough estimate of minimum key size overhead.
const size_t kOverheadSize = 16;

size_t CalculateArraySize(const IndexedDBKey::KeyArray& keys) {
  size_t size(0);
  for (const auto& key : keys)
    size += key.size_estimate();
  return size;
}

template <typename T>
int Compare(const T& a, const T& b) {
  // Using '<' for both comparisons here is as generic as possible (for e.g.
  // objects which only define operator<() and not operator>() or operator==())
  // and also allows e.g. floating point NaNs to compare equal.
  if (a < b)
    return -1;
  return (b < a) ? 1 : 0;
}

}  // namespace

IndexedDBKey::IndexedDBKey()
    : type_(kWebIDBKeyTypeNull), size_estimate_(kOverheadSize) {}

IndexedDBKey::IndexedDBKey(WebIDBKeyType type)
    : type_(type), size_estimate_(kOverheadSize) {
  DCHECK(type == kWebIDBKeyTypeNull || type == kWebIDBKeyTypeInvalid);
}

IndexedDBKey::IndexedDBKey(double number, WebIDBKeyType type)
    : type_(type),
      number_(number),
      size_estimate_(kOverheadSize + sizeof(number)) {
  DCHECK(type == kWebIDBKeyTypeNumber || type == kWebIDBKeyTypeDate);
}

IndexedDBKey::IndexedDBKey(KeyArray array)
    : type_(kWebIDBKeyTypeArray),
      array_(std::move(array)),
      size_estimate_(kOverheadSize + CalculateArraySize(array_)) {}

IndexedDBKey::IndexedDBKey(std::string binary)
    : type_(kWebIDBKeyTypeBinary),
      binary_(std::move(binary)),
      size_estimate_(kOverheadSize +
                     (binary_.length() * sizeof(std::string::value_type))) {}

IndexedDBKey::IndexedDBKey(base::string16 string)
    : type_(kWebIDBKeyTypeString),
      string_(std::move(string)),
      size_estimate_(kOverheadSize +
                     (string_.length() * sizeof(base::string16::value_type))) {}

IndexedDBKey::IndexedDBKey(const IndexedDBKey& other) = default;
IndexedDBKey::~IndexedDBKey() = default;
IndexedDBKey& IndexedDBKey::operator=(const IndexedDBKey& other) = default;

bool IndexedDBKey::IsValid() const {
  if (type_ == kWebIDBKeyTypeInvalid || type_ == kWebIDBKeyTypeNull)
    return false;

  if (type_ == kWebIDBKeyTypeArray) {
    for (size_t i = 0; i < array_.size(); i++) {
      if (!array_[i].IsValid())
        return false;
    }
  }

  return true;
}

bool IndexedDBKey::IsLessThan(const IndexedDBKey& other) const {
  return CompareTo(other) < 0;
}

bool IndexedDBKey::Equals(const IndexedDBKey& other) const {
  return !CompareTo(other);
}

int IndexedDBKey::CompareTo(const IndexedDBKey& other) const {
  DCHECK(IsValid());
  DCHECK(other.IsValid());
  if (type_ != other.type_)
    return type_ > other.type_ ? -1 : 1;

  switch (type_) {
    case kWebIDBKeyTypeArray:
      for (size_t i = 0; i < array_.size() && i < other.array_.size(); ++i) {
        int result = array_[i].CompareTo(other.array_[i]);
        if (result != 0)
          return result;
      }
      return Compare(array_.size(), other.array_.size());
    case kWebIDBKeyTypeBinary:
      return binary_.compare(other.binary_);
    case kWebIDBKeyTypeString:
      return string_.compare(other.string_);
    case kWebIDBKeyTypeDate:
    case kWebIDBKeyTypeNumber:
      return Compare(number_, other.number_);
    case kWebIDBKeyTypeInvalid:
    case kWebIDBKeyTypeNull:
    case kWebIDBKeyTypeMin:
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace blink
