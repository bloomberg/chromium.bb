// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_FOCUS_RING_H_
#define UI_VIEWS_CONTROLS_FOCUS_RING_H_

#include "base/scoped_observer.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/views_export.h"

namespace views {

// FocusRing is a View that is designed to act as an indicator of focus for its
// parent. It is a stand-alone view that paints to a layer which extends beyond
// the bounds of its parent view.
class VIEWS_EXPORT FocusRing : public View {
 public:
  static const char kViewClassName[];

  // Create a FocusRing and adds it to |parent|, or updates the one that already
  // exists with the given |color| and |corner_radius|. If not provided, the
  // focus ring color defaults to ui::NativeTheme::kColorId_FocusedBorderColor.
  // TODO(crbug.com/831926): Investigate switching to a ui::NativeTheme::ColorId
  // instead of a raw SkColor.
  static FocusRing* Install(
      View* parent,
      SkColor override_color = kInvalidColor,
      float corner_radius = FocusableBorder::kCornerRadiusDp);

  // Removes the FocusRing from |parent|.
  static void Uninstall(View* parent);

  // Configure |view| for painting focus ring highlights.
  static void InitFocusRing(View* view);

  // View:
  const char* GetClassName() const override;
  void Layout() override;
  void OnPaint(gfx::Canvas* canvas) override;

 protected:
  FocusRing(View* parent, SkColor color, float corner_radius);
  ~FocusRing() override;

 private:
  SkColor GetColor() const;

  static constexpr SkColor kInvalidColor = SK_ColorTRANSPARENT;

  // The View this focus ring is installed on.
  View* view_;

  // The focus ring color.
  SkColor color_;

  // The corner radius of the focus ring. This will be slightly enlarged by half
  // the thickness of the ring to make sure it hugs the border of |view_|.
  float corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(FocusRing);
};

}  // views

#endif  // UI_VIEWS_CONTROLS_FOCUS_RING_H_
