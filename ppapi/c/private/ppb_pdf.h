// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_PRIVATE_PPB_PDF_H_
#define PPAPI_C_PRIVATE_PPB_PDF_H_

#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"

#define PPB_PDF_INTERFACE "PPB_PDF;1"

// From the public PPB_Font_Dev file.
struct PP_FontDescription_Dev;

typedef enum {
  PP_RESOURCESTRING_PDFGETPASSWORD = 0,
  PP_RESOURCESTRING_PDFLOADING = 1,
  PP_RESOURCESTRING_PDFLOAD_FAILED = 2,
} PP_ResourceString;

typedef enum {
  PP_RESOURCEIMAGE_PDF_BUTTON_FTH = 0,
  PP_RESOURCEIMAGE_PDF_BUTTON_FTH_HOVER = 1,
  PP_RESOURCEIMAGE_PDF_BUTTON_FTH_PRESSED = 2,
  PP_RESOURCEIMAGE_PDF_BUTTON_FTW = 3,
  PP_RESOURCEIMAGE_PDF_BUTTON_FTW_HOVER = 4,
  PP_RESOURCEIMAGE_PDF_BUTTON_FTW_PRESSED = 5,
  PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMIN = 6,
  PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMIN_HOVER = 7,
  PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMIN_PRESSED = 8,
  PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMOUT = 9,
  PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMOUT_HOVER = 10,
  PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMOUT_PRESSED = 11,
  PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_0 = 12,
  PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_1 = 13,
  PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_2 = 14,
  PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_3 = 15,
  PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_4 = 16,
  PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_5 = 17,
  PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_6 = 18,
  PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_7 = 19,
  PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_8 = 20,
  PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_9 = 21,
  PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_NUM_BACKGROUND = 22,
} PP_ResourceImage;

typedef enum {
  PP_PRIVATEFONTCHARSET_ANSI = 0,
  PP_PRIVATEFONTCHARSET_DEFAULT = 1,
  PP_PRIVATEFONTCHARSET_SYMBOL = 2,
  PP_PRIVATEFONTCHARSET_MAC = 77,
  PP_PRIVATEFONTCHARSET_SHIFTJIS = 128,
  PP_PRIVATEFONTCHARSET_HANGUL = 129,
  PP_PRIVATEFONTCHARSET_JOHAB = 130,
  PP_PRIVATEFONTCHARSET_GB2312 =134,
  PP_PRIVATEFONTCHARSET_CHINESEBIG5 = 136,
  PP_PRIVATEFONTCHARSET_GREEK = 161,
  PP_PRIVATEFONTCHARSET_TURKISH = 162,
  PP_PRIVATEFONTCHARSET_VIETNAMESE = 163,
  PP_PRIVATEFONTCHARSET_HEBREW = 177,
  PP_PRIVATEFONTCHARSET_ARABIC = 178,
  PP_PRIVATEFONTCHARSET_BALTIC = 186,
  PP_PRIVATEFONTCHARSET_RUSSIAN = 204,
  PP_PRIVATEFONTCHARSET_THAI = 222,
  PP_PRIVATEFONTCHARSET_EASTEUROPE = 238,
  PP_PRIVATEFONTCHARSET_OEM = 255
} PP_PrivateFontCharset;

struct PP_PrivateFontFileDescription {
  const char* face;
  uint32_t weight;
  bool italic;
};

struct PP_PrivateFindResult {
  int start_index;
  int length;
};

struct PPB_PDF {
  // Returns a localized string.
  struct PP_Var (*GetLocalizedString)(PP_Instance instance,
                                      PP_ResourceString string_id);

  // Returns a resource image.
  PP_Resource (*GetResourceImage)(PP_Instance instance,
                                  PP_ResourceImage image_id);

  // Returns a resource identifying a font file corresponding to the given font
  // request after applying the browser-specific fallback.
  //
  // Currently Linux-only.
  PP_Resource (*GetFontFileWithFallback)(
      PP_Instance instance,
      const struct PP_FontDescription_Dev* description,
      PP_PrivateFontCharset charset);

  // Given a resource previously returned by GetFontFileWithFallback, returns
  // a pointer to the requested font table. Linux only.
  bool (*GetFontTableForPrivateFontFile)(PP_Resource font_file,
                                         uint32_t table,
                                         void* output,
                                         uint32_t* output_length);

  // Search the given string using ICU.  Use PPB_Core's MemFree on results when
  // done.
  void (*SearchString)(
     PP_Instance instance,
     const unsigned short* string,
     const unsigned short* term,
     bool case_sensitive,
     struct PP_PrivateFindResult** results,
     int* count);

  // Since WebFrame doesn't know about PPAPI requests, it'll think the page has
  // finished loading even if there are outstanding requests by the plugin.
  // Take this out once WebFrame knows about requests by PPAPI plugins.
  void (*DidStartLoading)(PP_Instance instance);
  void (*DidStopLoading)(PP_Instance instance);

  // Sets content restriction for a full-page plugin (i.e. can't copy/print).
  // The value is a bitfield of ContentRestriction enums.
  void (*SetContentRestriction)(PP_Instance instance, int restrictions);

  // Use UMA so we know average pdf page count.
  void (*HistogramPDFPageCount)(int count);

  // Notifies the browser that the given action has been performed.
  void (*UserMetricsRecordAction)(struct PP_Var action);

  // Notifies the browser that the PDF has an unsupported feature.
  void (*HasUnsupportedFeature)(PP_Instance instance);
};

#endif  // PPAPI_C_PRIVATE_PPB_PDF_H_
