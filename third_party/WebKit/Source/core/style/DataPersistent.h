// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DataPersistent_h
#define DataPersistent_h

#include <memory>
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

// DataPersistent<T> provides the copy-on-modify behavior of DataRef<>,
// but for Persistent<> heap references.
//
// That is, the DataPersistent<T> copy assignment, |a = b;|, makes |a|
// share a reference to the T object that |b| holds until |a| is mutated
// and access() on it is called. Or, dually, if |b| is mutated after the
// assignment that mutation isn't observable to |a| but will be performed
// on a copy of the underlying T object.
//
// DataPersistent<T> does assume that no one keeps non-DataPersistent<> shared
// references to the underlying T object that it manages, and is mutating the
// object via those.
template <typename T>
class DataPersistent {
  USING_FAST_MALLOC(DataPersistent);

 public:
  DataPersistent()
      : data_(WTF::WrapUnique(new Persistent<T>(T::Create()))),
        own_copy_(true) {}

  DataPersistent(std::nullptr_t) : data_(nullptr), own_copy_(false) {}

  DataPersistent(const DataPersistent& other) : own_copy_(false) {
    if (other.data_)
      data_ = WTF::WrapUnique(new Persistent<T>(other.data_->Get()));

    // Invalidated, subsequent mutations will happen on a new copy.
    //
    // (Clearing |m_ownCopy| will not be observable over T, hence
    // the const_cast<> is considered acceptable here.)
    const_cast<DataPersistent&>(other).own_copy_ = false;
  }

  const T* Get() const { return data_ ? data_->Get() : nullptr; }

  const T& operator*() const { return data_ ? *Get() : nullptr; }
  const T* operator->() const { return Get(); }

  T* Access() {
    if (data_ && !own_copy_) {
      *data_ = (*data_)->Copy();
      own_copy_ = true;
    }
    return data_ ? data_->Get() : nullptr;
  }

  void Init() {
    DCHECK(!data_);
    data_ = WTF::WrapUnique(new Persistent<T>(T::Create()));
    own_copy_ = true;
  }

  bool operator==(const DataPersistent<T>& o) const {
    DCHECK(data_);
    DCHECK(o.data_);
    return data_->Get() == o.data_->Get() || *data_->Get() == *o.data_->Get();
  }

  bool operator!=(const DataPersistent<T>& o) const {
    DCHECK(data_);
    DCHECK(o.data_);
    return data_->Get() != o.data_->Get() && *data_->Get() != *o.data_->Get();
  }

  void operator=(std::nullptr_t) { data_.clear(); }

 private:
  // Reduce size of DataPersistent<> by delaying creation of Persistent<>.
  std::unique_ptr<Persistent<T>> data_;
  unsigned own_copy_ : 1;
};

}  // namespace blink

#endif  // DataPersistent_h
