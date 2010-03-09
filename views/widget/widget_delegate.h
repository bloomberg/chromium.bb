// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_WIDGET_DELEGATE_H_
#define VIEWS_WIDGET_WIDGET_DELEGATE_H_

namespace views {

// WidgetDelegate interface
// Handles events on Widgets in context-specific ways.
class WidgetDelegate {
 public:
  virtual ~WidgetDelegate() {}

  // Called with the display changes (color depth or resolution).
  virtual void DisplayChanged() = 0;
};

}  // namespace views

#endif  // VIEWS_WIDGET_WIDGET_DELEGATE_H_

