// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_PDF_API_H_
#define PPAPI_THUNK_PPB_PDF_API_H_

#include "ppapi/c/private/ppb_pdf.h"

namespace ppapi {
namespace thunk {

class PPB_PDF_API {
 public:
  virtual PP_Var GetLocalizedString(PP_Instance instance,
                                    PP_ResourceString string_id) = 0;
  virtual PP_Resource GetResourceImage(PP_Instance instance,
                                       PP_ResourceImage image_id) = 0;
  virtual void SearchString(PP_Instance instance,
                            const unsigned short* input_string,
                            const unsigned short* input_term,
                            bool case_sensitive,
                            PP_PrivateFindResult** results,
                            int* count) = 0;
  virtual void DidStartLoading(PP_Instance instance) = 0;
  virtual void DidStopLoading(PP_Instance instance) = 0;
  virtual void SetContentRestriction(PP_Instance instance,
                                     int restrictions) = 0;
  virtual void HistogramPDFPageCount(PP_Instance instance, int count) = 0;
  virtual void UserMetricsRecordAction(PP_Instance instance,
                                       const PP_Var& action) = 0;
  virtual void HasUnsupportedFeature(PP_Instance instance) = 0;
  virtual void SaveAs(PP_Instance instance) = 0;
  virtual PP_Bool IsFeatureEnabled(PP_Instance instance,
                                   PP_PDFFeature feature) = 0;
  virtual void Print(PP_Instance instance) = 0;
  virtual PP_Resource GetResourceImageForScale(PP_Instance instance,
                                               PP_ResourceImage image_id,
                                               int scale) = 0;

  static const SingletonResourceID kSingletonResourceID = PDF_SINGLETON_ID;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK__API_H_
