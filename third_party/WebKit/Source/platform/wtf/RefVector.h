// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RefVector_h
#define RefVector_h

#include <utility>

#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"

namespace blink {

template <typename T>
class RefVector : public RefCounted<RefVector<T>> {
 public:
  static RefPtr<RefVector> Create() { return AdoptRef(new RefVector<T>); }
  static RefPtr<RefVector> Create(const Vector<T>& vector) {
    return AdoptRef(new RefVector<T>(vector));
  }
  static RefPtr<RefVector> Create(Vector<T>&& vector) {
    return AdoptRef(new RefVector<T>(std::move(vector)));
  }
  RefPtr<RefVector> Copy() { return Create(GetVector()); }

  const T& operator[](size_t i) const { return vector_[i]; }
  T& operator[](size_t i) { return vector_[i]; }
  const T& at(size_t i) const { return vector_.at(i); }
  T& at(size_t i) { return vector_.at(i); }

  bool operator==(const RefVector& o) const { return vector_ == o.vector_; }
  bool operator!=(const RefVector& o) const { return vector_ != o.vector_; }

  size_t size() const { return vector_.size(); }
  bool IsEmpty() const { return !size(); }
  void push_back(const T& decoration) { vector_.push_back(decoration); }
  const Vector<T>& GetVector() const { return vector_; }

 private:
  Vector<T> vector_;
  RefVector() {}
  RefVector(const Vector<T>& vector) : vector_(vector) {}
  RefVector(Vector<T>&& vector) : vector_(vector) {}
};

}  // namespace blink

#endif  // RefVector_h
