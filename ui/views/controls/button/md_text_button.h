// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_H_

#include "base/memory/scoped_ptr.h"
#include "ui/views/controls/button/custom_button.h"

namespace gfx {
class RenderText;
}

namespace views {

// A button class that implements the Material Design text button spec.
class VIEWS_EXPORT MdTextButton : public CustomButton {
 public:
  MdTextButton(ButtonListener* listener, const base::string16& text);
  ~MdTextButton() override;

  // View:
  void OnPaint(gfx::Canvas* canvas) override;
  gfx::Size GetPreferredSize() const override;

 private:
  void UpdateColor();

  scoped_ptr<gfx::RenderText> render_text_;

  DISALLOW_COPY_AND_ASSIGN(MdTextButton);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_H_
