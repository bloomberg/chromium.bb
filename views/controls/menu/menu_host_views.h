// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_MENU_MENU_HOST_VIEWS_H_
#define VIEWS_CONTROLS_MENU_MENU_HOST_VIEWS_H_
#pragma once

#include "views/controls/menu/native_menu_host.h"
#include "views/widget/native_widget_views.h"

namespace views {
namespace internal {
class NativeMenuHostDelegate;
}

// MenuHost implementation for views.
class MenuHostViews : public NativeWidgetViews,
                      public NativeMenuHost {
 public:
  explicit MenuHostViews(internal::NativeMenuHostDelegate* delegate);
  virtual ~MenuHostViews();

 private:
  // Overridden from NativeMenuHost:
  virtual void StartCapturing() OVERRIDE;
  virtual NativeWidget* AsNativeWidget() OVERRIDE;

  // Overridden from NativeWidgetViews:
  virtual void InitNativeWidget(const Widget::InitParams& params) OVERRIDE;

  internal::NativeMenuHostDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(MenuHostViews);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_MENU_HOST_VIEWS_H_

