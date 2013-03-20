// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_EXTENSIONS_OPTIONAL_H_
#define PPAPI_CPP_EXTENSIONS_OPTIONAL_H_

namespace pp {
namespace ext {

template <class T>
class Optional {
 public:
  Optional() : value_(NULL) {
  }
  // Takes ownership of |value|.
  explicit Optional(T* value) : value_(value) {
  }
  Optional(const T& value) : value_(new T(value)) {
  }
  Optional(const Optional<T>& other)
      : value_(other.value_ ? new T(*other.value_) : NULL) {
  }

  ~Optional() {
    Reset();
  }

  Optional<T>& operator=(const T& other) {
    if (value_ == &other)
      return *this;

    Reset();
    value_ = new T(other);

    return *this;
  }

  Optional<T>& operator=(const Optional<T>& other) {
    if (value_ == other.value_)
      return *this;

    Reset();
    if (other.value_)
      value_ = new T(*other.value_);

    return *this;
  }

  bool IsSet() const {
    return !!value_;
  }

  T* Get() const {
    return value_;
  }

  // Should only be used when IsSet() is true.
  T& operator*() const {
    return *value_;
  }

  // Should only be used when IsSet() is true.
  T* operator->() const {
    PP_DCHECK(value_);
    return value_;
  }

  // Takes ownership of |value|.
  void Set(T* value) {
    if (value == value_)
      return;

    Reset();
    *value_ = value;
  }

  void Reset() {
    T* value = value_;
    value_ = NULL;
    delete value;
  }

  void Swap(Optional<T>* other) {
    T* temp = value_;
    value_ = other->value_;
    other->value_ = temp;
  }

 private:
  T* value_;
};

}  // namespace ext
}  // namespace pp

#endif  // PPAPI_CPP_EXTENSIONS_OPTIONAL_H_
