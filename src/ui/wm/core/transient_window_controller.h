// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_TRANSIENT_WINDOW_CONTROLLER_H_
#define UI_WM_CORE_TRANSIENT_WINDOW_CONTROLLER_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/wm/core/wm_core_export.h"

namespace wm {

class TransientWindowManager;

// TransientWindowClient implementation. Uses TransientWindowManager to handle
// tracking transient per window.
class WM_CORE_EXPORT TransientWindowController
    : public aura::client::TransientWindowClient {
 public:
  TransientWindowController();
  ~TransientWindowController() override;

  // Returns the single TransientWindowController instance.
  static TransientWindowController* Get() { return instance_; }

  // TransientWindowClient:
  void AddTransientChild(aura::Window* parent, aura::Window* child) override;
  void RemoveTransientChild(aura::Window* parent, aura::Window* child) override;
  aura::Window* GetTransientParent(aura::Window* window) override;
  const aura::Window* GetTransientParent(const aura::Window* window) override;
  std::vector<aura::Window*> GetTransientChildren(
      const aura::Window* parent) override;
  void AddObserver(
      aura::client::TransientWindowClientObserver* observer) override;
  void RemoveObserver(
      aura::client::TransientWindowClientObserver* observer) override;

 private:
  friend class TransientWindowManager;

  static TransientWindowController* instance_;

  base::ObserverList<aura::client::TransientWindowClientObserver>::Unchecked
      observers_;

  DISALLOW_COPY_AND_ASSIGN(TransientWindowController);
};

}  // namespace wm

#endif  // UI_WM_CORE_TRANSIENT_WINDOW_CONTROLLER_H_
