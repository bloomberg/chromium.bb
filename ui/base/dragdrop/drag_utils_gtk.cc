// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/drag_utils.h"

#include <gtk/gtk.h>

#include "base/logging.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_gtk.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"

namespace drag_utils {

void SetDragImageOnDataObject(const gfx::ImageSkia& image_skia,
                              const gfx::Size& size,
                              const gfx::Vector2d& cursor_offset,
                              ui::OSExchangeData* data_object) {
  ui::OSExchangeDataProviderGtk& provider(
      static_cast<ui::OSExchangeDataProviderGtk&>(data_object->provider()));

  // Convert the image into a GdkPixbuf.
  GdkPixbuf* canvas_pixbuf = gfx::GdkPixbufFromSkBitmap(image_skia.bitmap());

  // Make a new pixbuf of the requested size and copy it over.
  GdkPixbuf* pixbuf = gdk_pixbuf_new(
      gdk_pixbuf_get_colorspace(canvas_pixbuf),
      gdk_pixbuf_get_has_alpha(canvas_pixbuf),
      gdk_pixbuf_get_bits_per_sample(canvas_pixbuf),
      size.width(),
      size.height());
  gdk_pixbuf_copy_area(canvas_pixbuf, 0, 0, size.width(), size.height(), pixbuf,
                       0, 0);
  g_object_unref(canvas_pixbuf);

  // Set the drag data on to the provider.
  provider.SetDragImage(pixbuf, cursor_offset);
  g_object_unref(pixbuf);
}

}  // namespace drag_utils
