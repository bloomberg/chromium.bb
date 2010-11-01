// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_DEV_PPB_URL_RESPONSE_INFO_DEV_H_
#define PPAPI_C_DEV_PPB_URL_RESPONSE_INFO_DEV_H_

#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"

typedef enum {
  PP_URLRESPONSEPROPERTY_URL,             // string
  PP_URLRESPONSEPROPERTY_REDIRECTURL,     // string
  PP_URLRESPONSEPROPERTY_REDIRECTMETHOD,  // string
  PP_URLRESPONSEPROPERTY_STATUSCODE,      // int32
  PP_URLRESPONSEPROPERTY_STATUSLINE,      // string
  PP_URLRESPONSEPROPERTY_HEADERS          // string, \n-delim
} PP_URLResponseProperty_Dev;

#define PPB_URLRESPONSEINFO_DEV_INTERFACE "PPB_URLResponseInfo(Dev);0.1"

struct PPB_URLResponseInfo_Dev {
  // Returns true if the given resource is an URLResponseInfo. Returns false if
  // the resource is invalid or some type other than an URLResponseInfo.
  bool (*IsURLResponseInfo)(PP_Resource resource);

  // Gets a response property.  Return PP_VarType_Void if an input parameter is
  // invalid.
  PP_Var (*GetProperty)(PP_Resource response,
                        PP_URLResponseProperty_Dev property);

  // Returns a FileRef pointing to the file containing the response body.  This
  // is only valid if PP_URLREQUESTPROPERTY_STREAMTOFILE was set on the
  // URLRequestInfo used to produce this response.  This file remains valid
  // until the URLLoader associated with this URLResponseInfo is closed or
  // destroyed.  Returns 0 if PP_URLREQUESTPROPERTY_STREAMTOFILE was not
  // requested or if the URLLoader has not been opened yet.
  PP_Resource (*GetBody)(PP_Resource response);
};

#endif  // PPAPI_C_DEV_PPB_URL_RESPONSE_INFO_DEV_H_
