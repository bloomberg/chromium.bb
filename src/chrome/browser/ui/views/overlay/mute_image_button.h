// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_MUTE_IMAGE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_MUTE_IMAGE_BUTTON_H_

#include "chrome/browser/ui/views/overlay/overlay_window_views.h"
#include "ui/views/controls/button/image_button.h"

namespace views {

// An image button representing a white mute button. When the user interacts
// with the button, a grey circular background appears as an indicator.
class MuteImageButton : public views::ImageButton {
 public:
  explicit MuteImageButton(ButtonListener*);
  ~MuteImageButton() override = default;

  // views::Button:
  void StateChanged(ButtonState old_state) override;

  // views::View:
  void OnFocus() override;
  void OnBlur() override;

  // Sets the position of itself with an offset from the given window size.
  void SetPosition(const gfx::Size&, OverlayWindowViews::WindowQuadrant);

  // Show mute/unmute image and update tooltip based on video muted status.
  void SetMutedState(OverlayWindowViews::MutedState);

  // Toggle visibility.
  void ToggleVisibility(bool is_visible);

 private:
  const gfx::ImageSkia mute_background_;
  const gfx::ImageSkia mute_image_;
  const gfx::ImageSkia unmute_image_;

  OverlayWindowViews::MutedState muted_state_;

  DISALLOW_COPY_AND_ASSIGN(MuteImageButton);
};

}  // namespace views

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_MUTE_IMAGE_BUTTON_H_
