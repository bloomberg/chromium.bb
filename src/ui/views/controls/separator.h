// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SEPARATOR_H_
#define UI_VIEWS_CONTROLS_SEPARATOR_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace views {

// The Separator class is a view that shows a line used to visually separate
// other views.
class VIEWS_EXPORT Separator : public View {
 public:
  METADATA_HEADER(Separator);

  // The separator's thickness in dip.
  static constexpr int kThickness = 1;

  Separator();

  Separator(const Separator&) = delete;
  Separator& operator=(const Separator&) = delete;

  ~Separator() override;

  SkColor GetColor() const;
  void SetColor(SkColor color);

  int GetPreferredHeight() const;
  void SetPreferredHeight(int height);

  // Overridden from View:
  gfx::Size CalculatePreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  int preferred_height_ = kThickness;
  absl::optional<SkColor> overridden_color_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, Separator, View)
VIEW_BUILDER_PROPERTY(SkColor, Color)
VIEW_BUILDER_PROPERTY(int, PreferredHeight)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, Separator)

#endif  // UI_VIEWS_CONTROLS_SEPARATOR_H_
