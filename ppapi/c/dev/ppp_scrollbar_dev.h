// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_DEV_PPP_SCROLLBAR_DEV_H_
#define PPAPI_C_DEV_PPP_SCROLLBAR_DEV_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

// Interface for the plugin to implement when using a scrollbar widget.
#define PPP_SCROLLBAR_DEV_INTERFACE "PPP_Scrollbar(Dev);0.1"

struct PPP_Scrollbar_Dev {
  // Informs the instance that the scrollbar's value has changed.
  void (*ValueChanged)(PP_Instance instance,
                       PP_Resource scrollbar,
                       uint32_t value);
};

#endif  // PPAPI_C_DEV_PPP_SCROLLBAR_DEV_H_
