/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PPP_PRINTING_DEV_H_
#define PPAPI_C_DEV_PPP_PRINTING_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

typedef enum {
  PP_PRINTORIENTATION_NORMAL         = 0,
  PP_PRINTORIENTATION_ROTATED_90_CW  = 1,
  PP_PRINTORIENTATION_ROTATED_180    = 2,
  PP_PRINTORIENTATION_ROTATED_90_CCW = 3
} PP_PrintOrientation_Dev;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_PrintOrientation_Dev, 4);

#ifdef PPP_PRINTING_DEV_USE_0_4
/* TODO(dmichael):  Remove the conditional compilation when NaCl proxy is
                    ported to 0.4. Remove 0.3 when PDF is ported. */
typedef enum {
  PP_PRINTOUTPUTFORMAT_RASTER_DEPRECATED     = 0,
  PP_PRINTOUTPUTFORMAT_PDF_DEPRECATED        = 1,
  PP_PRINTOUTPUTFORMAT_POSTSCRIPT_DEPRECATED = 2
} PP_PrintOutputFormat_Dev_0_3;
typedef enum {
  PP_PRINTOUTPUTFORMAT_RASTER     = 1u << 0,
  PP_PRINTOUTPUTFORMAT_PDF        = 1u << 1,
  PP_PRINTOUTPUTFORMAT_POSTSCRIPT = 1u << 2
} PP_PrintOutputFormat_Dev_0_4;
typedef PP_PrintOutputFormat_Dev_0_4 PP_PrintOutputFormat_Dev;
#else
typedef enum {
  PP_PRINTOUTPUTFORMAT_RASTER     = 0,
  PP_PRINTOUTPUTFORMAT_PDF        = 1,
  PP_PRINTOUTPUTFORMAT_POSTSCRIPT = 2
} PP_PrintOutputFormat_Dev_0_3;
typedef PP_PrintOutputFormat_Dev_0_3 PP_PrintOutputFormat_Dev;
#endif
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_PrintOutputFormat_Dev, 4);

struct PP_PrintSettings_Dev {
  // This is the size of the printable area in points (1/72 of an inch)
  struct PP_Rect printable_area;
  int32_t dpi;
  PP_PrintOrientation_Dev orientation;
  PP_Bool grayscale;
  PP_PrintOutputFormat_Dev format;
};
#ifdef PPP_PRINTING_DEV_USE_0_4
struct PP_PrintSettings_Dev_0_3 {
  struct PP_Rect printable_area;
  int32_t dpi;
  PP_PrintOrientation_Dev orientation;
  PP_Bool grayscale;
  PP_PrintOutputFormat_Dev_0_3 format;
};
#endif
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_PrintSettings_Dev, 32);

// Specifies a contiguous range of page numbers to be printed.
// The page numbers use a zero-based index.
struct PP_PrintPageNumberRange_Dev {
  uint32_t first_page_number;
  uint32_t last_page_number;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_PrintPageNumberRange_Dev, 8);

// Interface for the plugin to implement printing.
#define PPP_PRINTING_DEV_INTERFACE_0_3 "PPP_Printing(Dev);0.3"
#define PPP_PRINTING_DEV_INTERFACE_0_4 "PPP_Printing(Dev);0.4"
#ifdef PPP_PRINTING_DEV_USE_0_4
#define PPP_PRINTING_DEV_INTERFACE PPP_PRINTING_DEV_INTERFACE_0_4
#else
#define PPP_PRINTING_DEV_INTERFACE PPP_PRINTING_DEV_INTERFACE_0_3
#endif

#ifdef PPP_PRINTING_DEV_USE_0_4
struct PPP_Printing_Dev {
  // Returns a bit field representing the supported print output formats.  For
  // example, if only Raster and PostScript are supported,
  // QuerySupportedFormats returns a value equivalent to:
  // (PP_PRINTOUTPUTFORMAT_RASTER | PP_PRINTOUTPUTFORMAT_POSTSCRIPT)
  uint32_t (*QuerySupportedFormats)(PP_Instance instance);

  // Begins a print session with the given print settings. Calls to PrintPage
  // can only be made after a successful call to Begin. Returns the number of
  // pages required for the print output at the given page size (0 indicates
  // a failure).
  int32_t (*Begin)(PP_Instance instance,
                   const struct PP_PrintSettings_Dev* print_settings);

  // Prints the specified pages using the format specified in Begin.
  // Returns a resource that represents the printed output.
  // This is a PPB_ImageData resource if the output format is
  // PP_PrintOutputFormat_Raster and a PPB_Blob otherwise. Returns 0 on failure.
  PP_Resource (*PrintPages)(
      PP_Instance instance,
      const struct PP_PrintPageNumberRange_Dev* page_ranges,
      uint32_t page_range_count);

  // Ends the print session. Further calls to PrintPage will fail.
  void (*End)(PP_Instance instance);
};

struct PPP_Printing_Dev_0_3 {
  // Returns array of supported print output formats. The array is allocated
  // using PPB_Core.MemAlloc. The caller is responsible for freeing the array
  // using PPB_Core.MemFree.
  // Sets |*format_count| to 0 returns NULL if printing is not supported at all
  PP_PrintOutputFormat_Dev_0_3* (*QuerySupportedFormats)(
      PP_Instance instance, uint32_t* format_count);
  int32_t (*Begin)(PP_Instance instance,
                   const struct PP_PrintSettings_Dev_0_3* print_settings);
  // The following methods are the same as in 0_4.  See above for documentation.
  PP_Resource (*PrintPages)(
      PP_Instance instance,
      const struct PP_PrintPageNumberRange_Dev* page_ranges,
      uint32_t page_range_count);
  void (*End)(PP_Instance instance);
};
#else
struct PPP_Printing_Dev {
  PP_PrintOutputFormat_Dev* (*QuerySupportedFormats)(
      PP_Instance instance, uint32_t* format_count);
  int32_t (*Begin)(PP_Instance instance,
                   const struct PP_PrintSettings_Dev* print_settings);
  PP_Resource (*PrintPages)(
      PP_Instance instance,
      const struct PP_PrintPageNumberRange_Dev* page_ranges,
      uint32_t page_range_count);
  void (*End)(PP_Instance instance);
};
#endif

#endif  /* PPAPI_C_DEV_PPP_PRINTING_DEV_H_ */

