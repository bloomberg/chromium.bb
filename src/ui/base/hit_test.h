// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_HIT_TEST_H_
#define UI_BASE_HIT_TEST_H_

#include "build/build_config.h"

#if !defined(OS_WIN)

// Defines the same symbolic names used by the WM_NCHITTEST Notification under
// win32 (the integer values are not guaranteed to be equivalent). We do this
// because we have a whole bunch of code that deals with window resizing and
// such that requires these values.
enum HitTestCompat {
  HTNOWHERE = 0,
  HTBORDER,
  HTBOTTOM,
  HTBOTTOMLEFT,
  HTBOTTOMRIGHT,
  HTCAPTION,
  HTCLIENT,
  HTCLOSE,
  HTERROR,
  HTGROWBOX,
  HTHELP,
  HTHSCROLL,
  HTLEFT,
  HTMENU,
  HTMAXBUTTON,
  HTMINBUTTON,
  HTREDUCE,
  HTRIGHT,
  HTSIZE,
  HTSYSMENU,
  HTTOP,
  HTTOPLEFT,
  HTTOPRIGHT,
  HTTRANSPARENT,
  HTVSCROLL,
  HTZOOM
};

#endif  // !defined(OS_WIN)

#endif  // UI_BASE_HIT_TEST_H_
