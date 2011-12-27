// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TYPES_H_
#define UI_AURA_WINDOW_TYPES_H_
#pragma once

namespace aura {
namespace client {

// This isn't a property because it can't change after the window has been
// initialized. It's in client because the Aura Client application derives
// meaning from these values, not Aura itself.
enum WindowType {
  WINDOW_TYPE_UNKNOWN = 0,

  // Regular windows that should be laid out by the client.
  WINDOW_TYPE_NORMAL,

  // Always on top windows aligned to bottom right of screen.
  WINDOW_TYPE_PANEL,

  // Miscellaneous windows that should not be laid out by the shell.
  WINDOW_TYPE_POPUP,

  WINDOW_TYPE_MENU,
  WINDOW_TYPE_TOOLTIP,
};

}  // namespace client
}  // namespace aura

#endif  // UI_AURA_WINDOW_TYPES_H_
