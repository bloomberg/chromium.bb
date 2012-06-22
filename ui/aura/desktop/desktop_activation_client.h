// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_DESKTOP_DESTKOP_ACTIVATION_CLIENT_H_
#define UI_AURA_DESKTOP_DESTKOP_ACTIVATION_CLIENT_H_
#pragma once

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/focus_change_observer.h"
#include "ui/aura/root_window_observer.h"
#include "ui/aura/window_observer.h"

namespace aura {
class FocusManager;
namespace client {
class ActivationChangeObserver;
}

// An activation client that handles activation events in a single
// RootWindow. Used only on the Desktop where there can be multiple RootWindow
// objects.
class AURA_EXPORT DesktopActivationClient : public client::ActivationClient,
                                            public WindowObserver,
                                            public EnvObserver,
                                            public FocusChangeObserver {
 public:
  explicit DesktopActivationClient(FocusManager* focus_manager);
  virtual ~DesktopActivationClient();

  // ActivationClient:
  virtual void AddObserver(client::ActivationChangeObserver* observer) OVERRIDE;
  virtual void RemoveObserver(
      client::ActivationChangeObserver* observer) OVERRIDE;
  virtual void ActivateWindow(Window* window) OVERRIDE;
  virtual void DeactivateWindow(Window* window) OVERRIDE;
  virtual aura::Window* GetActiveWindow() OVERRIDE;
  virtual bool OnWillFocusWindow(Window* window, const Event* event) OVERRIDE;
  virtual bool CanActivateWindow(Window* window) const OVERRIDE;

  // Overridden from aura::WindowObserver:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

  // Overridden from aura::EnvObserver:
  virtual void OnWindowInitialized(aura::Window* window) OVERRIDE;

  // Overridden from aura::FocusChangeObserver:
  virtual void OnWindowFocused(aura::Window* window) OVERRIDE;

 protected:
  // Walks up the chain to find the correct parent window to activate when we
  // try to activate |window|.
  aura::Window* GetActivatableWindow(aura::Window* window);

  // The focus manager that we collaborate with.
  FocusManager* focus_manager_;

  // The current active window.
  Window* current_active_;

  // True inside ActivateWindow(). Used to prevent recursion of focus
  // change notifications causing activation.
  bool updating_activation_;

  ObserverList<client::ActivationChangeObserver> observers_;

  ScopedObserver<aura::Window, aura::WindowObserver> observer_manager_;

  DISALLOW_COPY_AND_ASSIGN(DesktopActivationClient);
};

}  // namespace aura

#endif  // UI_AURA_DESKTOP_DESTKOP_ACTIVATION_CLIENT_H_
