// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/slider.h"

#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

namespace {

void ClickAt(views::View* view, int x, int y) {
  views::MouseEvent press(ui::ET_MOUSE_PRESSED, x, y, ui::EF_LEFT_MOUSE_BUTTON);
  view->OnMousePressed(press);
  views::MouseEvent release(ui::ET_MOUSE_RELEASED, x, y,
      ui::EF_LEFT_MOUSE_BUTTON);
  view->OnMouseReleased(release);
}

}

namespace views {

TEST(SliderTest, UpdateFromClick) {
  scoped_ptr<Slider> slider(new Slider(NULL, Slider::HORIZONTAL));
  View* view = slider.get();
  gfx::Size size = view->GetPreferredSize();
  view->SetSize(size);

  ClickAt(view, 0, 0);
  EXPECT_EQ(0.0f, slider->value());

  ClickAt(view, view->width(), 0);
  EXPECT_EQ(1.0f, slider->value());
}

TEST(SliderTest, UpdateFromClickRTL) {
  std::string locale = l10n_util::GetApplicationLocale("");
  base::i18n::SetICUDefaultLocale("he");

  scoped_ptr<Slider> slider(new Slider(NULL, Slider::HORIZONTAL));
  View* view = slider.get();
  gfx::Size size = view->GetPreferredSize();
  view->SetSize(size);

  ClickAt(view, 0, 0);
  EXPECT_EQ(1.0f, slider->value());

  ClickAt(view, view->width(), 0);
  EXPECT_EQ(0.0f, slider->value());

  // Reset locale.
  base::i18n::SetICUDefaultLocale(locale);
}

}  // namespace views
