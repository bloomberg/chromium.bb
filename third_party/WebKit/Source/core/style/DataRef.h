/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2008 Apple Inc. All rights reserved.
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

#ifndef DataRef_h
#define DataRef_h

#include "platform/wtf/Allocator.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

template <typename T>
class DataRef {
  USING_FAST_MALLOC(DataRef);

 public:
  const T* Get() const { return data_.get(); }

  const T& operator*() const { return *Get(); }
  const T* operator->() const { return Get(); }

  T* Access() {
    if (!data_->HasOneRef())
      data_ = data_->Copy();
    return data_.get();
  }

  void Init() {
    DCHECK(!data_);
    data_ = T::Create();
  }

  bool operator==(const DataRef<T>& o) const {
    DCHECK(data_);
    DCHECK(o.data_);
    return data_ == o.data_ || *data_ == *o.data_;
  }

  bool operator!=(const DataRef<T>& o) const {
    DCHECK(data_);
    DCHECK(o.data_);
    return data_ != o.data_ && *data_ != *o.data_;
  }

  void operator=(std::nullptr_t) { data_ = nullptr; }

 private:
  scoped_refptr<T> data_;
};

}  // namespace blink

#endif  // DataRef_h
