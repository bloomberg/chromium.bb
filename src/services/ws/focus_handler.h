// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_FOCUS_HANDLER_H_
#define SERVICES_WS_FOCUS_HANDLER_H_

#include "base/macros.h"
#include "ui/aura/client/focus_change_observer.h"

namespace aura {
class Window;
}

namespace ws {

class ProxyWindow;
class WindowTree;

// FocusHandler handles focus requests from the client, as well as notifying
// the client when focus changes.
class FocusHandler : public aura::client::FocusChangeObserver {
 public:
  explicit FocusHandler(WindowTree* window_tree);
  ~FocusHandler() override;

  // Sets focus to |window| (which may be null). Returns true on success.
  bool SetFocus(aura::Window* window);

  // Sets whether |window| can be focused.
  void SetCanFocus(aura::Window* window, bool can_focus);

 private:
  // Returns true if |window| can be focused.
  bool IsFocusableWindow(aura::Window* window) const;

  bool IsEmbeddedClient(ProxyWindow* proxy_window) const;
  bool IsOwningClient(ProxyWindow* proxy_window) const;

  // aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  WindowTree* window_tree_;

  DISALLOW_COPY_AND_ASSIGN(FocusHandler);
};

}  // namespace ws

#endif  // SERVICES_WS_FOCUS_HANDLER_H_
