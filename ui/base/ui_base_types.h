// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_UI_BASE_TYPES_H_
#define UI_BASE_UI_BASE_TYPES_H_
#pragma once

namespace ui {

// Window "show" state.  These values are written to disk so should not be
// changed.
enum WindowShowState {
  // A default un-set state.
  SHOW_STATE_DEFAULT    = 0,
  SHOW_STATE_NORMAL     = 1,
  SHOW_STATE_MINIMIZED  = 2,
  SHOW_STATE_MAXIMIZED  = 3,
  SHOW_STATE_MAX        = 4
};

}  // namespace ui

#endif  // UI_BASE_UI_BASE_TYPES_H_
