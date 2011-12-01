// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/shell_accelerator_controller.h"

#include "base/logging.h"
#include "ui/aura/desktop.h"
#include "ui/aura/event.h"
#include "ui/aura_shell/shell.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/gfx/compositor/debug_utils.h"
#include "ui/gfx/compositor/layer_animation_sequence.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "ui/gfx/compositor/screen_rotation.h"

namespace {

enum AcceleratorAction {
  CYCLE_BACKWARD,
  CYCLE_FORWARD,
  TAKE_SCREENSHOT,
#if !defined(NDEBUG)
  ROTATE_SCREEN,
  PRINT_LAYER_HIERARCHY,
  TOGGLE_DESKTOP_FULL_SCREEN,
#endif
};

// Acceleraters handled by ShellAcceleratorController.
struct AcceleratorData {
  ui::KeyboardCode keycode;
  bool shift;
  bool ctrl;
  bool alt;
  AcceleratorAction action;
} kAcceleratorData[] = {
  { ui::VKEY_TAB, true, false, true, CYCLE_BACKWARD },
  { ui::VKEY_TAB, false, false, true, CYCLE_FORWARD },
  { ui::VKEY_F5, false, true, false, TAKE_SCREENSHOT },
  { ui::VKEY_PRINT, false, false, false, TAKE_SCREENSHOT },
#if !defined(NDEBUG)
  { ui::VKEY_HOME, false, true, false, ROTATE_SCREEN },
  { ui::VKEY_F11, false, true, false, TOGGLE_DESKTOP_FULL_SCREEN },
  { ui::VKEY_L, false, false, true, PRINT_LAYER_HIERARCHY },
#endif
};

bool HandleCycleBackward() {
  // TODO(mazda): http://crbug.com/105204
  NOTIMPLEMENTED();
  return false;
}

bool HandleCycleForward() {
  // TODO(mazda): http://crbug.com/105204
  NOTIMPLEMENTED();
  return false;
}

bool HandleTakeScreenshot() {
  // TODO(mazda): http://crbug.com/105198
  NOTIMPLEMENTED();
  return false;
}

#if !defined(NDEBUG)
// Rotates the screen.
bool HandleRotateScreen() {
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
  aura::Desktop::GetInstance()->layer()->GetAnimator()->StartAnimation(
      screen_rotation.release());
  return true;
}

bool HandleToggleDesktopFullScreen() {
  aura::Desktop::GetInstance()->ToggleFullScreen();
  return true;
}

bool HandlePrintLayerHierarchy() {
  ui::PrintLayerHierarchy(aura::Desktop::GetInstance()->layer());
  return true;
}
#endif

}  // namespace

namespace aura_shell {

////////////////////////////////////////////////////////////////////////////////
// ShellAcceleratorController, public:

ShellAcceleratorController::ShellAcceleratorController()
    : accelerator_manager_(new ui::AcceleratorManager) {
  Init();
}

ShellAcceleratorController::~ShellAcceleratorController() {
}

void ShellAcceleratorController::Init() {
  for (size_t i = 0; i < arraysize(kAcceleratorData); ++i) {
    ui::Accelerator accelerator(kAcceleratorData[i].keycode,
                                kAcceleratorData[i].shift,
                                kAcceleratorData[i].ctrl,
                                kAcceleratorData[i].alt);
    Register(accelerator, this);
    accelerators_.insert(std::make_pair(accelerator,
                                        kAcceleratorData[i].action));
  }
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
  std::map<ui::Accelerator, int>::const_iterator it =
      accelerators_.find(accelerator);
  DCHECK(it != accelerators_.end());
  switch (static_cast<AcceleratorAction>(it->second)) {
    case CYCLE_BACKWARD:
      return HandleCycleBackward();
    case CYCLE_FORWARD:
      return HandleCycleForward();
    case TAKE_SCREENSHOT:
      return HandleTakeScreenshot();
#if !defined(NDEBUG)
    case ROTATE_SCREEN:
      return HandleRotateScreen();
    case TOGGLE_DESKTOP_FULL_SCREEN:
      return HandleToggleDesktopFullScreen();
    case PRINT_LAYER_HIERARCHY:
      return HandlePrintLayerHierarchy();
#endif
    default:
      NOTREACHED() << "Unhandled action " << it->second;;
  }
  return false;
}

}  // namespace aura_shell
