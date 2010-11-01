// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_DEV_PPB_WIDGET_DEV_H_
#define PPAPI_C_DEV_PPB_WIDGET_DEV_H_

#include "ppapi/c/pp_resource.h"

struct PP_Rect;
struct PP_InputEvent;

#define PPB_WIDGET_DEV_INTERFACE "PPB_Widget(Dev);0.1"

// The interface for reusing browser widgets.
struct PPB_Widget_Dev {
  // Returns true if the given resource is a Widget. Returns false if the
  // resource is invalid or some type other than an Widget.
  bool (*IsWidget)(PP_Resource resource);

  // Paint the given rectangle of the widget into the given image.
  // Returns true on success, false on failure
  bool (*Paint)(PP_Resource widget,
                const struct PP_Rect* rect,
                PP_Resource image);

  // Pass in an event to a widget.  It'll return true if the event was consumed.
  bool (*HandleEvent)(PP_Resource widget,
                      const struct PP_InputEvent* event);

  // Get/set the location of the widget.
  bool (*GetLocation)(PP_Resource widget,
                      struct PP_Rect* location);

  void (*SetLocation)(PP_Resource widget,
                      const struct PP_Rect* location);
};

#endif  // PPAPI_C_DEV_PPB_WIDGET_DEV_H_
