// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPP_PDF_H_
#define WEBKIT_PLUGINS_PPAPI_PPP_PDF_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_var.h"

#define PPP_PDF_INTERFACE_1 "PPP_Pdf;1"
#define PPP_PDF_INTERFACE PPP_PDF_INTERFACE_1

struct PPP_Pdf_1 {
  // Returns an absolute URL if the position is over a link.
  PP_Var (*GetLinkAtPosition)(PP_Instance instance,
                              PP_Point point);
};

typedef PPP_Pdf_1 PPP_Pdf;

#endif  // WEBKIT_PLUGINS_PPAPI_PPP_PDF_H_
