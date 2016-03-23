// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/mac/combobox_background_mac.h"

#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/native_theme_mac.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/view.h"

namespace views {

ComboboxBackgroundMac::ComboboxBackgroundMac() {}

ComboboxBackgroundMac::~ComboboxBackgroundMac() {}

void ComboboxBackgroundMac::Paint(gfx::Canvas* canvas, View* view) const {
  DCHECK_EQ(view->GetClassName(), Combobox::kViewClassName);
  Combobox* combobox = static_cast<Combobox*>(view);

  gfx::RectF bounds(combobox->GetLocalBounds());
  // Inset the left side far enough to draw only the arrow button, and inset the
  // other three sides by half a pixel so the edge of the background doesn't
  // paint outside the border.
  bounds.Inset(bounds.width() - combobox->GetArrowButtonWidth(), 0.5, 0.5, 0.5);

  ui::NativeTheme::State state = ui::NativeTheme::kNormal;
  if (!combobox->enabled())
    state = ui::NativeTheme::kDisabled;

  skia::RefPtr<SkShader> shader =
      ui::NativeThemeMac::GetButtonBackgroundShader(
          state,
          bounds.height());
  SkPaint paint;
  paint.setShader(shader.get());
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAntiAlias(true);

  SkPoint no_curve = SkPoint::Make(0, 0);
  SkPoint curve = SkPoint::Make(
      ui::NativeThemeMac::kComboboxCornerRadius,
      ui::NativeThemeMac::kComboboxCornerRadius);
  SkVector curves[4] = { no_curve, curve, curve, no_curve };

  SkRRect fill_rect;
  fill_rect.setRectRadii(gfx::RectFToSkRect(bounds), curves);

  SkPath path;
  path.addRRect(fill_rect);

  canvas->DrawPath(path, paint);
}

}  // namespace views
