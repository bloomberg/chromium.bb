// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PDF_ACCESSIBILITY_SHARED_H_
#define PPAPI_SHARED_IMPL_PDF_ACCESSIBILITY_SHARED_H_

#include <string>

#include "ppapi/c/pp_rect.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

struct PPAPI_SHARED_EXPORT PdfAccessibilityLinkInfo {
  PdfAccessibilityLinkInfo();
  PdfAccessibilityLinkInfo(const PdfAccessibilityLinkInfo& other);
  PdfAccessibilityLinkInfo(const PP_PrivateAccessibilityLinkInfo& link);
  ~PdfAccessibilityLinkInfo();

  std::string url;
  uint32_t index_in_page;
  uint32_t text_run_index;
  uint32_t text_run_count;
  PP_FloatRect bounds;
};

struct PPAPI_SHARED_EXPORT PdfAccessibilityImageInfo {
  PdfAccessibilityImageInfo();
  PdfAccessibilityImageInfo(const PdfAccessibilityImageInfo& other);
  PdfAccessibilityImageInfo(const PP_PrivateAccessibilityImageInfo& image);
  ~PdfAccessibilityImageInfo();

  std::string alt_text;
  uint32_t text_run_index;
  PP_FloatRect bounds;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PDF_ACCESSIBILITY_SHARED_H_
