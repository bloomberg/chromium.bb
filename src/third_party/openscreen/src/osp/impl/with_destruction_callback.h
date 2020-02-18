// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_WITH_DESTRUCTION_CALLBACK_H_
#define OSP_IMPL_WITH_DESTRUCTION_CALLBACK_H_

#include "platform/base/macros.h"

namespace openscreen {
namespace osp {

// A decoration for classes which allows a callback to be run just after
// destruction. Setting the callback is optional.
class WithDestructionCallback {
 public:
  using DestructionCallbackFunctionPointer = void (*)(void*);

  WithDestructionCallback();
  ~WithDestructionCallback();

  // Sets the function to be called from the destructor.
  void SetDestructionCallback(DestructionCallbackFunctionPointer function,
                              void* state);

 private:
  DestructionCallbackFunctionPointer destruction_callback_function_ = nullptr;
  void* destruction_callback_state_ = nullptr;

  OSP_DISALLOW_COPY_AND_ASSIGN(WithDestructionCallback);
};

}  // namespace osp
}  // namespace openscreen

#endif  // OSP_IMPL_WITH_DESTRUCTION_CALLBACK_H_
