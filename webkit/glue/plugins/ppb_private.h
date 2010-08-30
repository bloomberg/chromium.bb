// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PPB_PRIVATE_H_
#define WEBKIT_GLUE_PLUGINS_PPB_PRIVATE_H_

#include "third_party/ppapi/c/pp_module.h"
#include "third_party/ppapi/c/pp_var.h"

#define PPB_PRIVATE_INTERFACE "PPB_Private;1"

typedef enum {
  PP_RESOURCESTRING_PDFGETPASSWORD = 0,
} PP_ResourceString;

typedef enum {
  PP_PRIVATEFONTPITCH_DEFAULT = 0,
  PP_PRIVATEFONTPITCH_FIXED = 1
} PP_PrivateFontPitch;

typedef enum {
  PP_PRIVATEFONTFAMILY_DEFAULT = 0,
  PP_PRIVATEFONTFAMILY_ROMAN = 1,
  PP_PRIVATEFONTFAMILY_SCRIPT = 2
} PP_PrivateFontFamily;

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
  PP_PrivateFontPitch pitch;
  PP_PrivateFontFamily family;
  PP_PrivateFontCharset charset;
};

struct PPB_Private {
  // Returns a localized string.
  PP_Var (*GetLocalizedString)(PP_ResourceString string_id);

  // Returns a resource identifying a font file corresponding to the given font
  // request after applying the browser-specific fallback. Linux only.
  PP_Resource (*GetFontFileWithFallback)(
      PP_Module module,
      const PP_PrivateFontFileDescription* description);

  // Given a resource previously returned by GetFontFileWithFallback, returns
  // a pointer to the requested font table. Linux only.
  bool (*GetFontTableForPrivateFontFile)(PP_Resource font_file,
                                         uint32_t table,
                                         void* output,
                                         uint32_t* output_length);
};

#endif  // WEBKIT_GLUE_PLUGINS_PPB_PRIVATE_H_
