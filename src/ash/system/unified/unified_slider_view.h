// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SLIDER_VIEW_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SLIDER_VIEW_H_

#include "ash/system/unified/top_shortcut_button.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/slider.h"
#include "ui/views/view.h"

namespace views {
class Label;
}

namespace ash {

class UnifiedSliderListener : public views::SliderListener {
 public:
  // Instantiates UnifiedSliderView. The view will be onwed by views hierarchy.
  // The view should be always deleted after the controller is destructed.
  virtual views::View* CreateView() = 0;

  ~UnifiedSliderListener() override = default;
};

// A button used in a slider row of UnifiedSystemTray. The button is togglable.
class UnifiedSliderButton : public views::ImageButton {
 public:
  UnifiedSliderButton(PressedCallback callback,
                      const gfx::VectorIcon& icon,
                      int accessible_name_id);

  UnifiedSliderButton(const UnifiedSliderButton&) = delete;
  UnifiedSliderButton& operator=(const UnifiedSliderButton&) = delete;

  ~UnifiedSliderButton() override;

  // Set the vector icon shown in a circle.
  void SetVectorIcon(const gfx::VectorIcon& icon);

  // Change the toggle state.
  void SetToggled(bool toggled);

  // views::ImageButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  const char* GetClassName() const override;
  void OnThemeChanged() override;

 private:
  void UpdateVectorIcon();

  // True if the button is currently toggled.
  bool toggled_ = false;

  const gfx::VectorIcon* icon_ = nullptr;
};

// Base view class of a slider row in UnifiedSystemTray. It has a button on the
// left side and a slider on the right side.
class UnifiedSliderView : public views::View {
 public:
  // If |readonly| is set, the slider will not accept any user events.
  UnifiedSliderView(views::Button::PressedCallback callback,
                    UnifiedSliderListener* listener,
                    const gfx::VectorIcon& icon,
                    int accessible_name_id,
                    bool readonly = false);

  UnifiedSliderView(const UnifiedSliderView&) = delete;
  UnifiedSliderView& operator=(const UnifiedSliderView&) = delete;

  ~UnifiedSliderView() override;

  UnifiedSliderButton* button() { return button_; }
  views::Slider* slider() { return slider_; }
  views::Label* toast_label() { return toast_label_; }

  // Sets a slider value. If |by_user| is false, accessibility events will not
  // be triggered.
  void SetSliderValue(float value, bool by_user);

  // views::View:
  const char* GetClassName() const override;
  void OnThemeChanged() override;

 protected:
  void CreateToastLabel();

 private:
  // Unowned. Owned by views hierarchy.
  UnifiedSliderButton* const button_;
  views::Slider* const slider_;
  views::Label* toast_label_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SLIDER_VIEW_H_
