// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/shell_accelerator_controller.h"

#include "ui/aura/desktop.h"
#include "ui/aura/event.h"
#include "ui/aura_shell/shell.h"
#include "ui/base/accelerator_manager.h"
#include "ui/base/models/accelerator.h"
#include "ui/gfx/compositor/layer_animation_sequence.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "ui/gfx/compositor/screen_rotation.h"

namespace {

// Acceleraters handled by ShellAcceleratorController.
struct AcceleratorData {
  ui::KeyboardCode keycode;
  bool shift;
  bool ctrl;
  bool alt;
} kAcceleratorData[] = {
  { ui::VKEY_F11, false, false, false },
  { ui::VKEY_HOME, false, true, false },
};

// Registers the accelerators with ShellAcceleratorController.
void RegisterAccelerators(aura_shell::ShellAcceleratorController* controller) {
  for (size_t i = 0; i < arraysize(kAcceleratorData); ++i) {
    controller->Register(ui::Accelerator(kAcceleratorData[i].keycode,
                                         kAcceleratorData[i].shift,
                                         kAcceleratorData[i].ctrl,
                                         kAcceleratorData[i].alt),
                         controller);
  }
}

#if !defined(NDEBUG)
// Rotates the screen.
void RotateScreen() {
  static int i = 0;
  int delta = 0;
  switch (i) {
    case 0: delta = 90; break;
    case 1: delta = 90; break;
    case 2: delta = 90; break;
    case 3: delta = 90; break;
    case 4: delta = -90; break;
    case 5: delta = -90; break;
    case 6: delta = -90; break;
    case 7: delta = -90; break;
    case 8: delta = -90; break;
    case 9: delta = 180; break;
    case 10: delta = 180; break;
    case 11: delta = 90; break;
    case 12: delta = 180; break;
    case 13: delta = 180; break;
  }
  i = (i + 1) % 14;
  aura::Desktop::GetInstance()->layer()->GetAnimator()->set_preemption_strategy(
      ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  scoped_ptr<ui::LayerAnimationSequence> screen_rotation(
      new ui::LayerAnimationSequence(new ui::ScreenRotation(delta)));
  screen_rotation->AddObserver(aura::Desktop::GetInstance());
  aura::Desktop::GetInstance()->layer()->GetAnimator()->ScheduleAnimation(
      screen_rotation.release());
}
#endif

}  // namespace

namespace aura_shell {

////////////////////////////////////////////////////////////////////////////////
// ShellAcceleratorController, public:

ShellAcceleratorController::ShellAcceleratorController()
    : accelerator_manager_(new ui::AcceleratorManager) {
  RegisterAccelerators(this);
}

ShellAcceleratorController::~ShellAcceleratorController() {
}

void ShellAcceleratorController::Register(
    const ui::Accelerator& accelerator,
    ui::AcceleratorTarget* target) {
  accelerator_manager_->Register(accelerator, target);
}

void ShellAcceleratorController::Unregister(
    const ui::Accelerator& accelerator,
    ui::AcceleratorTarget* target) {
  accelerator_manager_->Unregister(accelerator, target);
}

void ShellAcceleratorController::UnregisterAll(
    ui::AcceleratorTarget* target) {
  accelerator_manager_->UnregisterAll(target);
}

bool ShellAcceleratorController::Process(const ui::Accelerator& accelerator) {
  return accelerator_manager_->Process(accelerator);
}

////////////////////////////////////////////////////////////////////////////////
// ShellAcceleratorController, ui::AcceleratorTarget implementation:

bool ShellAcceleratorController::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
#if !defined(NDEBUG)
  if (accelerator.key_code() == ui::VKEY_F11) {
    aura::Desktop::GetInstance()->ToggleFullScreen();
    return true;
  } else if (accelerator.key_code() == ui::VKEY_HOME &&
             accelerator.IsCtrlDown()) {
    RotateScreen();
    return true;
  }
#endif
  return false;
}

}  // namespace aura_shell
