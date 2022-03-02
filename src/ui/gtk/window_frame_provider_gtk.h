// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_WINDOW_FRAME_PROVIDER_GTK_H_
#define UI_GTK_WINDOW_FRAME_PROVIDER_GTK_H_

#include "base/containers/flat_map.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/linux_ui/window_frame_provider.h"

namespace gtk {

class WindowFrameProviderGtk : public views::WindowFrameProvider {
 public:
  explicit WindowFrameProviderGtk(bool solid_frame);

  WindowFrameProviderGtk(const WindowFrameProviderGtk&) = delete;
  WindowFrameProviderGtk& operator=(const WindowFrameProviderGtk&) = delete;

  ~WindowFrameProviderGtk() override;

  // views::WindowFrameProvider:
  int GetTopCornerRadiusDip() override;
  gfx::Insets GetFrameThicknessDip() override;
  void PaintWindowFrame(gfx::Canvas* canvas,
                        const gfx::Rect& rect,
                        int top_area_height,
                        bool focused) override;

 private:
  // Data and metrics that depend on the scale.
  struct Asset {
    Asset();
    Asset(const Asset&);
    Asset& operator=(const Asset&);
    ~Asset();

    // Whether this record has valid data.
    bool valid = false;

    int frame_size_px = 0;
    gfx::Insets frame_thickness_px;

    // These are texture maps that we will sample from to draw the frame.  The
    // corners are drawn directly and the edges are tiled.
    SkBitmap focused_bitmap;
    SkBitmap unfocused_bitmap;

   private:
    void CloneFrom(const Asset&);
  };

  // Paint the window frame and update any metrics (like the frame thickness)
  // based on it.  Bitmaps and metrics are cached in |assets_|, so this is a
  // no-op if there is a cache entry created earlier.
  void MaybeUpdateBitmaps(float scale);

  int BitmapSizePx(const Asset& asset) const;

  // Input parameters used for drawing.
  const bool solid_frame_;
  std::string theme_name_;

  // Scale-independent metric calculated based on the bitmaps.
  gfx::Insets frame_thickness_dip_;
  int top_corner_radius_dip_ = 0;

  // Cached bitmaps and metrics.  The scale is rounded to percent.
  base::flat_map<int, Asset> assets_;
};

}  // namespace gtk

#endif  // UI_GTK_WINDOW_FRAME_PROVIDER_GTK_H_
