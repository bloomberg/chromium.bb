// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_TOOLS_CREATE_NOTE_ACTION_H_
#define ASH_SYSTEM_PALETTE_TOOLS_CREATE_NOTE_ACTION_H_

#include "ash/ash_export.h"
#include "ash/system/palette/common_palette_tool.h"
#include "base/macros.h"

namespace ash {

// A button in the ash palette that launches the selected note-taking app when
// clicked. This action dynamically hides itself if it is not available.
class ASH_EXPORT CreateNoteAction : public CommonPaletteTool {
 public:
  explicit CreateNoteAction(Delegate* delegate);
  ~CreateNoteAction() override;

 private:
  // PaletteTool overrides.
  PaletteGroup GetGroup() const override;
  PaletteToolId GetToolId() const override;
  void OnEnable() override;
  views::View* CreateView() override;

  // CommonPaletteTool overrides.
  const gfx::VectorIcon& GetPaletteIcon() const override;

  DISALLOW_COPY_AND_ASSIGN(CreateNoteAction);
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_TOOLS_CREATE_NOTE_ACTION_H_
