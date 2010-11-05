// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_DEV_PPP_ZOOM_DEV_H_
#define PPAPI_C_DEV_PPP_ZOOM_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"

// Zoom interface should only apply to those full-page "plugin-document".
#define PPP_ZOOM_DEV_INTERFACE "PPP_Zoom(Dev);0.2"

struct PPP_Zoom_Dev {
  // Instruct plug-in to zoom according to the given factor and whether the zoom
  // only applies to text only.  The scale factor is the percentage divided by
  // 100, i.e. 150% zoom is 1.5.
  void (*Zoom)(PP_Instance instance,
               double factor,
               PP_Bool text_only);
};

#endif  // PPAPI_C_DEV_PPP_ZOOM_DEV_H_
