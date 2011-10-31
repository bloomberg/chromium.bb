// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TYPES_H_
#define UI_AURA_WINDOW_TYPES_H_
#pragma once

namespace aura {

enum WindowType {
  WINDOW_TYPE_UNKNOWN = 0,

  // Regular windows that should be laid out by the shell.
  WINDOW_TYPE_NORMAL,

  // Miscellaneous windows that should not be laid out by the shell.
  WINDOW_TYPE_POPUP,

  WINDOW_TYPE_MENU,
  WINDOW_TYPE_TOOLTIP,
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TYPES_H_
