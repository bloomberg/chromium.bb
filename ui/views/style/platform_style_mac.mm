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
const bool PlatformStyle::kDialogDefaultButtonCanBeCancel = false;
const bool PlatformStyle::kSelectWordOnRightClick = true;
const bool PlatformStyle::kSelectAllOnRightClickWhenUnfocused = true;
const bool PlatformStyle::kTextfieldScrollsToStartOnFocusChange = true;
const bool PlatformStyle::kTreeViewSelectionPaintsEntireRow = true;
const bool PlatformStyle::kUseRipples = false;

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
void PlatformStyle::OnTextfieldEditFailed() {
  NSBeep();
}

}  // namespace views
