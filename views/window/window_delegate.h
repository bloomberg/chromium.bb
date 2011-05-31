// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WINDOW_WINDOW_DELEGATE_H_
#define VIEWS_WINDOW_WINDOW_DELEGATE_H_
#pragma once

#include "views/widget/widget_delegate.h"

namespace views {

///////////////////////////////////////////////////////////////////////////////
//
// WindowDelegate
//
//  WindowDelegate is an interface implemented by objects that wish to show a
//  Window. The window that is displayed uses this interface to determine how
//  it should be displayed and notify the delegate object of certain events.
//
class WindowDelegate : public WidgetDelegate {
 public:
  WindowDelegate();
  virtual ~WindowDelegate();

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowDelegate);
};

}  // namespace views

#endif  // VIEWS_WINDOW_WINDOW_DELEGATE_H_
