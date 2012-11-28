// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_STACKING_CLIENT_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_STACKING_CLIENT_H_

#include "ui/aura/client/stacking_client.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/views_export.h"

namespace aura {
class RootWindow;
class Window;
namespace client {
class DefaultCaptureClient;
class FocusClient;
}
}

namespace views {
class DesktopActivationClient;
namespace corewm {
class CompoundEventFilter;
class InputMethodEventFilter;
}

// A stacking client for the desktop; always sets the default parent to the
// RootWindow of the passed in Window.
class VIEWS_EXPORT DesktopStackingClient : public aura::client::StackingClient {
 public:
  DesktopStackingClient();
  virtual ~DesktopStackingClient();

  // Overridden from aura::client::StackingClient:
  virtual aura::Window* GetDefaultParent(aura::Window* context,
                                         aura::Window* window,
                                         const gfx::Rect& bounds) OVERRIDE;

 private:
  void CreateNULLParent();

  // Windows with NULL parents are parented to this.
  scoped_ptr<aura::RootWindow> null_parent_;

  // All the member variables below are necessary for the NULL parent root
  // window to function.
  scoped_ptr<aura::client::FocusClient> focus_client_;
  // Depends on focus_manager_.
  scoped_ptr<DesktopActivationClient> activation_client_;

  scoped_ptr<corewm::InputMethodEventFilter> input_method_filter_;
  corewm::CompoundEventFilter* window_event_filter_;

  scoped_ptr<aura::client::DefaultCaptureClient> capture_client_;

  DISALLOW_COPY_AND_ASSIGN(DesktopStackingClient);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_STACKING_CLIENT_H_
