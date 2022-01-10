// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_LINUX_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/browser_frame_view_layout_linux.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"
#include "ui/views/linux_ui/window_button_order_observer.h"

// A specialization of OpaqueBrowserFrameView that is also able to
// render client side decorations (shadow, border, and rounded corners).
class BrowserFrameViewLinux : public OpaqueBrowserFrameView,
                              public views::WindowButtonOrderObserver {
 public:
  BrowserFrameViewLinux(BrowserFrame* frame,
                        BrowserView* browser_view,
                        BrowserFrameViewLayoutLinux* layout);

  BrowserFrameViewLinux(const BrowserFrameViewLinux&) = delete;
  BrowserFrameViewLinux& operator=(const BrowserFrameViewLinux&) = delete;

  ~BrowserFrameViewLinux() override;

  BrowserFrameViewLayoutLinux* layout() { return layout_; }

  // Gets the rounded-rect that will be used to clip the window frame when
  // drawing.  The region will be as if the window was restored, and will be in
  // view coordinates.
  SkRRect GetRestoredClipRegion() const;

  // Gets the shadow metrics (radius, offset, and number of shadows).  This will
  // always return shadow values, even if shadows are not actually drawn.
  static gfx::ShadowValues GetShadowValues();

 protected:
  // views::WindowButtonOrderObserver:
  void OnWindowButtonOrderingChange(
      const std::vector<views::FrameButton>& leading_buttons,
      const std::vector<views::FrameButton>& trailing_buttons) override;

  // views::View:
  void PaintRestoredFrameBorder(gfx::Canvas* canvas) const override;

  // views::NonClientFrameView:
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;

  // OpaqueBrowserFrameViewLayoutDelegate:
  bool ShouldDrawRestoredFrameShadow() const override;

  // Gets the radius of the top corners when the window is restored.  The
  // returned value is in DIPs.  The result will be 0 if rounded corners are
  // disabled (eg. if the compositor doesn't support translucency.)
  virtual float GetRestoredCornerRadiusDip() const;

 private:
  const raw_ptr<BrowserFrameViewLayoutLinux> layout_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_LINUX_H_
