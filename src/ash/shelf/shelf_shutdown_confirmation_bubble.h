// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_SHUTDOWN_CONFIRMATION_BUBBLE_H_
#define ASH_SHELF_SHELF_SHUTDOWN_CONFIRMATION_BUBBLE_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf_bubble.h"
#include "ash/style/default_colors.h"
#include "base/callback_forward.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace views {
class BubbleDialogDelegateView;
class View;
}  // namespace views

namespace ash {

// The implementation of tooltip bubbles for the shelf.
class ASH_EXPORT ShelfShutdownConfirmationBubble : public ShelfBubble {
 public:
  enum ButtonId {
    // We start from 1 because 0 is the default view ID.
    kShutdown = 1,  // Shut down the device.
    kCancel,        // Cancel shutdown.
  };

  ShelfShutdownConfirmationBubble(views::View* anchor,
                                  ShelfAlignment alignment,
                                  SkColor background_color,
                                  base::OnceClosure on_confirm_callback,
                                  base::OnceClosure on_cancel_callback);

  ShelfShutdownConfirmationBubble(const ShelfShutdownConfirmationBubble&) =
      delete;
  ShelfShutdownConfirmationBubble& operator=(
      const ShelfShutdownConfirmationBubble&) = delete;
  ~ShelfShutdownConfirmationBubble() override;

  // views::View:
  void OnThemeChanged() override;

 protected:
  // ShelfBubble:
  bool ShouldCloseOnPressDown() override;
  bool ShouldCloseOnMouseExit() override;

 private:
  // BubbleDialogDelegateView overrides:
  gfx::Size CalculatePreferredSize() const override;

  // Callback functions of cancel and confirm buttons
  void OnCancelled();
  void OnConfirmed();
  base::OnceClosure confirm_callback_;
  base::OnceClosure cancel_callback_;

  views::ImageView* icon_ = nullptr;
  views::Label* title_ = nullptr;
  views::LabelButton* cancel_ = nullptr;
  views::LabelButton* confirm_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_SHUTDOWN_CONFIRMATION_BUBBLE_H_