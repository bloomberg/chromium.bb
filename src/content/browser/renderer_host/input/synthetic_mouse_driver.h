// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_MOUSE_DRIVER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_MOUSE_DRIVER_H_

#include "base/macros.h"
#include "content/browser/renderer_host/input/synthetic_pointer_driver.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_web_input_event_builders.h"

namespace content {

class CONTENT_EXPORT SyntheticMouseDriver : public SyntheticPointerDriver {
 public:
  SyntheticMouseDriver();
  ~SyntheticMouseDriver() override;

  void DispatchEvent(SyntheticGestureTarget* target,
                     const base::TimeTicks& timestamp) override;

  void Press(
      float x,
      float y,
      int index = 0,
      SyntheticPointerActionParams::Button button =
          SyntheticPointerActionParams::Button::LEFT,
      int key_modifiers = 0,
      float width = 1.f,
      float height = 1.f,
      float rotation_angle = 0.f,
      float force = 1.f,
      const base::TimeTicks& timestamp = base::TimeTicks::Now()) override;
  void Move(float x,
            float y,
            int index = 0,
            int key_modifiers = 0,
            float width = 1.f,
            float height = 1.f,
            float rotation_angle = 0.f,
            float force = 1.f) override;
  void Release(int index = 0,
               SyntheticPointerActionParams::Button button =
                   SyntheticPointerActionParams::Button::LEFT,
               int key_modifiers = 0) override;
  void Cancel(int index = 0,
              SyntheticPointerActionParams::Button button =
                  SyntheticPointerActionParams::Button::LEFT,
              int key_modifiers = 0) override;
  void Leave(int index = 0) override;

  bool UserInputCheck(
      const SyntheticPointerActionParams& params) const override;

 protected:
  blink::WebMouseEvent mouse_event_;
  unsigned last_modifiers_ = 0;

 private:
  bool IsRepeatedClickEvent(const base::TimeTicks& timestamp, float x, float y);
  int click_count_ = 0;
  base::TimeTicks last_mouse_click_time_ = base::TimeTicks::Now();
  float last_x_ = 0;
  float last_y_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SyntheticMouseDriver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_MOUSE_DRIVER_H_
