/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppp_printing_dev.idl modified Tue May 15 15:17:01 2012. */

#ifndef PPAPI_C_DEV_PPP_PRINTING_DEV_H_
#define PPAPI_C_DEV_PPP_PRINTING_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"

#define PPP_PRINTING_DEV_INTERFACE_0_6 "PPP_Printing(Dev);0.6"
#define PPP_PRINTING_DEV_INTERFACE PPP_PRINTING_DEV_INTERFACE_0_6

/**
 * @file
 * Implementation of the Printing interface.
 */


/**
 * @addtogroup Enums
 * @{
 */
typedef enum {
  PP_PRINTORIENTATION_NORMAL = 0,
  PP_PRINTORIENTATION_ROTATED_90_CW = 1,
  PP_PRINTORIENTATION_ROTATED_180 = 2,
  PP_PRINTORIENTATION_ROTATED_90_CCW = 3
} PP_PrintOrientation_Dev;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_PrintOrientation_Dev, 4);

typedef enum {
  PP_PRINTOUTPUTFORMAT_RASTER = 1u << 0,
  PP_PRINTOUTPUTFORMAT_PDF = 1u << 1,
  PP_PRINTOUTPUTFORMAT_POSTSCRIPT = 1u << 2,
  PP_PRINTOUTPUTFORMAT_EMF = 1u << 3
} PP_PrintOutputFormat_Dev;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_PrintOutputFormat_Dev, 4);

typedef enum {
  PP_PRINTSCALINGOPTION_NONE = 0,
  PP_PRINTSCALINGOPTION_FIT_TO_PRINTABLE_AREA = 1,
  PP_PRINTSCALINGOPTION_SOURCE_SIZE = 2
} PP_PrintScalingOption_Dev;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_PrintScalingOption_Dev, 4);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
struct PP_PrintSettings_Dev {
  /** This is the size of the printable area in points (1/72 of an inch) */
  struct PP_Rect printable_area;
  struct PP_Rect content_area;
  struct PP_Size paper_size;
  int32_t dpi;
  PP_PrintOrientation_Dev orientation;
  PP_PrintScalingOption_Dev print_scaling_option;
  PP_Bool grayscale;
  /** Note that Chrome currently only supports PDF printing. */
  PP_PrintOutputFormat_Dev format;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_PrintSettings_Dev, 60);

/**
 * Specifies a contiguous range of page numbers to be printed.
 * The page numbers use a zero-based index.
 */
struct PP_PrintPageNumberRange_Dev {
  uint32_t first_page_number;
  uint32_t last_page_number;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_PrintPageNumberRange_Dev, 8);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
struct PPP_Printing_Dev_0_6 {
  /**
   *  Returns a bit field representing the supported print output formats.  For
   *  example, if only PDF and PostScript are supported,
   *  QuerySupportedFormats returns a value equivalent to:
   *  (PP_PRINTOUTPUTFORMAT_PDF | PP_PRINTOUTPUTFORMAT_POSTSCRIPT)
   */
  uint32_t (*QuerySupportedFormats)(PP_Instance instance);
  /**
   * Begins a print session with the given print settings. Calls to PrintPages
   * can only be made after a successful call to Begin. Returns the number of
   * pages required for the print output at the given page size (0 indicates
   * a failure).
   */
  int32_t (*Begin)(PP_Instance instance,
                   const struct PP_PrintSettings_Dev* print_settings);
  /**
   * Prints the specified pages using the format specified in Begin.
   * Returns a PPB_Buffer resource that represents the printed output. Returns
   * 0 on failure.
   */
  PP_Resource (*PrintPages)(
      PP_Instance instance,
      const struct PP_PrintPageNumberRange_Dev* page_ranges,
      uint32_t page_range_count);
  /** Ends the print session. Further calls to PrintPages will fail. */
  void (*End)(PP_Instance instance);
  /**
   *  Returns true if the current content should be printed into the full page
   *  and not scaled down to fit within the printer's printable area.
   */
  PP_Bool (*IsScalingDisabled)(PP_Instance instance);
};

typedef struct PPP_Printing_Dev_0_6 PPP_Printing_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPP_PRINTING_DEV_H_ */

