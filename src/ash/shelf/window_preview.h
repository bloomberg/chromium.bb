// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_WINDOW_PREVIEW_H_
#define ASH_SHELF_WINDOW_PREVIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/window_mirror_view.h"
#include "base/observer_list.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace aura {
class Window;
}

namespace views {
class ImageButton;
class Label;
}  // namespace views

namespace ash {

class ASH_EXPORT WindowPreview : public views::View,
                                 public views::ButtonListener {
 public:
  class Delegate {
   public:
    // Notify the delegate that the preview has closed.
    virtual void OnPreviewDismissed(WindowPreview* preview) = 0;

    // Notify the delegate that the preview has been activated.
    virtual void OnPreviewActivated(WindowPreview* preview) = 0;

   protected:
    virtual ~Delegate() {}
  };

  WindowPreview(aura::Window* window,
                Delegate* delegate,
                const ui::NativeTheme* theme);
  ~WindowPreview() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;

  // ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  void SetStyling(const ui::NativeTheme* theme);

  // Child views.
  views::ImageButton* close_button_;
  views::Label* title_;
  wm::WindowMirrorView* mirror_;

  // Unowned pointer to the delegate. The delegate should outlive this instance.
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(WindowPreview);
};

}  // namespace ash

#endif  // ASH_SHELF_WINDOW_PREVIEW_H_
