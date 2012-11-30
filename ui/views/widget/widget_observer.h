// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_WIDGET_OBSERVER_H_
#define UI_VIEWS_WIDGET_WIDGET_OBSERVER_H_

#include "ui/views/views_export.h"

namespace views {

class Widget;

// Observers can listen to various events on the Widgets.
class VIEWS_EXPORT WidgetObserver {
 public:
  virtual void OnWidgetClosing(Widget* widget) {}

  virtual void OnWidgetVisibilityChanged(Widget* widget, bool visible) {}

  virtual void OnWidgetActivationChanged(Widget* widget, bool active) {}

  virtual void OnWidgetMoved(Widget* widget) {}

 protected:
  virtual ~WidgetObserver() {}
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_WIDGET_OBSERVER_H_
