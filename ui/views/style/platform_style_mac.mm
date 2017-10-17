// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/platform_style.h"

#include "base/memory/ptr_util.h"
#include "ui/base/ui_features.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/controls/button/label_button.h"
#import "ui/views/controls/scrollbar/cocoa_scroll_bar.h"

#import <Cocoa/Cocoa.h>

namespace views {

const int PlatformStyle::kMinLabelButtonWidth = 32;
const int PlatformStyle::kMinLabelButtonHeight = 30;
const bool PlatformStyle::kDefaultLabelButtonHasBoldFont = false;
const bool PlatformStyle::kDialogDefaultButtonCanBeCancel = false;
const bool PlatformStyle::kSelectWordOnRightClick = true;
const bool PlatformStyle::kSelectAllOnRightClickWhenUnfocused = true;
const bool PlatformStyle::kTextfieldScrollsToStartOnFocusChange = true;
const bool PlatformStyle::kTreeViewSelectionPaintsEntireRow = true;
const bool PlatformStyle::kUseRipples = false;

// On Mac, the Cocoa browser window does not flip its UI in RTL (e.g. bookmark
// star remains on the right, padlock on the left). So bubbles should open in
// the same direction as in LTR by default, unless the entire browser is views.
const bool PlatformStyle::kMirrorBubbleArrowInRTLByDefault =
    BUILDFLAG(MAC_VIEWS_BROWSER);

const Button::NotifyAction PlatformStyle::kMenuNotifyActivationAction =
    Button::NOTIFY_ON_PRESS;

const Button::KeyClickAction PlatformStyle::kKeyClickActionOnSpace =
    Button::CLICK_ON_KEY_PRESS;

// On Mac, the Return key is used to perform the default action even when a
// control is focused.
const bool PlatformStyle::kReturnClicksFocusedControl = false;

// static
std::unique_ptr<ScrollBar> PlatformStyle::CreateScrollBar(bool is_horizontal) {
  return std::make_unique<CocoaScrollBar>(is_horizontal);
}

// static
SkColor PlatformStyle::TextColorForButton(
    const ButtonColorByState& color_by_state,
    const LabelButton& button) {
  Button::ButtonState state = button.state();
  if (button.style() == Button::STYLE_BUTTON && button.is_default()) {
    // For convenience, we currently assume Mac wants the color corresponding to
    // the pressed state for default buttons.
    state = Button::STATE_PRESSED;
  }
  return color_by_state[state];
}

// static
void PlatformStyle::ApplyLabelButtonTextStyle(
    views::Label* label,
    ButtonColorByState* color_by_state) {
  ButtonColorByState& colors = *color_by_state;
  colors[Button::STATE_PRESSED] = SK_ColorWHITE;
}

// static
void PlatformStyle::OnTextfieldEditFailed() {
  NSBeep();
}

}  // namespace views
