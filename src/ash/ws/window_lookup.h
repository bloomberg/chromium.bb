// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WS_WINDOW_LOOKUP_H_
#define ASH_WS_WINDOW_LOOKUP_H_

#include "ash/ash_export.h"
#include "services/ws/common/types.h"

namespace aura {
class Window;
}

// TODO: this file is only necessary for single-process mash and should be
// removed.
namespace ash {
namespace window_lookup {

// Returns true if |window| is a proxy created by the WindowService to represent
// a window created by another client.
ASH_EXPORT bool IsProxyWindow(aura::Window* window);

// Returns the proxy aura::Window that represents a window created with aura
// configured to use mus.
ASH_EXPORT aura::Window* GetProxyWindowForClientWindow(aura::Window* window);

// Returns the client window for a proxy window.
ASH_EXPORT aura::Window* GetClientWindowForProxyWindow(aura::Window* window);

// Returns true if |window| is a proxy for a window created by a client that
// is running in its own process (e.g. shortcut_viewer).
ASH_EXPORT bool IsProxyWindowForOutOfProcess(aura::Window* window);

}  // namespace window_lookup
}  // namespace ash

#endif  // ASH_WS_WINDOW_LOOKUP_H_
