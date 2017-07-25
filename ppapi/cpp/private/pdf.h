// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_PDF_H_
#define PPAPI_CPP_PRIVATE_PDF_H_

#include <stdint.h>

#include <string>

#include "ppapi/c/private/ppb_pdf.h"

struct PP_BrowserFont_Trusted_Description;

namespace pp {

class InstanceHandle;
class Var;

class PDF {
 public:
  // Returns true if the required interface is available.
  static bool IsAvailable();

  static PP_Resource GetFontFileWithFallback(
      const InstanceHandle& instance,
      const PP_BrowserFont_Trusted_Description* description,
      PP_PrivateFontCharset charset);
  static bool GetFontTableForPrivateFontFile(PP_Resource font_file,
                                             uint32_t table,
                                             void* output,
                                             uint32_t* output_length);
  static void SearchString(const InstanceHandle& instance,
                           const unsigned short* string,
                           const unsigned short* term,
                           bool case_sensitive,
                           PP_PrivateFindResult** results,
                           int* count);
  static void DidStartLoading(const InstanceHandle& instance);
  static void DidStopLoading(const InstanceHandle& instance);
  static void SetContentRestriction(const InstanceHandle& instance,
                                    int restrictions);
  static void UserMetricsRecordAction(const InstanceHandle& instance,
                                      const Var& action);
  static void HasUnsupportedFeature(const InstanceHandle& instance);
  static void SaveAs(const InstanceHandle& instance);
  static void Print(const InstanceHandle& instance);
  static bool IsFeatureEnabled(const InstanceHandle& instance,
                               PP_PDFFeature feature);
  static void SetSelectedText(const InstanceHandle& instance,
                              const char* selected_text);
  static void SetLinkUnderCursor(const InstanceHandle& instance,
                                 const char* url);
  static void GetV8ExternalSnapshotData(const InstanceHandle& instance,
                                        const char** natives_data_out,
                                        int* natives_size_out,
                                        const char** snapshot_data_out,
                                        int* snapshot_size_out);
  static void SetAccessibilityViewportInfo(
      const InstanceHandle& instance,
      PP_PrivateAccessibilityViewportInfo* viewport_info);
  static void SetAccessibilityDocInfo(
      const InstanceHandle& instance,
      PP_PrivateAccessibilityDocInfo* doc_info);
  static void SetAccessibilityPageInfo(
      const InstanceHandle& instance,
      PP_PrivateAccessibilityPageInfo* page_info,
      PP_PrivateAccessibilityTextRunInfo text_runs[],
      PP_PrivateAccessibilityCharInfo chars[]);
  static void SetCrashData(const InstanceHandle& instance,
                           const char* pdf_url,
                           const char* top_level_url);
};

}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_PDF_H_
