// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_ARC_ARC_INPUT_METHOD_SURFACE_MANAGER_H_
#define ASH_KEYBOARD_ARC_ARC_INPUT_METHOD_SURFACE_MANAGER_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/exo/input_method_surface_manager.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

class ArcInputMethodSurfaceManager : public exo::InputMethodSurfaceManager {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnArcInputMethodSurfaceBoundsChanged(
        const gfx::Rect& bounds) = 0;
  };

  ArcInputMethodSurfaceManager();
  ~ArcInputMethodSurfaceManager() override;

  // exo::InputMethodSurfaceManager:
  exo::InputMethodSurface* GetSurface() const override;
  void AddSurface(exo::InputMethodSurface* surface) override;
  void RemoveSurface(exo::InputMethodSurface* surface) override;
  void OnTouchableBoundsChanged(exo::InputMethodSurface* surface) override;

  // Management of the observer list.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  exo::InputMethodSurface* input_method_surface_ = nullptr;  // Not owned

  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(ArcInputMethodSurfaceManager);
};

}  // namespace ash

#endif  // ASH_KEYBOARD_ARC_ARC_INPUT_METHOD_SURFACE_MANAGER_H_
