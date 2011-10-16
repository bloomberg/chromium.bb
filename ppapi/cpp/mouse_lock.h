// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_MOUSE_LOCK_H_
#define PPAPI_CPP_MOUSE_LOCK_H_

#include "ppapi/c/pp_stdint.h"

namespace pp {

class CompletionCallback;
class Instance;

// This class allows you to associate the PPP_MouseLock and PPB_MouseLock
// C-based interfaces with an object. It associates itself with the given
// instance, and registers as the global handler for handling the PPP_MouseLock
// interface that the browser calls.
//
// You would typically use this either via inheritance on your instance:
//   class MyInstance : public pp::Instance, public pp::MouseLock {
//     class MyInstance() : pp::MouseLock(this) {
//     }
//     ...
//   };
//
// or by composition:
//   class MyMouseLock : public pp::MouseLock {
//     ...
//   };
//
//   class MyInstance : public pp::Instance {
//     MyInstance() : mouse_lock_(this) {
//     }
//
//     MyMouseLock mouse_lock_;
//   };
class MouseLock {
 public:
  explicit MouseLock(Instance* instance);
  virtual ~MouseLock();

  // PPP_MouseLock functions exposed as virtual functions for you to override.
  virtual void MouseLockLost() = 0;

  // PPB_MouseLock functions for you to call.
  int32_t LockMouse(const CompletionCallback& cc);
  void UnlockMouse();

 private:
  Instance* associated_instance_;
};

}  // namespace pp

#endif  // PPAPI_CPP_MOUSE_LOCK_H_
