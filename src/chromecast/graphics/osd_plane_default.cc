// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/public/osd_plane.h"
#include "chromecast/public/osd_plane_shlib.h"
#include "chromecast/public/osd_surface.h"

namespace chromecast {
namespace {

// Default no-op OsdSurface implementation
class OsdSurfaceDefault : public OsdSurface {
 public:
  OsdSurfaceDefault(const Size& size) : size_(size) {}

  // OsdSurface implementation:
  void Blit(OsdSurface* src_surface,
            const Rect& src_rect,
            const Point& dst_point) override {}
  void Composite(OsdSurface* src_surface,
                 const Rect& src_rect,
                 const Point& dst_point) override {}
  void CopyBitmap(char* src_bitmap,
                  const Rect& src_rect,
                  const Rect& damage_rect,
                  const Point& dst_point) override {}
  void Fill(const Rect& rect, int argb) override {}

  const Size& size() const override { return size_; }

 private:
  const Size size_;

  DISALLOW_COPY_AND_ASSIGN(OsdSurfaceDefault);
};

// Default no-op OsdPlane implementation
class OsdPlaneDefault : public OsdPlane {
 public:
  OsdPlaneDefault() : size_(0, 0) {}

  // OsdPlane implementation:
  OsdSurface* CreateSurface(const Size& size) override {
    return new OsdSurfaceDefault(size);
  }
  void SetClipRectangle(const Rect& rect,
                        const Size& screen_res,
                        float output_scale) override {
    size_ = Size(rect.width, rect.height);
  }
  OsdSurface* GetBackBuffer() override {
    if (!back_buffer_)
      back_buffer_.reset(new OsdSurfaceDefault(size_));
    return back_buffer_.get();
  }

  void Flip() override {}

 private:
  std::unique_ptr<OsdSurface> back_buffer_;
  Size size_;

  DISALLOW_COPY_AND_ASSIGN(OsdPlaneDefault);
};

}  // namespace

OsdPlane* OsdPlaneShlib::Create(const std::vector<std::string>& argv) {
  return new OsdPlaneDefault;
}

}  // namespace chromecast
