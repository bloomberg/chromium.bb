// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_TOOLS_LASER_POINTER_MODE_H_
#define ASH_SYSTEM_PALETTE_TOOLS_LASER_POINTER_MODE_H_

#include "ash/ash_export.h"
#include "ash/system/palette/common_palette_tool.h"

namespace ash {

// Controller for the laser pointer functionality.
class ASH_EXPORT LaserPointerMode : public CommonPaletteTool {
 public:
  explicit LaserPointerMode(Delegate* delegate);
  ~LaserPointerMode() override;

 private:
  // PaletteTool:
  PaletteGroup GetGroup() const override;
  PaletteToolId GetToolId() const override;
  void OnEnable() override;
  void OnDisable() override;
  const gfx::VectorIcon& GetActiveTrayIcon() const override;
  views::View* CreateView() override;

  // CommonPaletteTool:
  const gfx::VectorIcon& GetPaletteIcon() const override;

  DISALLOW_COPY_AND_ASSIGN(LaserPointerMode);
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_TOOLS_LASER_POINTER_MODE_H_
