// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_TOPLEVEL_FRAME_VIEW_H_
#define UI_AURA_SHELL_TOPLEVEL_FRAME_VIEW_H_
#pragma once

#include "ui/aura_shell/aura_shell_export.h"
#include "ui/views/window/non_client_view.h"
#include "views/controls/button/button.h"

namespace aura_shell {
namespace internal {

class FrameComponent;
class SizingBorder;
class WindowCaption;

// A NonClientFrameView implementation for generic top-level windows in Aura.
// TODO(beng): Find a way to automatically this for all top-level windows in
//             Aura. Right now windows have to override CreateNonClientFrameView
//             on WidgetDelegate to specify this.
class AURA_SHELL_EXPORT ToplevelFrameView : public views::NonClientFrameView {
 public:
  ToplevelFrameView();
  virtual ~ToplevelFrameView();

 private:
  // Returns the height of the side/bottom non-client edges.
  int NonClientBorderThickness() const;

  // Returns the height of the top non-client edge - the caption.
  int NonClientTopBorderHeight() const;

  // Implementation of NonClientHitTest().
  int NonClientHitTestImpl(const gfx::Point& point);

  // Shows the specified |sizing_border|, hiding all others.
  // If |sizing_border| is NULL, all other sizing borders are hidden.
  void ShowFrameComponent(FrameComponent* sizing_border);

  // Returns true if the specified point (in FrameView coordinates) hit-tests
  // against the specified child.
  bool PointIsInChildView(views::View* child, const gfx::Point& point) const;

  // Returns the bounds of the specified sizing border for its visible and
  // hidden states.
  gfx::Rect GetHiddenBoundsForSizingBorder(int frame_component) const;
  gfx::Rect GetVisibleBoundsForSizingBorder(int frame_component) const;

  // Overridden from views::NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) OVERRIDE;
  virtual void ResetWindowControls() OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;

  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual void OnMouseMoved(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;
  virtual views::View* GetEventHandlerForPoint(
      const gfx::Point& point) OVERRIDE;
  virtual gfx::NativeCursor GetCursor(const views::MouseEvent& event) OVERRIDE;

  gfx::Rect client_view_bounds_;

  int current_hittest_code_;

  WindowCaption* caption_;
  SizingBorder* left_edge_;
  SizingBorder* right_edge_;
  SizingBorder* bottom_edge_;

  DISALLOW_COPY_AND_ASSIGN(ToplevelFrameView);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // #ifndef UI_AURA_SHELL_TOPLEVEL_FRAME_VIEW_H_
