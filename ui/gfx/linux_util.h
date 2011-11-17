// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_LINUX_UTIL_H_
#define UI_GFX_LINUX_UTIL_H_
#pragma once

#include <glib-object.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "ui/base/ui_export.h"

typedef struct _PangoContext PangoContext;

namespace gfx {

// Creates and returns a PangoContext. The caller owns the context.
PangoContext* GetPangoContext();

// Returns the resolution (DPI) used by pango. A negative values means the
// resolution hasn't been set.
double GetPangoResolution();

// Change windows accelerator style to GTK style. (GTK uses _ for
// accelerators.  Windows uses & with && as an escape for &.)
UI_EXPORT std::string ConvertAcceleratorsFromWindowsStyle(
    const std::string& label);

// Removes the "&" accelerators from a Windows label.
UI_EXPORT std::string RemoveWindowsStyleAccelerators(const std::string& label);

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

#endif  // UI_GFX_LINUX_UTIL_H_
