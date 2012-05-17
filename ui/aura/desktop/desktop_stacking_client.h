// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_DESKTOP_DESKTOP_STACKING_CLIENT_H_
#define UI_AURA_DESKTOP_DESKTOP_STACKING_CLIENT_H_

#include "ui/aura/client/stacking_client.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/aura_export.h"

namespace aura {
class RootWindow;
class Window;

// A stacking client for the desktop; always sets the default parent to the
// RootWindow of the passed in Window.
class AURA_EXPORT DesktopStackingClient : public client::StackingClient {
 public:
  DesktopStackingClient();
  virtual ~DesktopStackingClient();

  // Overridden from client::StackingClient:
  virtual Window* GetDefaultParent(Window* window) OVERRIDE;

 private:
  // Windows with NULL parents are parented to this.
  scoped_ptr<aura::RootWindow> null_parent_;

  DISALLOW_COPY_AND_ASSIGN(DesktopStackingClient);
};

}  // namespace aura

#endif  // UI_AURA_DESKTOP_DESKTOP_STACKING_CLIENT_H_
