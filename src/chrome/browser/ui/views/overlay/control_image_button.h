// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_CONTROL_IMAGE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_CONTROL_IMAGE_BUTTON_H_

#include "ui/views/controls/button/image_button.h"

namespace views {

// An image button with an associated id.
class ControlImageButton : public views::ImageButton {
 public:
  explicit ControlImageButton(ButtonListener* listener);

  ~ControlImageButton() override;

  void set_id(const std::string& id) { id_ = id; }

  const std::string& id() const { return id_; }

 private:
  // The id that is passed back to the browser when interacting with it fires an
  // event.
  std::string id_;

  DISALLOW_COPY_AND_ASSIGN(ControlImageButton);
};

}  // namespace views

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_CONTROL_IMAGE_BUTTON_H_
