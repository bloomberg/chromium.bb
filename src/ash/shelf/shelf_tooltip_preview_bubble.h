// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_TOOLTIP_PREVIEW_BUBBLE_H_
#define ASH_SHELF_SHELF_TOOLTIP_PREVIEW_BUBBLE_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/shelf/shelf_tooltip_bubble_base.h"
#include "ash/shelf/shelf_tooltip_manager.h"
#include "ash/shelf/window_preview.h"
#include "ash/wm/window_mirror_view.h"
#include "ui/aura/window.h"
#include "ui/views/controls/label.h"

namespace ash {

// The implementation of tooltip bubbles for the shelf item.
class ASH_EXPORT ShelfTooltipPreviewBubble : public ShelfTooltipBubbleBase,
                                             public WindowPreview::Delegate {
 public:
  ShelfTooltipPreviewBubble(views::View* anchor,
                            views::BubbleBorder::Arrow arrow,
                            const std::vector<aura::Window*>& windows,
                            ShelfTooltipManager* manager);
  ~ShelfTooltipPreviewBubble() override;

 private:
  // Removes the given preview from the list of previewed windows.
  void RemovePreview(WindowPreview* preview);

  // BubbleDialogDelegateView overrides:
  gfx::Size CalculatePreferredSize() const override;

  // ShelfTooltipBubbleBase:
  bool ShouldCloseOnPressDown() override;
  bool ShouldCloseOnMouseExit() override;

  // WindowPreview::Delegate:
  void OnPreviewDismissed(WindowPreview* preview) override;
  void OnPreviewActivated(WindowPreview* preview) override;

  // views::View:
  void Layout() override;

  std::vector<WindowPreview*> previews_;

  // Computed dimensions for the tooltip.
  int width_ = 0;
  int height_ = 0;
  ShelfTooltipManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(ShelfTooltipPreviewBubble);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_TOOLTIP_PREVIEW_BUBBLE_H_
