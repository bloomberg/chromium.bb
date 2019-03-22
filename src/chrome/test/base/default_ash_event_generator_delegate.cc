// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/default_ash_event_generator_delegate.h"

#include "ash/shell.h"
#include "base/macros.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/test/default_event_generator_delegate.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/mus/mus_client.h"

namespace {

class DefaultAshEventGeneratorDelegate
    : public aura::test::DefaultEventGeneratorDelegate {
 public:
  explicit DefaultAshEventGeneratorDelegate(aura::Window* root_window);
  ~DefaultAshEventGeneratorDelegate() override = default;

  // aura::test::DefaultEventGeneratorDelegate:
  ui::EventDispatchDetails DispatchKeyEventToIME(ui::EventTarget* target,
                                                 ui::KeyEvent* event) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultAshEventGeneratorDelegate);
};

DefaultAshEventGeneratorDelegate::DefaultAshEventGeneratorDelegate(
    aura::Window* root_window)
    : DefaultEventGeneratorDelegate(root_window) {}

ui::EventDispatchDetails
DefaultAshEventGeneratorDelegate::DispatchKeyEventToIME(ui::EventTarget* target,
                                                        ui::KeyEvent* event) {
  return ui::EventDispatchDetails();
}

}  // namespace

std::unique_ptr<ui::test::EventGeneratorDelegate>
CreateAshEventGeneratorDelegate(ui::test::EventGenerator* owner,
                                gfx::NativeWindow root_window,
                                gfx::NativeWindow window) {
  // Tests should not create event generators for a "root window" that's not
  // actually the root window.
  if (root_window)
    DCHECK_EQ(root_window, root_window->GetRootWindow());

  // Do not create EventGeneratorDelegateMus if a root window is supplied.
  // Assume that if a root is supplied the event generator should target the
  // specified window, and there is no need to dispatch remotely.
  if (features::IsUsingWindowService() && !root_window) {
    DCHECK(views::MusClient::Exists());
    return aura::test::EventGeneratorDelegateAura::Create(
        views::MusClient::Get()->window_tree_client()->connector(), owner,
        root_window, window);
  }
  return std::make_unique<DefaultAshEventGeneratorDelegate>(root_window);
}
