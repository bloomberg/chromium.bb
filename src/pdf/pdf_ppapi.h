// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_PPAPI_H_
#define PDF_PDF_PPAPI_H_

#include "ppapi/c/ppb.h"
#include "ppapi/cpp/module.h"

namespace chrome_pdf {

int PPP_InitializeModule(PP_Module module_id,
                         PPB_GetInterface get_browser_interface);
void PPP_ShutdownModule();
const void* PPP_GetInterface(const char* interface_name);

bool IsSDKInitializedViaPepper();

}  // namespace chrome_pdf

#endif  // PDF_PDF_PPAPI_H_
