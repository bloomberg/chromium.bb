// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SCREEN_AURA_H_
#define UI_AURA_SCREEN_AURA_H_
#pragma once

#include "base/compiler_specific.h"
#include "ui/gfx/screen.h"

namespace aura {
namespace internal {

// Aura implementation of gfx::Screen. Implemented here to avoid circular
// dependencies.
class ScreenAura : public gfx::Screen {
 public:
  ScreenAura();
  virtual ~ScreenAura();

 protected:
  virtual gfx::Point GetCursorScreenPointImpl() OVERRIDE;
  virtual gfx::Rect GetMonitorWorkAreaNearestWindowImpl(
      gfx::NativeView view) OVERRIDE;
  virtual gfx::Rect GetMonitorAreaNearestWindowImpl(
      gfx::NativeView view) OVERRIDE;
  virtual gfx::Rect GetMonitorWorkAreaNearestPointImpl(
      const gfx::Point& point) OVERRIDE;
  virtual gfx::Rect GetMonitorAreaNearestPointImpl(
      const gfx::Point& point) OVERRIDE;
  virtual gfx::NativeWindow GetWindowAtCursorScreenPointImpl() OVERRIDE;

private:
  DISALLOW_COPY_AND_ASSIGN(ScreenAura);
};

}  // namespace internal
}  // namespace aura

#endif  // UI_AURA_SCREEN_AURA_H_
