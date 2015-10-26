// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_PTR_INFO_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_PTR_INFO_H_

#include "mojo/public/cpp/system/macros.h"

namespace mojo {

// AssociatedInterfacePtrInfo stores necessary information to construct an
// associated interface pointer.
// TODO(yzshen): implement it.
template <typename Interface>
class AssociatedInterfacePtrInfo {
  MOJO_MOVE_ONLY_TYPE(AssociatedInterfacePtrInfo);

 public:
  AssociatedInterfacePtrInfo() {}
  AssociatedInterfacePtrInfo(AssociatedInterfacePtrInfo&& other) {}

  AssociatedInterfacePtrInfo& operator=(AssociatedInterfacePtrInfo&& other) {
    return *this;
  }

  bool is_valid() const { return false; }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_PTR_INFO_H_
