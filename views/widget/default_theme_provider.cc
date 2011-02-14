// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/default_theme_provider.h"

#include "ui/base/resource/resource_bundle.h"

#if defined(OS_WIN)
#include "views/widget/widget_win.h"
#endif

namespace views {

SkBitmap* DefaultThemeProvider::GetBitmapNamed(int id) const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(id);
}

bool DefaultThemeProvider::ShouldUseNativeFrame() const {
#if defined(OS_WIN)
  return WidgetWin::IsAeroGlassEnabled();
#else
  return false;
#endif
}

}  // namespace views
