// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_DEFAULT_ACTIVATION_CLIENT_H_
#define UI_AURA_CLIENT_DEFAULT_ACTIVATION_CLIENT_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/window_observer.h"

namespace aura {
namespace client {
class ActivationChangeObserver;
}

namespace client {

// Simple ActivationClient implementation for use by tests and other targets
// that just need basic behavior (e.g. activate windows whenever requested,
// restack windows at the top when they're activated, etc.).
class AURA_EXPORT DefaultActivationClient : public client::ActivationClient,
                                            public WindowObserver {
 public:
  explicit DefaultActivationClient(Window* root_window);
  virtual ~DefaultActivationClient();

  // Overridden from client::ActivationClient:
  virtual void AddObserver(client::ActivationChangeObserver* observer) OVERRIDE;
  virtual void RemoveObserver(
      client::ActivationChangeObserver* observer) OVERRIDE;
  virtual void ActivateWindow(Window* window) OVERRIDE;
  virtual void DeactivateWindow(Window* window) OVERRIDE;
  virtual Window* GetActiveWindow() OVERRIDE;
  virtual Window* GetActivatableWindow(Window* window) OVERRIDE;
  virtual Window* GetToplevelWindow(Window* window) OVERRIDE;
  virtual bool OnWillFocusWindow(Window* window,
                                 const ui::Event* event) OVERRIDE;
  virtual bool CanActivateWindow(Window* window) const OVERRIDE;

  // Overridden from WindowObserver:
  virtual void OnWindowDestroyed(Window* window) OVERRIDE;

 private:
  void RemoveActiveWindow(Window* window);

  // This class explicitly does NOT store the active window in a window property
  // to make sure that ActivationChangeObserver is not treated as part of the
  // aura API. Assumptions to that end will cause tests that use this client to
  // fail.
  std::vector<Window*> active_windows_;

  // The window which was active before the currently active one.
  Window* last_active_;

  ObserverList<client::ActivationChangeObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(DefaultActivationClient);
};

}  // namespace client
}  // namespace aura

#endif  // UI_AURA_CLIENT_DEFAULT_ACTIVATION_CLIENT_H_
