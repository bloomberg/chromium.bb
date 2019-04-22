// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WITH_DESTRUCTION_CALLBACK_H_
#define BASE_WITH_DESTRUCTION_CALLBACK_H_

#include "base/macros.h"

namespace openscreen {

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

  DISALLOW_COPY_AND_ASSIGN(WithDestructionCallback);
};

}  // namespace openscreen

#endif  // BASE_WITH_DESTRUCTION_CALLBACK_H_
