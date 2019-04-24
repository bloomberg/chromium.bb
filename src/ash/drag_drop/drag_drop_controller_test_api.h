// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DRAG_DROP_DRAG_DROP_CONTROLLER_TEST_API_H_
#define ASH_DRAG_DROP_DRAG_DROP_CONTROLLER_TEST_API_H_

#include "ash/ash_export.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "base/macros.h"

namespace ash {

class DragDropControllerTestApi {
 public:
  DragDropControllerTestApi(DragDropController* controller)
      : controller_(controller) {}
  ~DragDropControllerTestApi() = default;

  bool enabled() const { return controller_->enabled_; }

 private:
  DragDropController* controller_;

  DISALLOW_COPY_AND_ASSIGN(DragDropControllerTestApi);
};

}  // namespace ash

#endif  // ASH_DRAG_DROP_DRAG_DROP_CONTROLLER_TEST_API_H_
