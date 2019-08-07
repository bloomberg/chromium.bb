// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRING_DATA_VIEW_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRING_DATA_VIEW_H_

#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_context.h"

namespace mojo {

// Access to the contents of a serialized string.
class StringDataView {
 public:
  StringDataView() {}

  StringDataView(internal::String_Data* data,
                 internal::SerializationContext* context)
      : data_(data) {}

  bool is_null() const { return !data_; }

  const char* storage() const { return data_->storage(); }

  size_t size() const { return data_->size(); }

 private:
  internal::String_Data* data_ = nullptr;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRING_DATA_VIEW_H_
