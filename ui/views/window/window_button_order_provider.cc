// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/window_button_order_provider.h"

namespace views {

// static
WindowButtonOrderProvider* WindowButtonOrderProvider::instance_ = NULL;

///////////////////////////////////////////////////////////////////////////////
// WindowButtonOrderProvider, public:

// static
WindowButtonOrderProvider* WindowButtonOrderProvider::GetInstance() {
  if (!instance_)
    instance_ = new WindowButtonOrderProvider;
  return instance_;
}

std::vector<views::FrameButton> const
    WindowButtonOrderProvider::GetLeadingButtons() const {
  return leading_buttons_;
}

std::vector<views::FrameButton> const
    WindowButtonOrderProvider::GetTrailingButtons() const {
  return trailing_buttons_;
}

///////////////////////////////////////////////////////////////////////////////
// WindowButtonOrderProvider, protected:

WindowButtonOrderProvider::WindowButtonOrderProvider() {
  trailing_buttons_.push_back(views::FRAME_BUTTON_MINIMIZE);
  trailing_buttons_.push_back(views::FRAME_BUTTON_MAXIMIZE);
  trailing_buttons_.push_back(views::FRAME_BUTTON_CLOSE);
}

WindowButtonOrderProvider::~WindowButtonOrderProvider() {
}

void WindowButtonOrderProvider::SetWindowButtonOrder(
    const std::vector<views::FrameButton>& leading_buttons,
    const std::vector<views::FrameButton>& trailing_buttons) {
  leading_buttons_ = leading_buttons;
  trailing_buttons_ = trailing_buttons;
}

}  // namespace views
