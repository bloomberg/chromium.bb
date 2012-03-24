// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SLIDER_H_
#define UI_VIEWS_CONTROLS_SLIDER_H_
#pragma once

#include "ui/base/animation/animation_delegate.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace ui {
class SlideAnimation;
}

namespace views {

class Slider;

enum SliderChangeReason {
  VALUE_CHANGED_BY_USER,  // value was changed by the user (by clicking, e.g.)
  VALUE_CHANGED_BY_API,   // value was changed by a call to SetValue.
};

class VIEWS_EXPORT SliderListener {
 public:
  virtual void SliderValueChanged(Slider* sender,
                                  float value,
                                  float old_value,
                                  SliderChangeReason reason) = 0;

 protected:
  virtual ~SliderListener() {}
};

class VIEWS_EXPORT Slider : public View,
                            public ui::AnimationDelegate {
 public:
  enum Orientation {
    HORIZONTAL,
    VERTICAL
  };

  Slider(SliderListener* listener, Orientation orientation);
  virtual ~Slider();

  float value() const { return value_; }
  void SetValue(float value);

  // Set the delta used for changing the value via keyboard.
  void SetKeyboardIncrement(float increment);

  void SetAccessibleName(const string16& name);

 private:
  void SetValueInternal(float value, SliderChangeReason reason);

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const views::MouseEvent& event) OVERRIDE;
  virtual bool OnKeyPressed(const views::KeyEvent& event) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // ui::AnimationDelegate overrides:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  SliderListener* listener_;
  Orientation orientation_;

  scoped_ptr<ui::SlideAnimation> move_animation_;

  float value_;
  float keyboard_increment_;
  float animating_value_;
  bool value_is_valid_;
  string16 accessible_name_;

  DISALLOW_COPY_AND_ASSIGN(Slider);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SLIDER_H_
