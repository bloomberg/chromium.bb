// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_WIDGET_UTILS_H_
#define VIEWS_WIDGET_WIDGET_UTILS_H_
#pragma once

namespace ui {
class ThemeProvider;
}
using ui::ThemeProvider;

namespace views {

class Widget;

ThemeProvider* GetWidgetThemeProvider(const Widget* widget);

}  // namespace views

#endif  // VIEWS_WIDGET_WIDGET_UTILS_H_
