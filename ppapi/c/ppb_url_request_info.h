/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PPB_URL_REQUEST_INFO_H_
#define PPAPI_C_PPB_URL_REQUEST_INFO_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"

struct PP_Var;

/**
 * @file
 * Defines the API ...
 */

/**
 *
 * @addtogroup Enums
 * @{
 */
typedef enum {
  PP_URLREQUESTPROPERTY_URL,                  // string
  PP_URLREQUESTPROPERTY_METHOD,               // string
  PP_URLREQUESTPROPERTY_HEADERS,              // string, \n-delim
  PP_URLREQUESTPROPERTY_STREAMTOFILE,         // PP_Bool (default=PP_FALSE)
  PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS,      // PP_Bool (default=PP_TRUE)

  // Set to true if you want to be able to poll the download progress via the
  // URLLoader.GetDownloadProgress function.
  //
  // Boolean (default = PP_FALSE).
  PP_URLREQUESTPROPERTY_RECORDDOWNLOADPROGRESS,

  // Set to true if you want to be able to pull the upload progress via the
  // URLLoader.GetUploadProgress function.
  //
  // Boolean (default = PP_FALSE).
  PP_URLREQUESTPROPERTY_RECORDUPLOADPROGRESS,

  // Set to a String to set a custom referrer (if empty, the referrer header
  // will be omitted), or to an Undefined Var to use the default referrer. Only
  // loaders with universal access (only available on trusted implementations)
  // will accept URLRequestInfo objects which try to set a custom referrer; if
  // given to a loader without universal access, PP_ERROR_BADARGUMENT will
  // result.
  //
  // Undefined/String (default = Undefined)
  PP_URLREQUESTPROPERTY_CUSTOMREFERRERURL
} PP_URLRequestProperty;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_URLRequestProperty, 4);
/**
 * @}
 */

#define PPB_URLREQUESTINFO_INTERFACE "PPB_URLRequestInfo;0.2"

/**
 *
 * @addtogroup Interfaces
 * @{
 */
struct PPB_URLRequestInfo {
  // Create a new URLRequestInfo object.  Returns 0 if the module is invalid.
  PP_Resource (*Create)(PP_Instance instance);

  // Returns PP_TRUE if the given resource is an URLRequestInfo. Returns
  // PP_FALSE if the resource is invalid or some type other than an
  // URLRequestInfo.
  PP_Bool (*IsURLRequestInfo)(PP_Resource resource);

  // Sets a request property.  Returns PP_FALSE if any of the parameters are
  // invalid, PP_TRUE on success.  The value property must be the correct type
  // according to the property being set.
  PP_Bool (*SetProperty)(PP_Resource request,
                         PP_URLRequestProperty property,
                         struct PP_Var value);

  // Append data to the request body.
  //
  // A Content-Length request header will be automatically generated.
  //
  // Returns PP_FALSE if any of the parameters are invalid, PP_TRUE on success.
  PP_Bool (*AppendDataToBody)(PP_Resource request,
                              const char* data,
                              uint32_t len);

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
  // Returns PP_FALSE if any of the parameters are invalid, PP_TRUE on success.
  PP_Bool (*AppendFileToBody)(PP_Resource request,
                              PP_Resource file_ref,
                              int64_t start_offset,
                              int64_t number_of_bytes,
                              PP_Time expected_last_modified_time);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_URL_REQUEST_INFO_H_ */

