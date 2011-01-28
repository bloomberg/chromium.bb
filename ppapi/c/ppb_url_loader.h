/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PPB_URL_LOADER_H_
#define PPAPI_C_PPB_URL_LOADER_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

struct PP_CompletionCallback;

#define PPB_URLLOADER_INTERFACE "PPB_URLLoader;0.1"

/**
 * @file
 * Defines the API ...
 */

/**
 * @addtogroup Interfaces
 * @{
 */

// The interface for loading URLs.
//
// Typical steps for loading an URL:
// 1- Create an URLLoader object.
// 2- Create an URLRequestInfo object and set properties on it.
// 3- Call URLLoader's Open method passing the URLRequestInfo.
// 4- When Open completes, call GetResponseInfo to examine the response headers.
// 5- Then call ReadResponseBody to stream the data for the response.
//
// Alternatively, if PP_URLREQUESTPROPERTY_STREAMTOFILE was set on the
// URLRequestInfo, then call FinishStreamingToFile at step #5 to wait for the
// downloaded file to be complete.  The downloaded file may be accessed via the
// GetBody method of the URLResponseInfo returned in step #4.
//
struct PPB_URLLoader {
  // Create a new URLLoader object.  Returns 0 if the instance is invalid.  The
  // URLLoader is associated with a particular instance, so that any UI dialogs
  // that need to be shown to the user can be positioned relative to the window
  // containing the instance.  It is also important for security reasons to
  // know the origin of the URL request.
  PP_Resource (*Create)(PP_Instance instance);

  // Returns PP_TRUE if the given resource is an URLLoader. Returns PP_FALSE if
  // the resource is invalid or some type other than an URLLoader.
  PP_Bool (*IsURLLoader)(PP_Resource resource);

  // Begins loading the URLRequestInfo.  Completes when response headers are
  // received or when an error occurs.  Use the GetResponseInfo method to
  // access the response headers.
  int32_t (*Open)(PP_Resource loader,
                  PP_Resource request_info,
                  struct PP_CompletionCallback callback);

  // If the current URLResponseInfo object corresponds to a redirect, then call
  // this method to follow the redirect.
  int32_t (*FollowRedirect)(PP_Resource loader,
                            struct PP_CompletionCallback callback);

  // Returns the current upload progress, which is meaningful after Open has
  // been called. Progress only refers to the request body and does not include
  // the headers.
  //
  // This data is only available if the URLRequestInfo passed to Open() had the
  // PP_URLREQUESTPROPERTY_REPORTUPLOADPROGRESS flag set to PP_TRUE.
  //
  // This method returns PP_FALSE if upload progress is not available.
  PP_Bool (*GetUploadProgress)(PP_Resource loader,
                               int64_t* bytes_sent,
                               int64_t* total_bytes_to_be_sent);

  // Returns the current download progress, which is meaningful after Open has
  // been called.  Progress only refers to the response body and does not
  // include the headers.
  //
  // This data is only available if the URLRequestInfo passed to Open() had the
  // PP_URLREQUESTPROPERTY_REPORTDOWNLOADPROGRESS flag set to PP_TRUE.
  //
  // The total bytes to be received may be unknown, in which case
  // total_bytes_to_be_received will be set to -1. This method returns PP_FALSE
  // if download progress is not available.
  PP_Bool (*GetDownloadProgress)(PP_Resource loader,
                                 int64_t* bytes_received,
                                 int64_t* total_bytes_to_be_received);

  // Returns the current URLResponseInfo object.
  PP_Resource (*GetResponseInfo)(PP_Resource loader);

  // Call this method to read the response body.  The size of the buffer must
  // be large enough to hold the specified number of bytes to read.  May
  // perform a partial read.  Returns the number of bytes read or an error
  // code.
  int32_t (*ReadResponseBody)(PP_Resource loader,
                              char* buffer,
                              int32_t bytes_to_read,
                              struct PP_CompletionCallback callback);

  // If PP_URLREQUESTPROPERTY_STREAMTOFILE was set on the URLRequestInfo passed
  // to the Open method, then this method may be used to wait for the response
  // body to be completely downloaded to the file provided by URLResponseInfo's
  // GetBody method.
  int32_t (*FinishStreamingToFile)(PP_Resource loader,
                                   struct PP_CompletionCallback callback);

  // Cancels any IO that may be pending, and closes the URLLoader object.  Any
  // pending callbacks will still run, reporting PP_ERROR_ABORTED if pending IO
  // was interrupted.  It is NOT valid to call Open again after a call to this
  // method.  Note: If the URLLoader object is destroyed, and it is still open,
  // then it will be implicitly closed, so you are not required to call the
  // Close method.
  void (*Close)(PP_Resource loader);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_URL_LOADER_H_ */

