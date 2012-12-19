// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_DIALOG_FRAME_VIEW_H_
#define UI_VIEWS_WINDOW_DIALOG_FRAME_VIEW_H_

#include "ui/views/controls/button/button.h"
#include "ui/views/window/non_client_view.h"

namespace gfx {
class Font;
}

namespace views {

class ImageButton;

// A NonClientFrameView that implements a new-style for dialogs.
class VIEWS_EXPORT DialogFrameView : public NonClientFrameView,
                                     public ButtonListener {
 public:
  // Internal class name.
  static const char kViewClassName[];

  DialogFrameView();
  virtual ~DialogFrameView();

  // Overridden from NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) OVERRIDE;
  virtual void ResetWindowControls() OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;
  virtual void UpdateWindowTitle() OVERRIDE;

  // Overridden from View:
  virtual std::string GetClassName() const OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // Overridden from ButtonListener:
  virtual void ButtonPressed(Button* sender, const ui::Event& event) OVERRIDE;

 private:
  gfx::Insets GetPaddingInsets() const;
  gfx::Insets GetClientInsets() const;

  scoped_ptr<gfx::Font> title_font_;
  gfx::Rect title_display_rect_;

  ImageButton* close_button_;

  DISALLOW_COPY_AND_ASSIGN(DialogFrameView);
};

}  // namespace views

#endif  // UI_VIEWS_WINDOW_DIALOG_FRAME_VIEW_H_
