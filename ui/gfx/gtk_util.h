// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GTK_UTIL_H_
#define UI_GFX_GTK_UTIL_H_
#pragma once

#include <glib-object.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/scoped_ptr.h"

typedef struct _GdkPixbuf GdkPixbuf;
typedef struct _GdkRegion GdkRegion;
typedef struct _GdkCursor GdkCursor;

class CommandLine;
class SkBitmap;

namespace gfx {

class Rect;

// Call gtk_init() using the argc and argv from command_line.
// gtk_init() wants an argc and argv that it can mutate; we provide those,
// but leave the original CommandLine unaltered.
void GtkInitFromCommandLine(const CommandLine& command_line);

// Convert and copy a SkBitmap to a GdkPixbuf. NOTE: this uses BGRAToRGBA, so
// it is an expensive operation.  The returned GdkPixbuf will have a refcount of
// 1, and the caller is responsible for unrefing it when done.
GdkPixbuf* GdkPixbufFromSkBitmap(const SkBitmap* bitmap);

// Modify the given region by subtracting the given rectangles.
void SubtractRectanglesFromRegion(GdkRegion* region,
                                  const std::vector<Rect>& cutouts);

// Returns the resolution (DPI) used by pango. A negative values means the
// resolution hasn't been set.
double GetPangoResolution();

// Returns a static instance of a GdkCursor* object, sharable across the
// process. Caller must gdk_cursor_ref() it if they want to assume ownership.
GdkCursor* GetCursor(int type);

// Change windows accelerator style to GTK style. (GTK uses _ for
// accelerators.  Windows uses & with && as an escape for &.)
std::string ConvertAcceleratorsFromWindowsStyle(const std::string& label);

// Removes the "&" accelerators from a Windows label.
std::string RemoveWindowsStyleAccelerators(const std::string& label);

// Makes a copy of |pixels| with the ordering changed from BGRA to RGBA.
// The caller is responsible for free()ing the data. If |stride| is 0, it's
// assumed to be 4 * |width|.
uint8_t* BGRAToRGBA(const uint8_t* pixels, int width, int height, int stride);

}  // namespace gfx

// It's not legal C++ to have a templatized typedefs, so we wrap it in a
// struct.  When using this, you need to include ::Type.  E.g.,
// ScopedGObject<GdkPixbufLoader>::Type loader(gdk_pixbuf_loader_new());
template<class T>
struct ScopedGObject {
  // A helper class that will g_object_unref |p| when it goes out of scope.
  // This never adds a ref, it only unrefs.
  template<class U>
  struct GObjectUnrefer {
    void operator()(U* ptr) const {
      if (ptr)
        g_object_unref(ptr);
    }
  };

  typedef scoped_ptr_malloc<T, GObjectUnrefer<T> > Type;
};

#endif  // UI_GFX_GTK_UTIL_H_
