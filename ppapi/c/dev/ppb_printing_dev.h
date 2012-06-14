/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_printing_dev.idl modified Wed Jun 13 09:16:33 2012. */

#ifndef PPAPI_C_DEV_PPB_PRINTING_DEV_H_
#define PPAPI_C_DEV_PPB_PRINTING_DEV_H_

#include "ppapi/c/dev/pp_print_settings_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_PRINTING_DEV_INTERFACE_0_6 "PPB_Printing(Dev);0.6"
#define PPB_PRINTING_DEV_INTERFACE PPB_PRINTING_DEV_INTERFACE_0_6

/**
 * @file
 * Definition of the PPB_Printing interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_Printing_Dev_0_6 {
  /**
   * Outputs the default print settings for the default printer into
   * <code>print_settings</code>. Returns <code>PP_FALSE</code> on error.
   */
  PP_Bool (*GetDefaultPrintSettings)(
      PP_Instance instance,
      struct PP_PrintSettings_Dev* print_settings);
};

typedef struct PPB_Printing_Dev_0_6 PPB_Printing_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_PRINTING_DEV_H_ */

