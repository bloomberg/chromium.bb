// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_BASE_VIEW_H_
#define ASH_WALLPAPER_WALLPAPER_BASE_VIEW_H_

#include "base/macros.h"
#include "ui/views/view.h"

namespace ash {

// A view that paints the wallpaper according to its layout inside its bounds.
// This view doesn't add any dimming or blur to the painted wallpaper. Sub
// classes may override DrawWallpaper() to achieve these effects.
// This can be used directly (e.g. by DeskPreviewView) to paint just the
// wallpaper without any extra effects.
class WallpaperBaseView : public views::View {
 public:
  WallpaperBaseView() = default;
  ~WallpaperBaseView() override = default;

  // views::View:
  const char* GetClassName() const override;
  void OnPaint(gfx::Canvas* canvas) override;

 protected:
  virtual void DrawWallpaper(const gfx::ImageSkia& wallpaper,
                             const gfx::Rect& src,
                             const gfx::Rect& dst,
                             const cc::PaintFlags& flags,
                             gfx::Canvas* canvas);

 private:
  DISALLOW_COPY_AND_ASSIGN(WallpaperBaseView);
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_BASE_VIEW_H_
