// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/platform_style.h"

#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/focusable_rounded_border_mac.h"
#import "ui/views/controls/scrollbar/cocoa_scroll_bar.h"
#include "ui/views/style/mac/combobox_background_mac.h"
#include "ui/views/style/mac/dialog_button_border_mac.h"

namespace views {

// static
gfx::ImageSkia PlatformStyle::CreateComboboxArrow(bool is_enabled,
                                                  Combobox::Style style) {
  // TODO(ellyjones): IDR_MENU_DROPARROW is a cross-platform image that doesn't
  // look right on Mac. See https://crbug.com/384071.
  if (style == Combobox::STYLE_ACTION) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    return *rb.GetImageSkiaNamed(IDR_MENU_DROPARROW);
  }
  const int kComboboxArrowWidth = 13;
  return gfx::CreateVectorIcon(gfx::VectorIconId::COMBOBOX_ARROW_MAC,
                               kComboboxArrowWidth,
                               is_enabled ? SK_ColorWHITE : SK_ColorBLACK);
}

// static
scoped_ptr<FocusableBorder> PlatformStyle::CreateComboboxBorder() {
  return make_scoped_ptr(new FocusableRoundedBorder);
}

// static
scoped_ptr<Background> PlatformStyle::CreateComboboxBackground() {
  return make_scoped_ptr(new ComboboxBackgroundMac);
}

// static
scoped_ptr<LabelButtonBorder> PlatformStyle::CreateLabelButtonBorder(
    Button::ButtonStyle style) {
  if (style == Button::STYLE_BUTTON)
    return make_scoped_ptr(new DialogButtonBorderMac());

  return make_scoped_ptr(new LabelButtonAssetBorder(style));
}

// static
scoped_ptr<ScrollBar> PlatformStyle::CreateScrollBar(bool is_horizontal) {
  return make_scoped_ptr(new CocoaScrollBar(is_horizontal));
}

}  // namespace views
