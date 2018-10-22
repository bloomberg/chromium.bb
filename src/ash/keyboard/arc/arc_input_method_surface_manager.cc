// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/arc/arc_input_method_surface_manager.h"

namespace ash {

exo::InputMethodSurface* ArcInputMethodSurfaceManager::GetSurface() const {
  return input_method_surface_;
}

void ArcInputMethodSurfaceManager::AddSurface(
    exo::InputMethodSurface* surface) {
  DCHECK_EQ(input_method_surface_, nullptr);
  input_method_surface_ = surface;
}

void ArcInputMethodSurfaceManager::RemoveSurface(
    exo::InputMethodSurface* surface) {
  DLOG_IF(ERROR, input_method_surface_ != surface)
      << "Can't remove not registered surface";

  if (input_method_surface_ == surface)
    input_method_surface_ = nullptr;
}

}  // namespace ash
