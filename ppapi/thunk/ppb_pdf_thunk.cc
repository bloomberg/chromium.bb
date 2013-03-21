// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_pdf_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Var GetLocalizedString(PP_Instance instance, PP_ResourceString string_id) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.functions()->GetLocalizedString(instance, string_id);
}

PP_Resource GetResourceImage(PP_Instance instance,
                             PP_ResourceImage image_id) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->GetResourceImage(instance, image_id);
}

PP_Resource GetFontFileWithFallback(
    PP_Instance instance,
    const PP_FontDescription_Dev* description,
    PP_PrivateFontCharset charset) {
  // Not implemented out-of-process.
  NOTIMPLEMENTED();
  return 0;
}

bool GetFontTableForPrivateFontFile(PP_Resource font_file,
                                    uint32_t table,
                                    void* output,
                                    uint32_t* output_length) {
  // Not implemented out-of-process.
  NOTIMPLEMENTED();
  return false;
}

void SearchString(PP_Instance instance,
                  const unsigned short* string,
                  const unsigned short* term,
                  bool case_sensitive,
                  PP_PrivateFindResult** results,
                  int* count) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.failed())
    return;
  enter.functions()->SearchString(instance, string, term, case_sensitive,
                                  results, count);
}

void DidStartLoading(PP_Instance instance) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.succeeded())
    enter.functions()->DidStartLoading(instance);
}

void DidStopLoading(PP_Instance instance) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.succeeded())
    enter.functions()->DidStopLoading(instance);
}

void SetContentRestriction(PP_Instance instance, int restrictions) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.succeeded())
    enter.functions()->SetContentRestriction(instance, restrictions);
}

void HistogramPDFPageCount(PP_Instance instance, int count) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.succeeded())
    enter.functions()->HistogramPDFPageCount(instance, count);
}

void UserMetricsRecordAction(PP_Instance instance, PP_Var action) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.succeeded())
    enter.functions()->UserMetricsRecordAction(instance, action);
}

void HasUnsupportedFeature(PP_Instance instance) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.succeeded())
    enter.functions()->HasUnsupportedFeature(instance);
}

void SaveAs(PP_Instance instance) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.succeeded())
    enter.functions()->SaveAs(instance);
}

void Print(PP_Instance instance) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.succeeded())
    enter.functions()->Print(instance);
}

PP_Bool IsFeatureEnabled(PP_Instance instance, PP_PDFFeature feature) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->IsFeatureEnabled(instance, feature);
}

PP_Resource GetResourceImageForScale(PP_Instance instance,
                                     PP_ResourceImage image_id,
                                     float scale) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->GetResourceImageForScale(instance, image_id, scale);
}

const PPB_PDF g_ppb_pdf_thunk = {
  &GetLocalizedString,
  &GetResourceImage,
  &GetFontFileWithFallback,
  &GetFontTableForPrivateFontFile,
  &SearchString,
  &DidStartLoading,
  &DidStopLoading,
  &SetContentRestriction,
  &HistogramPDFPageCount,
  &UserMetricsRecordAction,
  &HasUnsupportedFeature,
  &SaveAs,
  &Print,
  &IsFeatureEnabled,
  &GetResourceImageForScale
};

}  // namespace

const PPB_PDF* GetPPB_PDF_Thunk() {
  return &g_ppb_pdf_thunk;
}

}  // namespace thunk
}  // namespace ppapi
