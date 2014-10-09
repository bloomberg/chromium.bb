// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_NATIVE_FRAME_VIEW_H_
#define UI_VIEWS_WINDOW_NATIVE_FRAME_VIEW_H_

#include "ui/views/window/non_client_view.h"

namespace views {

class Widget;

class VIEWS_EXPORT NativeFrameView : public NonClientFrameView {
 public:
  static const char kViewClassName[];

  explicit NativeFrameView(Widget* frame);
  virtual ~NativeFrameView();

  // NonClientFrameView overrides:
  virtual gfx::Rect GetBoundsForClientView() const override;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  virtual int NonClientHitTest(const gfx::Point& point) override;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) override;
  virtual void ResetWindowControls() override;
  virtual void UpdateWindowIcon() override;
  virtual void UpdateWindowTitle() override;
  virtual void SizeConstraintsChanged() override;

  // View overrides:
  virtual gfx::Size GetPreferredSize() const override;
  virtual gfx::Size GetMinimumSize() const override;
  virtual gfx::Size GetMaximumSize() const override;
  virtual const char* GetClassName() const override;

 private:
  // Our containing frame.
  Widget* frame_;

  DISALLOW_COPY_AND_ASSIGN(NativeFrameView);
};

}  // namespace views

#endif  // UI_VIEWS_WINDOW_NATIVE_FRAME_VIEW_H_
