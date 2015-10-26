// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_REQUEST_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_REQUEST_H_

#include "mojo/public/cpp/system/macros.h"

namespace mojo {

// Represents an associated interface request.
// TODO(yzshen): implement it.
template <typename Interface>
class AssociatedInterfaceRequest {
  MOJO_MOVE_ONLY_TYPE(AssociatedInterfaceRequest)

 public:
  AssociatedInterfaceRequest() {}
  AssociatedInterfaceRequest(AssociatedInterfaceRequest&& other) {}

  AssociatedInterfaceRequest& operator=(AssociatedInterfaceRequest&& other) {
    return *this;
  }

  bool is_pending() const { return false; }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_REQUEST_H_
