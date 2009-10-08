// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "views/window/window.h"

#include "app/gfx/font.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/gfx/size.h"
#include "base/string_util.h"
#include "views/widget/widget.h"

namespace views {

// static
int Window::GetLocalizedContentsWidthForFont(int col_resource_id,
                                             const gfx::Font& font) {
  double chars = 0;
  StringToDouble(WideToUTF8(l10n_util::GetString(col_resource_id)), &chars);
  int width = font.GetExpectedTextWidth(static_cast<int>(chars));
  DCHECK(width > 0);
  return width;
}

// static
int Window::GetLocalizedContentsWidth(int col_resource_id) {
  return GetLocalizedContentsWidthForFont(col_resource_id,
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont));
}

// static
int Window::GetLocalizedContentsHeightForFont(int row_resource_id,
                                              const gfx::Font& font) {
  double lines = 0;
  StringToDouble(WideToUTF8(l10n_util::GetString(row_resource_id)), &lines);
  int height = static_cast<int>(font.height() * lines);
  DCHECK(height > 0);
  return height;
}

// static
int Window::GetLocalizedContentsHeight(int row_resource_id) {
  return GetLocalizedContentsHeightForFont(row_resource_id,
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont));
}

gfx::Size Window::GetLocalizedContentsSizeForFont(int col_resource_id,
                                                  int row_resource_id,
                                                  const gfx::Font& font) {
  return gfx::Size(GetLocalizedContentsWidthForFont(col_resource_id, font),
                   GetLocalizedContentsHeightForFont(row_resource_id, font));
}

// static
gfx::Size Window::GetLocalizedContentsSize(int col_resource_id,
                                           int row_resource_id) {
  return gfx::Size(GetLocalizedContentsWidth(col_resource_id),
                   GetLocalizedContentsHeight(row_resource_id));
}

// static
void Window::CloseSecondaryWidget(Widget* widget) {
  if (!widget)
    return;

  // Close widget if it's identified as a secondary window.
  Window* window = widget->GetWindow();
  if (window) {
    if (!window->IsAppWindow())
      window->Close();
  } else {
    // If it's not a Window, then close it anyway since it probably is
    // secondary.
    widget->Close();
  }
}

}  // namespace views
