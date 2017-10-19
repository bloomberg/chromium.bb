// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Nullable_h
#define Nullable_h

#include "platform/heap/Handle.h"
#include "platform/wtf/Assertions.h"

namespace blink {

enum class UnionTypeConversionMode {
  kNullable,
  kNotNullable,
};

template <typename T>
class Nullable {
  DISALLOW_NEW();

 public:
  Nullable() : value_(), is_null_(true) {}

  Nullable(std::nullptr_t) : value_(), is_null_(true) {}

  Nullable(const T& value) : value_(value), is_null_(false) {}

  Nullable(const Nullable& other)
      : value_(other.value_), is_null_(other.is_null_) {}

  Nullable& operator=(const Nullable& other) {
    value_ = other.value_;
    is_null_ = other.is_null_;
    return *this;
  }

  Nullable& operator=(std::nullptr_t) {
    value_ = T();
    is_null_ = true;
    return *this;
  }

  void Set(const T& value) {
    value_ = value;
    is_null_ = false;
  }

  void Set(std::nullptr_t) {
    value_ = T();
    is_null_ = true;
  }

  const T& Get() const {
    DCHECK(!is_null_);
    return value_;
  }
  T& Get() {
    DCHECK(!is_null_);
    return value_;
  }
  bool IsNull() const { return is_null_; }

  explicit operator bool() const { return !is_null_; }

  bool operator==(const Nullable& other) const {
    return (is_null_ && other.is_null_) ||
           (!is_null_ && !other.is_null_ && value_ == other.value_);
  }

  void Trace(blink::Visitor* visitor) {
    TraceIfNeeded<T>::Trace(visitor, value_);
  }

 private:
  T value_;
  bool is_null_;
};

}  // namespace blink

#endif  // Nullable_h
