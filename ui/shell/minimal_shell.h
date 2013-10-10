// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_MINIMAL_SHELL_H_
#define UI_SHELL_MINIMAL_SHELL_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/stacking_client.h"

namespace aura {
class RootWindow;
class Window;
namespace client {
class DefaultActivationClient;
class DefaultCaptureClient;
class FocusClient;
}  // class client
}  // class aura

namespace gfx {
class Rect;
class Size;
}  // class gfx

namespace views {
namespace corewm {
class CompoundEventFilter;
class InputMethodEventFilter;
}  // corewm
}  // views

namespace shell {

// Creates a minimal environment for running the shell. We can't pull in all of
// ash here, but we can create attach several of the same things we'd find in
// the ash parts of the code.
class MinimalShell : public aura::client::StackingClient {
 public:
  explicit MinimalShell(const gfx::Size& default_window_size);
  virtual ~MinimalShell();

  aura::RootWindow* root_window() { return root_window_.get(); }

  // Overridden from client::StackingClient:
  virtual aura::Window* GetDefaultParent(aura::Window* context,
                                         aura::Window* window,
                                         const gfx::Rect& bounds) OVERRIDE;

 private:
  scoped_ptr<aura::RootWindow> root_window_;

  // Owned by RootWindow
  views::corewm::CompoundEventFilter* root_window_event_filter_;

  scoped_ptr<aura::client::DefaultCaptureClient> capture_client_;
  scoped_ptr<views::corewm::InputMethodEventFilter> input_method_filter_;
  scoped_ptr<aura::client::DefaultActivationClient> activation_client_;
  scoped_ptr<aura::client::FocusClient> focus_client_;

  DISALLOW_COPY_AND_ASSIGN(MinimalShell);
};

}  // namespace shell

#endif  // UI_SHELL_MINIMAL_SHELL_H_
