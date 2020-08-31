// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHORTCUT_VIEWER_VIEWS_BUBBLE_VIEW_H_
#define ASH_SHORTCUT_VIEWER_VIEWS_BUBBLE_VIEW_H_

#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/views/view.h"

namespace gfx {
class ShadowValue;
struct VectorIcon;
}  // namespace gfx

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace keyboard_shortcut_viewer {

// Displays a keyboard shortcut component (such as a modifier or a key) as a
// rounded-corner bubble which can contain either text, icon, or both.
class BubbleView : public views::View {
 public:
  BubbleView();
  ~BubbleView() override;

  void SetIcon(const gfx::VectorIcon& icon);

  void SetText(const base::string16& text);

 private:
  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;

  views::ImageView* icon_ = nullptr;

  views::Label* text_ = nullptr;

  std::vector<gfx::ShadowValue> shadows_;

  DISALLOW_COPY_AND_ASSIGN(BubbleView);
};

}  // namespace keyboard_shortcut_viewer

#endif  // ASH_SHORTCUT_VIEWER_VIEWS_BUBBLE_VIEW_H_
