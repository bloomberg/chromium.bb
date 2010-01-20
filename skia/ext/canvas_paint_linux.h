
// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_CANVAS_PAINT_LINUX_H_
#define SKIA_EXT_CANVAS_PAINT_LINUX_H_

#include "base/logging.h"
#include "skia/ext/platform_canvas.h"

#include <gdk/gdk.h>

namespace skia {

// A class designed to translate skia painting into a region in a
// GdkWindow. This class has been adapted from the class with the same name in
// platform_canvas_win.h. On construction, it will set up a context for
// painting into, and on destruction, it will commit it to the GdkWindow.
template <class T>
class CanvasPaintT : public T {
 public:
  // This constructor assumes the result is opaque.
  explicit CanvasPaintT(GdkEventExpose* event)
      : context_(NULL),
        window_(event->window),
        region_(gdk_region_copy(event->region)),
        composite_alpha_(false) {
    init(true);
  }

  CanvasPaintT(GdkEventExpose* event, bool opaque)
      : context_(NULL),
        window_(event->window),
        region_(gdk_region_copy(event->region)),
        composite_alpha_(false) {
    init(opaque);
  }

  virtual ~CanvasPaintT() {
    if (!is_empty()) {
      T::restoreToCount(1);

      // Blit the dirty rect to the window.
      CHECK(window_);
      cairo_t* cr = gdk_cairo_create(window_);
      CHECK(cr);
      if (composite_alpha_)
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
      cairo_surface_t* source_surface = cairo_get_target(context_);
      CHECK(source_surface);
      GdkRectangle bounds = rectangle();
      cairo_set_source_surface(cr, source_surface, bounds.x, bounds.y);
      gdk_cairo_region(cr, region_);
      cairo_fill(cr);
      cairo_destroy(cr);
    }

    gdk_region_destroy(region_);
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
    return gdk_region_empty(region_);
  }

  GdkRectangle rectangle() const {
    GdkRectangle bounds;
    gdk_region_get_clipbox(region_, &bounds);
    return bounds;
  }

 private:
  void init(bool opaque) {
    GdkRectangle bounds = rectangle();
    if (!T::initialize(bounds.width, bounds.height, opaque, NULL)) {
      // Cause a deliberate crash;
      *(char*) 0 = 0;
    }

    // Need to translate so that the dirty region appears at the origin of the
    // surface.
    T::translate(-SkIntToScalar(bounds.x), -SkIntToScalar(bounds.y));

    context_ = T::getTopPlatformDevice().beginPlatformPaint();
  }

  cairo_t* context_;
  GdkWindow* window_;
  GdkRegion* region_;
  // See description above setter.
  bool composite_alpha_;

  // Disallow copy and assign.
  CanvasPaintT(const CanvasPaintT&);
  CanvasPaintT& operator=(const CanvasPaintT&);
};

typedef CanvasPaintT<PlatformCanvas> PlatformCanvasPaint;

}  // namespace skia

#endif  // SKIA_EXT_CANVAS_PAINT_LINUX_H_
