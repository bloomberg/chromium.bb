// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

GdkCursor* GetCursor(int type) {
  static GdkCursorCache impl;
  return impl.GetCursorImpl(static_cast<GdkCursorType>(type));
}

#if !defined(USE_WAYLAND) && !defined(USE_AURA)
PangoContext* GetPangoContext() {
  return gdk_pango_context_get();
}
#endif

void InitRCStyles() {
  static const char kRCText[] =
      // Make our dialogs styled like the GNOME HIG.
      //
      // TODO(evanm): content-area-spacing was introduced in a later
      // version of GTK, so we need to set that manually on all dialogs.
      // Perhaps it would make sense to have a shared FixupDialog() function.
      "style \"gnome-dialog\" {\n"
      "  xthickness = 12\n"
      "  GtkDialog::action-area-border = 0\n"
      "  GtkDialog::button-spacing = 6\n"
      "  GtkDialog::content-area-spacing = 18\n"
      "  GtkDialog::content-area-border = 12\n"
      "}\n"
      // Note we set it at the "application" priority, so users can override.
      "widget \"GtkDialog\" style : application \"gnome-dialog\"\n"

      // Make our about dialog special, so the image is flush with the edge.
      "style \"about-dialog\" {\n"
      "  GtkDialog::action-area-border = 12\n"
      "  GtkDialog::button-spacing = 6\n"
      "  GtkDialog::content-area-spacing = 18\n"
      "  GtkDialog::content-area-border = 0\n"
      "}\n"
      "widget \"about-dialog\" style : application \"about-dialog\"\n";

  gtk_rc_parse_string(kRCText);
}

}  // namespace gfx
