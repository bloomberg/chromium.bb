// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_CUSTOM_FRAME_VIEW_H_
#define UI_VIEWS_WINDOW_CUSTOM_FRAME_VIEW_H_
#pragma once

#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"
#include "views/controls/button/image_button.h"

namespace gfx {
class Canvas;
class Font;
class Size;
class Path;
class Point;
}

namespace views {

///////////////////////////////////////////////////////////////////////////////
//
// CustomFrameView
//
//  A ChromeView that provides the non client frame for Windows. This means
//  rendering the non-standard window caption, border, and controls.
//
////////////////////////////////////////////////////////////////////////////////
class CustomFrameView : public NonClientFrameView,
                        public ButtonListener {
 public:
  explicit CustomFrameView(Widget* frame);
  virtual ~CustomFrameView();

  // Overridden from NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask)
      OVERRIDE;
  virtual void ResetWindowControls() OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;

  // View overrides:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // ButtonListener implementation:
  virtual void ButtonPressed(Button* sender, const views::Event& event)
      OVERRIDE;

 private:
  // Returns the thickness of the border that makes up the window frame edges.
  // This does not include any client edge.
  int FrameBorderThickness() const;

  // Returns the thickness of the entire nonclient left, right, and bottom
  // borders, including both the window frame and any client edge.
  int NonClientBorderThickness() const;

  // Returns the height of the entire nonclient top border, including the window
  // frame, any title area, and any connected client edge.
  int NonClientTopBorderHeight() const;

  // Returns the y-coordinate of the caption buttons.
  int CaptionButtonY() const;

  // Returns the thickness of the nonclient portion of the 3D edge along the
  // bottom of the titlebar.
  int TitlebarBottomThickness() const;

  // Returns the size of the titlebar icon.  This is used even when the icon is
  // not shown, e.g. to set the titlebar height.
  int IconSize() const;

  // Returns the bounds of the titlebar icon (or where the icon would be if
  // there was one).
  gfx::Rect IconBounds() const;

  // Returns true if the client edge should be drawn. This is true if
  // the window delegate wants a client edge and we are not maxmized.
  bool ShouldShowClientEdge() const;

  // Paint various sub-components of this view.
  void PaintRestoredFrameBorder(gfx::Canvas* canvas);
  void PaintMaximizedFrameBorder(gfx::Canvas* canvas);
  void PaintTitleBar(gfx::Canvas* canvas);
  void PaintRestoredClientEdge(gfx::Canvas* canvas);

  // Layout various sub-components of this view.
  void LayoutWindowControls();
  void LayoutTitleBar();
  void LayoutClientView();

  // The bounds of the client view, in this view's coordinates.
  gfx::Rect client_view_bounds_;

  // The layout rect of the title, if visible.
  gfx::Rect title_bounds_;

  // Window controls.
  ImageButton* close_button_;
  ImageButton* restore_button_;
  ImageButton* maximize_button_;
  ImageButton* minimize_button_;
  ImageButton* window_icon_;
  bool should_show_minmax_buttons_;
  bool should_show_client_edge_;

  // The window that owns this view.
  Widget* frame_;

  // Initialize various static resources.
  static void InitClass();
  static gfx::Font* title_font_;

  DISALLOW_COPY_AND_ASSIGN(CustomFrameView);
};

}  // namespace views

#endif  // UI_VIEWS_WINDOW_CUSTOM_FRAME_VIEW_H_
