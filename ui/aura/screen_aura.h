// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SCREEN_AURA_H_
#define UI_AURA_SCREEN_AURA_H_
#pragma once

#include "base/compiler_specific.h"
#include "ui/aura/aura_export.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/screen.h"

namespace aura {

// Aura implementation of gfx::Screen. Implemented here to avoid circular
// dependencies.
class AURA_EXPORT ScreenAura : public gfx::Screen {
 public:
  ScreenAura();
  virtual ~ScreenAura();

  void set_work_area_insets(const gfx::Insets& insets) {
    work_area_insets_ = insets;
  }
  const gfx::Insets& work_area_insets() const { return work_area_insets_; }

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
  virtual gfx::Size GetPrimaryMonitorSizeImpl() OVERRIDE;
  virtual int GetNumMonitorsImpl() OVERRIDE;

 private:
  // We currently support only one monitor. These two methods return the bounds
  // and work area.
  gfx::Rect GetBounds();
  gfx::Rect GetWorkAreaBounds();

  // Insets for the work area.
  gfx::Insets work_area_insets_;

  DISALLOW_COPY_AND_ASSIGN(ScreenAura);
};

}  // namespace aura

#endif  // UI_AURA_SCREEN_AURA_H_
