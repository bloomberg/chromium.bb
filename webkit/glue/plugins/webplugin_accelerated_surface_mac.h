// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBPLUGIN_ACCELERATED_SURFACE_MAC_H_
#define WEBKIT_GLUE_WEBPLUGIN_ACCELERATED_SURFACE_MAC_H_
#pragma once

#include "gfx/native_widget_types.h"
#include "gfx/size.h"

// Avoid having to include OpenGL headers here.
typedef struct _CGLContextObject* CGLContextObj;

namespace webkit_glue {

// Interface class for interacting with an accelerated plugin surface, used
// for the Core Animation flavors of plugin drawing on the Mac.
class WebPluginAcceleratedSurface {
 public:
  virtual ~WebPluginAcceleratedSurface() {}

  // Sets the window handle used throughout the browser to identify this
  // surface.
  virtual void SetWindowHandle(gfx::PluginWindowHandle window) = 0;

  // Sets the size of the surface.
  virtual void SetSize(const gfx::Size& size) = 0;

  // Returns the context used to draw into this surface.
  virtual CGLContextObj context() = 0;

  // Readies the surface for drawing. Must be called before any drawing session.
  virtual void StartDrawing() = 0;

  // Ends a drawing session. Changes to the surface may not be reflected until
  // this is called.
  virtual void EndDrawing() = 0;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBPLUGIN_ACCELERATED_SURFACE_MAC_H_
