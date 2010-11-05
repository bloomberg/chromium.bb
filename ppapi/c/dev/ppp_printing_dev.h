// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_DEV_PPP_PRINTING_DEV_H_
#define PPAPI_C_DEV_PPP_PRINTING_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

typedef enum {
  PP_PRINTORIENTATION_NORMAL         = 0,
  PP_PRINTORIENTATION_ROTATED_90_CW  = 1,
  PP_PRINTORIENTATION_ROTATED_180    = 2,
  PP_PRINTORIENTATION_ROTATED_90_CCW = 3
} PP_PrintOrientation_Dev;

typedef enum {
  PP_PRINTOUTPUTFORMAT_RASTER     = 0,
  PP_PRINTOUTPUTFORMAT_PDF        = 1,
  PP_PRINTOUTPUTFORMAT_POSTSCRIPT = 2
} PP_PrintOutputFormat_Dev;

struct PP_PrintSettings_Dev {
  // This is the size of the printable area in points (1/72 of an inch)
  struct PP_Rect printable_area;
  int32_t dpi;
  PP_PrintOrientation_Dev orientation;
  PP_Bool grayscale;
  PP_PrintOutputFormat_Dev format;
};

// Specifies a contiguous range of page numbers to be printed.
// The page numbers use a zero-based index.
struct PP_PrintPageNumberRange_Dev {
  uint32_t first_page_number;
  uint32_t last_page_number;
};

// Interface for the plugin to implement printing.
#define PPP_PRINTING_DEV_INTERFACE "PPP_Printing(Dev);0.2"

struct PPP_Printing_Dev {
  // Returns array of supported print output formats. The array is allocated
  // using PPB_Core.MemAlloc. The caller is responsible for freeing the array
  // using PPB_Core.MemFree.
  // Sets |*format_count| to 0 returns NULL if printing is not supported at all.
  PP_PrintOutputFormat_Dev* (*QuerySupportedFormats)(PP_Instance instance,
                                                     uint32_t* format_count);

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

#endif  // PPAPI_C_DEV_PPP_PRINTING_DEV_H_
