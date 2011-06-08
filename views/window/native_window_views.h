// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WINDOW_NATIVE_WINDOW_VIEWS_H_
#define VIEWS_WINDOW_NATIVE_WINDOW_VIEWS_H_
#pragma once

#include "base/message_loop.h"
#include "views/window/native_window.h"
#include "views/widget/native_widget_views.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeWindowViews
//
//  A NativeWindow implementation that uses another View as its native widget.
//
class NativeWindowViews : public NativeWidgetViews,
                          public NativeWindow {
 public:
  NativeWindowViews(View* host, internal::NativeWindowDelegate* delegate);
  virtual ~NativeWindowViews();

 private:
  // Overridden from NativeWindow:
  virtual Window* GetWindow() OVERRIDE;
  virtual const Window* GetWindow() const OVERRIDE;
  virtual NativeWidget* AsNativeWidget() OVERRIDE;
  virtual const NativeWidget* AsNativeWidget() const OVERRIDE;
  virtual void BecomeModal() OVERRIDE;

  internal::NativeWindowDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(NativeWindowViews);
};

}

#endif  // VIEWS_WINDOW_NATIVE_WINDOW_VIEWS_H_
