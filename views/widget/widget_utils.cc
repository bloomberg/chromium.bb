// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/widget_utils.h"

#include <string>

#include "views/widget/widget.h"

namespace views {

ThemeProvider* GetWidgetThemeProvider(const Widget* widget) {
  const Widget* root_widget = widget->GetTopLevelWidget();
  if (root_widget && root_widget != widget) {
    // Attempt to get the theme provider, and fall back to the default theme
    // provider if not found.
    ThemeProvider* provider = root_widget->GetThemeProvider();
    if (provider)
      return provider;

    provider = root_widget->GetDefaultThemeProvider();
    if (provider)
      return provider;
  }
  return widget->GetDefaultThemeProvider();
}

}  // namespace views
