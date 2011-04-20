// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_WIDGET_DELEGATE_H_
#define VIEWS_WIDGET_WIDGET_DELEGATE_H_
#pragma once

namespace views {

class View;

// WidgetDelegate interface
// Handles events on Widgets in context-specific ways.
class WidgetDelegate {
 public:
  // Called whenever the widget is activated or deactivated.
  // TODO(beng): This should be consolidated with
  //             WindowDelegate::OnWindowActivationChanged().
  virtual void OnWidgetActivated(bool active);

  // Called whenever the widget's position changes.
  virtual void OnWidgetMove();

  // Called with the display changes (color depth or resolution).
  virtual void OnDisplayChanged();

  // Called when the work area (the desktop area minus task bars,
  // menu bars, etc.) changes in size.
  virtual void OnWorkAreaChanged();

  // Returns the view that should have the focus when the widget is shown.  If
  // NULL no view is focused.
  virtual View* GetInitiallyFocusedView();

 protected:
  virtual ~WidgetDelegate() {}
};

}  // namespace views

#endif  // VIEWS_WIDGET_WIDGET_DELEGATE_H_

