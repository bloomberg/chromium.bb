// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_EVENT_TRANSFORMATION_HANDLER_H_
#define ASH_DISPLAY_EVENT_TRANSFORMATION_HANDLER_H_

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/events/event_handler.h"

namespace ash {

// An event filter that transforms input event properties in extended desktop
// environment.
class ASH_EXPORT EventTransformationHandler : public ui::EventHandler {
 public:
  enum TransformationMode {
    TRANSFORM_AUTO,  // Transform events by the default amount.
                     // 1. Linear scaling w.r.t. the device scale factor.
                     // 2. Add 20% more for non-integrated displays.
    TRANSFORM_NONE,  // No transformation.
  };

  EventTransformationHandler();
  ~EventTransformationHandler() override;

  void set_transformation_mode(TransformationMode transformation_mode) {
    transformation_mode_ = transformation_mode;
  }

  // Overridden from ui::EventHandler.
  void OnScrollEvent(ui::ScrollEvent* event) override;

 private:
  TransformationMode transformation_mode_;

  DISALLOW_COPY_AND_ASSIGN(EventTransformationHandler);
};

}  // namespace ash

#endif  // ASH_DISPLAY_EVENT_TRANSFORMATION_HANDLER_H_
