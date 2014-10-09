// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_RESIZE_AREA_H_
#define UI_VIEWS_CONTROLS_RESIZE_AREA_H_

#include <string>

#include "ui/views/view.h"

namespace views {

class ResizeAreaDelegate;

////////////////////////////////////////////////////////////////////////////////
//
// An invisible area that acts like a horizontal resizer.
//
////////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT ResizeArea : public View {
 public:
  static const char kViewClassName[];

  explicit ResizeArea(ResizeAreaDelegate* delegate);
  virtual ~ResizeArea();

  // Overridden from views::View:
  virtual const char* GetClassName() const override;
  virtual gfx::NativeCursor GetCursor(const ui::MouseEvent& event) override;
  virtual bool OnMousePressed(const ui::MouseEvent& event) override;
  virtual bool OnMouseDragged(const ui::MouseEvent& event) override;
  virtual void OnMouseReleased(const ui::MouseEvent& event) override;
  virtual void OnMouseCaptureLost() override;
  virtual void GetAccessibleState(ui::AXViewState* state) override;

 private:
  // Report the amount the user resized by to the delegate, accounting for
  // directionality.
  void ReportResizeAmount(int resize_amount, bool last_update);

  // The delegate to notify when we have updates.
  ResizeAreaDelegate* delegate_;

  // The mouse position at start (in screen coordinates).
  int initial_position_;

  DISALLOW_COPY_AND_ASSIGN(ResizeArea);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_RESIZE_AREA_H_
