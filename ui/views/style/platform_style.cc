// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/platform_style.h"

#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"

namespace views {

#if !defined(OS_MACOSX)
// static
scoped_ptr<LabelButtonBorder> PlatformStyle::CreateLabelButtonBorder(
    Button::ButtonStyle style) {
  return make_scoped_ptr(new LabelButtonAssetBorder(style));
}
#endif

#if !defined(OS_LINUX) || defined(OS_CHROMEOS)
// static
scoped_ptr<Border> PlatformStyle::CreateThemedLabelButtonBorder(
    LabelButton* button) {
  return button->CreateDefaultBorder();
}
#endif

}  // namespace views
