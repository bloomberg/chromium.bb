// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_COMMON_PALETTE_TOOL_H_
#define ASH_SYSTEM_PALETTE_COMMON_PALETTE_TOOL_H_

#include "ash/system/palette/palette_tool.h"
#include "ash/system/tray/view_click_listener.h"
#include "base/strings/string16.h"
#include "base/time/time.h"

namespace gfx {
struct VectorIcon;
}

namespace ash {

class HoverHighlightView;

// A PaletteTool implementation with a standard view support.
class CommonPaletteTool : public PaletteTool, public ViewClickListener {
 protected:
  explicit CommonPaletteTool(Delegate* delegate);
  ~CommonPaletteTool() override;

  // PaletteTool:
  void OnViewDestroyed() override;
  void OnEnable() override;
  void OnDisable() override;

  // ViewClickListener:
  void OnViewClicked(views::View* sender) override;

  // Returns the icon used in the palette tray on the left-most edge of the
  // tool.
  virtual const gfx::VectorIcon& GetPaletteIcon() const = 0;

  // Creates a default view implementation to be returned by CreateView.
  views::View* CreateDefaultView(const base::string16& name);

  HoverHighlightView* highlight_view_ = nullptr;

 private:
  // start_time_ is initialized when the tool becomes active.
  // Used for recording UMA metrics.
  base::TimeTicks start_time_;

  DISALLOW_COPY_AND_ASSIGN(CommonPaletteTool);
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_COMMON_PALETTE_TOOL_H_
