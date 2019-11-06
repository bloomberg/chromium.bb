// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_RELATIVE_POINTER_DELEGATE_H_
#define COMPONENTS_EXO_RELATIVE_POINTER_DELEGATE_H_

#include "base/time/time.h"

namespace exo {
class Pointer;

// Handles sending relative mouse movements.
class RelativePointerDelegate {
 public:
  // Called at the top of the pointer's destructor, to give observers a
  // chance to remove themselves.
  virtual void OnPointerDestroying(Pointer* pointer) = 0;

  virtual void OnPointerRelativeMotion(base::TimeTicks time_stamp,
                                       const gfx::PointF& relativeMotion) = 0;

 protected:
  virtual ~RelativePointerDelegate() {}
};

}  // namespace exo

#endif  // COMPONENTS_EXO_RELATIVE_POINTER_DELEGATE_H_
