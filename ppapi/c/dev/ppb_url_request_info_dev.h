// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_DEV_PPB_URL_REQUEST_INFO_DEV_H_
#define PPAPI_C_DEV_PPB_URL_REQUEST_INFO_DEV_H_

#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"

struct PP_Var;

typedef enum {
  PP_URLREQUESTPROPERTY_URL,                  // string
  PP_URLREQUESTPROPERTY_METHOD,               // string
  PP_URLREQUESTPROPERTY_HEADERS,              // string, \n-delim
  PP_URLREQUESTPROPERTY_STREAMTOFILE,         // bool (default=false)
  PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS,      // bool (default=true)

  // Set to true if you want to be able to poll the download progress via the
  // URLLoader.GetDownloadProgress function.
  //
  // Boolean (default = false).
  PP_URLREQUESTPROPERTY_RECORDDOWNLOADPROGRESS,

  // Set to true if you want to be able to pull the upload progress via the
  // URLLoader.GetUploadProgress function.
  //
  // Boolean (default = false).
  PP_URLREQUESTPROPERTY_RECORDUPLOADPROGRESS

  // TODO(darin): Add security/privacy options?
} PP_URLRequestProperty_Dev;

#define PPB_URLREQUESTINFO_DEV_INTERFACE "PPB_URLRequestInfo(Dev);0.1"

struct PPB_URLRequestInfo_Dev {
  // Create a new URLRequestInfo object.  Returns 0 if the module is invalid.
  PP_Resource (*Create)(PP_Module module);

  // Returns true if the given resource is an URLRequestInfo. Returns false if
  // the resource is invalid or some type other than an URLRequestInfo.
  bool (*IsURLRequestInfo)(PP_Resource resource);

  // Sets a request property.  Returns false if any of the parameters are
  // invalid.  The value property must be the correct type according to the
  // property being set.
  bool (*SetProperty)(PP_Resource request,
                      PP_URLRequestProperty_Dev property,
                      struct PP_Var value);

  // Append data to the request body.
  //
  // A Content-Length request header will be automatically generated.
  //
  // Returns false if any of the parameters are invalid.
  bool (*AppendDataToBody)(PP_Resource request, const char* data, uint32_t len);

  // Append a file reference to be uploaded.
  //
  // A sub-range of the file starting from start_offset may be specified.  If
  // number_of_bytes is -1, then the sub-range to upload extends to the end of
  // the file.
  //
  // An optional (non-zero) last modified time stamp may be provided, which
  // will be used to validate that the file was not modified since the given
  // time before it is uploaded.  The upload will fail with an error code of
  // PP_Error_FileChanged if the file has been modified since the given time.
  // If expected_last_modified_time is 0, then no validation is performed.
  //
  // A Content-Length request header will be automatically generated.
  //
  // Returns false if any of the parameters are invalid.
  bool (*AppendFileToBody)(PP_Resource request,
                           PP_Resource file_ref,
                           int64_t start_offset,
                           int64_t number_of_bytes,
                           PP_Time expected_last_modified_time);
};

#endif  // PPAPI_C_DEV_PPB_URL_REQUEST_INFO_DEV_H_
