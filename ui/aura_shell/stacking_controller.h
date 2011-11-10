// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_STACKING_CONTROLLER_H_
#define UI_AURA_SHELL_STACKING_CONTROLLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/stacking_client.h"

namespace aura_shell {
namespace internal {

class StackingController : public aura::StackingClient {
 public:
  StackingController();
  virtual ~StackingController();

  // Returns true if |window| exists within a container that supports
  // activation.
  static aura::Window* GetActivatableWindow(aura::Window* window);

  // Overridden from aura::StackingClient:
  virtual void AddChildToDefaultParent(aura::Window* window) OVERRIDE;
  virtual bool CanActivateWindow(aura::Window* window) const OVERRIDE;
  virtual aura::Window* GetTopmostWindowToActivate(
      aura::Window* ignore) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(StackingController);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_STACKING_CONTROLLER_H_
