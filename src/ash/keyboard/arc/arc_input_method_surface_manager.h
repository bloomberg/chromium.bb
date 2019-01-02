// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_ARC_ARC_INPUT_METHOD_SURFACE_MANAGER_H_
#define ASH_KEYBOARD_ARC_ARC_INPUT_METHOD_SURFACE_MANAGER_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/exo/input_method_surface_manager.h"

namespace ash {

class ArcInputMethodSurfaceManager : public exo::InputMethodSurfaceManager {
 public:
  ArcInputMethodSurfaceManager() = default;
  ~ArcInputMethodSurfaceManager() override = default;

  // TODO(yhanada): Observer surface addition and removal from
  // ArcInputMethodManagerService.

  // exo::InputMethodSurfaceManager:
  exo::InputMethodSurface* GetSurface() const override;
  void AddSurface(exo::InputMethodSurface* surface) override;
  void RemoveSurface(exo::InputMethodSurface* surface) override;

 private:
  exo::InputMethodSurface* input_method_surface_ = nullptr;  // Not owned

  DISALLOW_COPY_AND_ASSIGN(ArcInputMethodSurfaceManager);
};

}  // namespace ash

#endif  // ASH_KEYBOARD_ARC_ARC_INPUT_METHOD_SURFACE_MANAGER_H_
