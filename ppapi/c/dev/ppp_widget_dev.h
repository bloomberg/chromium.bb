// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_DEV_PPP_WIDGET_DEV_H_
#define PPAPI_C_DEV_PPP_WIDGET_DEV_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_rect.h"

// Interface for the plugin to implement when using a widget.
#define PPP_WIDGET_DEV_INTERFACE "PPP_Widget(Dev);0.1"

struct PPP_Widget_Dev {
  // Informs the instance that the given rectangle needs to be repainted.
  void (*Invalidate)(PP_Instance instance,
                     PP_Resource widget,
                     const struct PP_Rect* dirty_rect);
};

#endif  // PPAPI_C_DEV_PPP_WIDGET_DEV_H_
