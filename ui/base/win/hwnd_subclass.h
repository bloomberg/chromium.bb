// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_HWND_SUBCLASS_H_
#define UI_BASE_WIN_HWND_SUBCLASS_H_
#pragma once

#include <windows.h>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/ui_export.h"
#include "ui/base/view_prop.h"

namespace ui {

// Classes implementing this interface get the opportunity to handle and consume
// messages before they are sent to their target HWND.
class UI_EXPORT HWNDMessageFilter {
 public:
  virtual ~HWNDMessageFilter() {}

  // A derived class overrides this method to perform filtering of the messages
  // before the |original_wnd_proc_| sees them. Return true to consume the
  // message and prevent |original_wnd_proc_| from seeing them at all, false to
  // allow it to process them.
  virtual bool FilterMessage(HWND hwnd,
                             UINT message,
                             WPARAM w_param,
                             LPARAM l_param,
                             LRESULT* l_result) = 0;
};

// An object that instance-subclasses a window. If the window has already been
// instance-subclassed, that subclassing is lost.
class UI_EXPORT HWNDSubclass {
 public:
  explicit HWNDSubclass(HWND target);
  ~HWNDSubclass();

  // HWNDSubclass takes ownership of the filter.
  void SetFilter(HWNDMessageFilter* filter);

  LRESULT OnWndProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);

 private:
  HWND target_;
  scoped_ptr<HWNDMessageFilter> filter_;
  WNDPROC original_wnd_proc_;
  ui::ViewProp prop_;

  DISALLOW_COPY_AND_ASSIGN(HWNDSubclass);
};

}  // namespace ui

#endif  // UI_BASE_WIN_HWND_SUBCLASS_H_
