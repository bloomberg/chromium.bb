// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_DELEGATE_CLASSIC_H_
#define ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_DELEGATE_CLASSIC_H_

#include "ash/ash_export.h"

class AccountId;

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Delegate used in classic and single-process mash mode. This delegate is
// notified when the state of a window changes. In classic mode, this delegate
// is called for all windows, in single-process mash mode, this delegate is
// called for Windows created in ash's window hierarchy (more specifically,
// Windows whose aura::Env has a mode of aura::Mode::LOCAL). For example, Arc
// creates Windows that are parented directly to Ash's window hierarchy, changes
// to such windows are called through this delegate.
class ASH_EXPORT MultiUserWindowManagerDelegateClassic {
 public:
  // Called when the owner of the window tracked by the manager is changed.
  // |was_minimized| is true if the window was minimized. |teleported| is true
  // if the window was not on the desktop of the current user.
  virtual void OnOwnerEntryChanged(aura::Window* window,
                                   const AccountId& account_id,
                                   bool was_minimized,
                                   bool teleported) {}

 protected:
  virtual ~MultiUserWindowManagerDelegateClassic() {}
};

}  // namespace ash

#endif  // ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_DELEGATE_CLASSIC_H_
