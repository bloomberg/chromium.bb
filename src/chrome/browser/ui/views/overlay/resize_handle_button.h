// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_RESIZE_HANDLE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_RESIZE_HANDLE_BUTTON_H_

#include "chrome/browser/ui/views/overlay/overlay_window_views.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"

// An image button representing a white resize handle affordance.
class ResizeHandleButton : public views::ImageButton {
 public:
  METADATA_HEADER(ResizeHandleButton);

  explicit ResizeHandleButton(PressedCallback callback);
  ResizeHandleButton(const ResizeHandleButton&) = delete;
  ResizeHandleButton& operator=(const ResizeHandleButton&) = delete;
  ~ResizeHandleButton() override;

  void SetPosition(const gfx::Size& size,
                   OverlayWindowViews::WindowQuadrant quadrant);
  int GetHTComponent() const;

 private:
  void SetImageForQuadrant(OverlayWindowViews::WindowQuadrant quadrant);

  absl::optional<OverlayWindowViews::WindowQuadrant> current_quadrant_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_RESIZE_HANDLE_BUTTON_H_
