/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PPP_SCROLLBAR_DEV_H_
#define PPAPI_C_DEV_PPP_SCROLLBAR_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

// Interface for the plugin to implement when using a scrollbar widget.
#define PPP_SCROLLBAR_DEV_INTERFACE_0_2 "PPP_Scrollbar(Dev);0.2"
#define PPP_SCROLLBAR_DEV_INTERFACE_0_3 "PPP_Scrollbar(Dev);0.3"
#define PPP_SCROLLBAR_DEV_INTERFACE PPP_SCROLLBAR_DEV_INTERFACE_0_3

struct PPP_Scrollbar_Dev {
  // Informs the instance that the scrollbar's value has changed.
  void (*ValueChanged)(PP_Instance instance,
                       PP_Resource scrollbar,
                       uint32_t value);

  // Informs the instance that the user has changed the system scrollbar style.
  void (*OverlayChanged)(PP_Instance instance,
                         PP_Resource scrollbar,
                         PP_Bool overlay);
};

#endif  /* PPAPI_C_DEV_PPP_SCROLLBAR_DEV_H_ */

