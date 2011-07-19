// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/native_theme_painter.h"

#include "base/logging.h"
#include "ui/base/animation/animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/rect.h"
#include "views/native_theme_delegate.h"

namespace views {

NativeThemePainter::NativeThemePainter(NativeThemeDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

gfx::Size NativeThemePainter::GetPreferredSize() {
  const gfx::NativeTheme* theme = gfx::NativeTheme::instance();
  gfx::NativeTheme::ExtraParams extra;
  gfx::NativeTheme::State state = delegate_->GetThemeState(&extra);
  return theme->GetPartSize(delegate_->GetThemePart(), state, extra);
}

void NativeThemePainter::Paint(int w, int h, gfx::Canvas* canvas) {
  const gfx::NativeTheme* native_theme = gfx::NativeTheme::instance();
  gfx::NativeTheme::Part part = delegate_->GetThemePart();
  gfx::CanvasSkia* skia_canvas = canvas->AsCanvasSkia();
  gfx::Rect rect(0, 0, w, h);

  if (delegate_->GetThemeAnimation() != NULL &&
      delegate_->GetThemeAnimation()->is_animating()) {
    // Paint background state.
    gfx::NativeTheme::ExtraParams prev_extra;
    gfx::NativeTheme::State prev_state =
        delegate_->GetBackgroundThemeState(&prev_extra);
    native_theme->Paint(skia_canvas, part, prev_state, rect, prev_extra);

    // Composite foreground state above it.
    gfx::NativeTheme::ExtraParams extra;
    gfx::NativeTheme::State state = delegate_->GetForegroundThemeState(&extra);
    int alpha = delegate_->GetThemeAnimation()->CurrentValueBetween(0, 255);
    skia_canvas->SaveLayerAlpha(static_cast<uint8>(alpha));
    native_theme->Paint(skia_canvas, part, state, rect, extra);
    skia_canvas->Restore();
  } else {
    gfx::NativeTheme::ExtraParams extra;
    gfx::NativeTheme::State state = delegate_->GetThemeState(&extra);
    native_theme->Paint(skia_canvas, part, state, rect, extra);
  }
}

}  // namespace views
