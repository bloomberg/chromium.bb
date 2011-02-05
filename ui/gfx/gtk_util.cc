// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gtk_util.h"

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <stdlib.h>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/linux_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkUnPreMultiply.h"
#include "ui/gfx/rect.h"

namespace {

// A process wide singleton that manages our usage of gdk
// cursors. gdk_cursor_new() hits the disk in several places and GdkCursor
// instances can be reused throughout the process.
class GdkCursorCache {
 public:
   GdkCursorCache() {}
  ~GdkCursorCache() {
    for (std::map<GdkCursorType, GdkCursor*>::iterator it =
        cursor_cache_.begin(); it != cursor_cache_.end(); ++it) {
      gdk_cursor_unref(it->second);
    }
    cursor_cache_.clear();
  }

  GdkCursor* GetCursorImpl(GdkCursorType type) {
    std::map<GdkCursorType, GdkCursor*>::iterator it = cursor_cache_.find(type);
    GdkCursor* cursor = NULL;
    if (it == cursor_cache_.end()) {
      cursor = gdk_cursor_new(type);
      cursor_cache_.insert(std::make_pair(type, cursor));
    } else {
      cursor = it->second;
    }

    // It is not necessary to add a reference here. The callers can ref the
    // cursor if they need it for something.
    return cursor;
  }

  std::map<GdkCursorType, GdkCursor*> cursor_cache_;

  DISALLOW_COPY_AND_ASSIGN(GdkCursorCache);
};

void FreePixels(guchar* pixels, gpointer data) {
  free(data);
}

// Common implementation of ConvertAcceleratorsFromWindowsStyle() and
// RemoveWindowsStyleAccelerators().
// Replaces all ampersands (as used in our grd files to indicate mnemonics)
// to |target|. Similarly any underscores get replaced with two underscores as
// is needed by pango.
std::string ConvertAmperstandsTo(const std::string& label,
                                 const std::string& target) {
  std::string ret;
  ret.reserve(label.length() * 2);
  for (size_t i = 0; i < label.length(); ++i) {
    if ('_' == label[i]) {
      ret.push_back('_');
      ret.push_back('_');
    } else if ('&' == label[i]) {
      if (i + 1 < label.length() && '&' == label[i + 1]) {
        ret.push_back('&');
        ++i;
      } else {
        ret.append(target);
      }
    } else {
      ret.push_back(label[i]);
    }
  }

  return ret;
}

}  // namespace

namespace gfx {

void GtkInitFromCommandLine(const CommandLine& command_line) {
  const std::vector<std::string>& args = command_line.argv();
  int argc = args.size();
  scoped_array<char *> argv(new char *[argc + 1]);
  for (size_t i = 0; i < args.size(); ++i) {
    // TODO(piman@google.com): can gtk_init modify argv? Just being safe
    // here.
    argv[i] = strdup(args[i].c_str());
  }
  argv[argc] = NULL;
  char **argv_pointer = argv.get();

  gtk_init(&argc, &argv_pointer);
  for (size_t i = 0; i < args.size(); ++i) {
    free(argv[i]);
  }
}

GdkPixbuf* GdkPixbufFromSkBitmap(const SkBitmap* bitmap) {
  if (bitmap->isNull())
    return NULL;

  bitmap->lockPixels();

  int width = bitmap->width();
  int height = bitmap->height();
  int stride = bitmap->rowBytes();

  // SkBitmaps are premultiplied, we need to unpremultiply them.
  const int kBytesPerPixel = 4;
  uint8* divided = static_cast<uint8*>(malloc(height * stride));

  for (int y = 0, i = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      uint32 pixel = bitmap->getAddr32(0, y)[x];

      int alpha = SkColorGetA(pixel);
      if (alpha != 0 && alpha != 255) {
        SkColor unmultiplied = SkUnPreMultiply::PMColorToColor(pixel);
        divided[i + 0] = SkColorGetR(unmultiplied);
        divided[i + 1] = SkColorGetG(unmultiplied);
        divided[i + 2] = SkColorGetB(unmultiplied);
        divided[i + 3] = alpha;
      } else {
        divided[i + 0] = SkColorGetR(pixel);
        divided[i + 1] = SkColorGetG(pixel);
        divided[i + 2] = SkColorGetB(pixel);
        divided[i + 3] = alpha;
      }
      i += kBytesPerPixel;
    }
  }

  // This pixbuf takes ownership of our malloc()ed data and will
  // free it for us when it is destroyed.
  GdkPixbuf* pixbuf = gdk_pixbuf_new_from_data(
      divided,
      GDK_COLORSPACE_RGB,  // The only colorspace gtk supports.
      true,  // There is an alpha channel.
      8,
      width, height, stride, &FreePixels, divided);

  bitmap->unlockPixels();
  return pixbuf;
}

void SubtractRectanglesFromRegion(GdkRegion* region,
                                  const std::vector<Rect>& cutouts) {
  for (size_t i = 0; i < cutouts.size(); ++i) {
    GdkRectangle rect = cutouts[i].ToGdkRectangle();
    GdkRegion* rect_region = gdk_region_rectangle(&rect);
    gdk_region_subtract(region, rect_region);
    // TODO(deanm): It would be nice to be able to reuse the GdkRegion here.
    gdk_region_destroy(rect_region);
  }
}

double GetPangoResolution() {
  static double resolution;
  static bool determined_resolution = false;
  if (!determined_resolution) {
    determined_resolution = true;
    PangoContext* default_context = gdk_pango_context_get();
    resolution = pango_cairo_context_get_resolution(default_context);
    g_object_unref(default_context);
  }
  return resolution;
}

GdkCursor* GetCursor(int type) {
  static GdkCursorCache impl;
  return impl.GetCursorImpl(static_cast<GdkCursorType>(type));
}

std::string ConvertAcceleratorsFromWindowsStyle(const std::string& label) {
  return ConvertAmperstandsTo(label, "_");
}

std::string RemoveWindowsStyleAccelerators(const std::string& label) {
  return ConvertAmperstandsTo(label, "");
}

uint8_t* BGRAToRGBA(const uint8_t* pixels, int width, int height, int stride) {
  if (stride == 0)
    stride = width * 4;

  uint8_t* new_pixels = static_cast<uint8_t*>(malloc(height * stride));

  // We have to copy the pixels and swap from BGRA to RGBA.
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      int idx = i * stride + j * 4;
      new_pixels[idx] = pixels[idx + 2];
      new_pixels[idx + 1] = pixels[idx + 1];
      new_pixels[idx + 2] = pixels[idx];
      new_pixels[idx + 3] = pixels[idx + 3];
    }
  }

  return new_pixels;
}

}  // namespace gfx
