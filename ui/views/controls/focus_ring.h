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
class VIEWS_EXPORT FocusRing : public View, public ViewObserver {
 public:
  static const char kViewClassName[];

  // Create a FocusRing and adds it to |parent|, or updates the one that already
  // exists with the given |color| and |corner_radius|.
  // TODO(crbug.com/831926): Prefer using the below Install() method - this one
  // should eventually be removed.
  static FocusRing* Install(
      View* parent,
      SkColor color,
      float corner_radius = FocusableBorder::kCornerRadiusDp);

  // Similar to FocusRing::Install(View, SkColor, float), but
  // |override_color_id| will be used in place of the default coloration
  // when provided.
  static FocusRing* Install(View* parent,
                            ui::NativeTheme::ColorId override_color_id =
                                ui::NativeTheme::kColorId_NumColors);

  // Removes the FocusRing from |parent|.
  static void Uninstall(View* parent);

  // Configure |view| for painting focus ring highlights.
  static void InitFocusRing(View* view);

  // View:
  const char* GetClassName() const override;
  void Layout() override;
  void OnPaint(gfx::Canvas* canvas) override;

  // ViewObserver:
  void OnViewNativeThemeChanged(View* observed_view) override;

 protected:
  FocusRing(SkColor color, float corner_radius);
  ~FocusRing() override;

 private:
  SkColor color_;
  float corner_radius_;
  base::Optional<ui::NativeTheme::ColorId> override_color_id_;
  ScopedObserver<View, FocusRing> view_observer_;

  DISALLOW_COPY_AND_ASSIGN(FocusRing);
};

}  // views

#endif  // UI_VIEWS_CONTROLS_FOCUS_RING_H_
