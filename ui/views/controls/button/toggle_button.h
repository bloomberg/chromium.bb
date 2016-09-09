// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_TOGGLE_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_TOGGLE_BUTTON_H_

#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/controls/button/custom_button.h"

namespace views {

// This view presents a button that has two states: on and off. This is similar
// to a checkbox but has no text and looks more like a two-state horizontal
// slider.
class VIEWS_EXPORT ToggleButton : public CustomButton {
 public:
  explicit ToggleButton(ButtonListener* listener);
  ~ToggleButton() override;

  void SetIsOn(bool is_on, bool animate);
  bool is_on() const { return is_on_; }

 private:
  // CustomButton:
  gfx::Size GetPreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;
  void NotifyClick(const ui::Event& event) override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;
  std::unique_ptr<InkDropRipple> CreateInkDropRipple() const override;
  SkColor GetInkDropBaseColor() const override;
  bool ShouldShowInkDropHighlight() const override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

  // Calculates the bounding box for the thumb (the circle).
  gfx::Rect GetThumbBounds() const;

  bool is_on_;
  gfx::SlideAnimation slide_animation_;

  DISALLOW_COPY_AND_ASSIGN(ToggleButton);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_TOGGLE_BUTTON_H_
