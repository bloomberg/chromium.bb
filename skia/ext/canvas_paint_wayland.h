// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_CANVAS_PAINT_WAYLAND_H_
#define SKIA_EXT_CANVAS_PAINT_WAYLAND_H_
#pragma once

#include "base/logging.h"
#include "skia/ext/platform_canvas.h"

namespace skia {

// A class designed to translate skia painting into a region in a Wayland window
// surface. On construction, it will set up a context for painting into, and on
// destruction, it will commit it to the Wayland window surface.
template <class T>
class CanvasPaintT : public T {
 public:
  // This constructor assumes the result is opaque.
  CanvasPaintT(cairo_surface_t* cairo_window_surface,
               cairo_rectangle_int_t* region)
      : context_(NULL),
        cairo_window_surface_(cairo_window_surface),
        region_(region),
        composite_alpha_(false) {
    init(true);
  }

  CanvasPaintT(cairo_surface_t* cairo_window_surface,
               cairo_rectangle_int_t* region,
               bool opaque)
      : context_(NULL),
        cairo_window_surface_(cairo_window_surface),
        region_(region),
        composite_alpha_(false) {
    init(opaque);
  }

  virtual ~CanvasPaintT() {
    if (!is_empty()) {
      T::restoreToCount(1);

      // Blit the dirty rect to the window.
      CHECK(cairo_window_surface_);
      cairo_t* cr = cairo_create(cairo_window_surface_);
      CHECK(cr);

      if (composite_alpha_)
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

      cairo_surface_t* source_surface = cairo_get_target(context_);
      CHECK(source_surface);
      // Flush cairo's cache of the surface.
      cairo_surface_mark_dirty(source_surface);
      cairo_set_source_surface(cr, source_surface, region_->x, region_->y);
      cairo_rectangle(cr,
                      region_->x,
                      region_->y,
                      region_->width,
                      region_->height);
      cairo_fill(cr);
      cairo_destroy(cr);
    }
  }

  // Sets whether the bitmap is composited in such a way that the alpha channel
  // is honored. This is only useful if you've enabled an RGBA colormap on the
  // widget. The default is false.
  void set_composite_alpha(bool composite_alpha) {
    composite_alpha_ = composite_alpha;
  }

  // Returns true if the invalid region is empty. The caller should call this
  // function to determine if anything needs painting.
  bool is_empty() const {
    return region_->width == 0 && region_->height == 0;
  }

 private:
  void init(bool opaque) {
    if (!T::initialize(region_->width, region_->height, opaque, NULL)) {
      // Cause a deliberate crash;
      CHECK(false);
    }

    // Need to translate so that the dirty region appears at the origin of the
    // surface.
    T::translate(-SkIntToScalar(region_->x), -SkIntToScalar(region_->y));

    context_ = BeginPlatformPaint(this);
  }

  cairo_t* context_;
  cairo_surface_t* cairo_window_surface_;
  cairo_rectangle_int_t* region_;
  // See description above setter.
  bool composite_alpha_;

  // Disallow copy and assign.
  CanvasPaintT(const CanvasPaintT&);
  CanvasPaintT& operator=(const CanvasPaintT&);
};

typedef CanvasPaintT<PlatformCanvas> PlatformCanvasPaint;

}  // namespace skia

#endif  // SKIA_EXT_CANVAS_PAINT_WAYLAND_H_
