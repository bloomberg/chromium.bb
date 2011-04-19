/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
 * This file defines the PPB_URLLoader interface for loading URLs.
 */

/**
 * @addtogroup Interfaces
 * @{
 */

/**
 * The PPB_URLLoader interface contains pointers to functions for loading URLs.
 * The typical steps for loading a URL are:
 *
 * -# Call Create() to create a URLLoader object.</li>
 * -# Create a URLRequestInfo object and set properties on it. Refer to
 * PPB_URLRequestInfo for further information.</li>
 * -# Call Open() with the URLRequestInfo as an argument.</li>
 * -# When Open() completes, call GetResponseInfo() to examine the response
 * headers. Refer to PPB_URLResponseInfo for further information.</li>
 * -# Call ReadResponseBody() to stream the data for the response.</li>
 *
 * Alternatively, if PP_URLREQUESTPROPERTY_STREAMTOFILE was set on the
 * URLRequestInfo in step #2:
 * - Call FinishStreamingToFile(), after examining the response headers
 * (step #4),  to wait for the downloaded file to be complete.
 * - Then, access the downloaded file using the GetBodyAsFileRef() function of
 * the URLResponseInfo returned in step #4.
 */
struct PPB_URLLoader {
  /**
   * Create is a pointer to a function that creates a new URLLoader
   * object. The URLLoader is associated with a particular instance, so that
   * any UI dialogs that need to be shown to the user can be positioned
   * relative to the window containing the instance.
   *
   * @param[in] instance A PP_Instance indentifying one instance of a module.
   * @return A PP_Resource corresponding to a URLLoader if successful, 0 if
   * the instance is invalid.
   */
  PP_Resource (*Create)(PP_Instance instance);

  /**
   * IsURLLoader is a pointer to a function that determines if a resource is a
   * URLLoader.
   *
   * @param[in] resource A PP_Resource corresponding to a URLLoader.
   * @return PP_TRUE if the resource is a URLLoader, PP_FALSE if the resource is
   * invalid or some type other than URLLoader.
   */
  PP_Bool (*IsURLLoader)(PP_Resource resource);

  /**
   * Open is a pointer to a function that begins loading the URLRequestInfo.
   * The operation completes when response headers are received or when an
   * error occurs.  Use GetResponseInfo() to access the response
   * headers.
   *
   * @param[in] loader A PP_Resource corresponding to a URLLoader.
   * @param[in] resource A PP_Resource corresponding to a URLRequestInfo.
   * @param[in] callback A PP_CompletionCallback to run on asynchronous
   * completion of Open(). This callback will run when response
   * headers for the url are received or error occured. This callback
   * will only run if Open() returns PP_OK_COMPLETIONPENDING.
   */
  int32_t (*Open)(PP_Resource loader,
                  PP_Resource request_info,
                  struct PP_CompletionCallback callback);

  /**
   * FollowRedirect is a pointer to a function that can be invoked to follow a
   * redirect after Open() completed on receiving redirect headers.
   *
   * @param[in] loader A PP_Resource corresponding to a URLLoader.
   * @param[in] callback A PP_CompletionCallback to run on asynchronous
   * completion of FollowRedirect(). This callback will run when response
   * headers for the redirect url are received or error occured. This callback
   * will only run if FollowRedirect() returns PP_OK_COMPLETIONPENDING.
   * @return An int32_t containing an error code from pp_errors.h.
   */
  int32_t (*FollowRedirect)(PP_Resource loader,
                            struct PP_CompletionCallback callback);

  /**
   * GetUploadProgress is a pointer to a function that returns the current
   * upload progress (which is meaningful after Open() has been called).
   * Progress only refers to the request body and does not include the
   * headers.
   *
   * This data is only available if the URLRequestInfo passed to Open() had the
   * PP_URLREQUESTPROPERTY_REPORTUPLOADPROGRESS property set to PP_TRUE.
   *
   * @param[in] loader A PP_Resource corresponding to a URLLoader.
   * @param[in] bytes_sent The number of bytes sent thus far.
   * @parm[in] total_bytes_to_be_sent The total number of bytes to be sent.
   * @return PP_TRUE if the upload progress is available, PP_FALSE if it is not
   * available.
   */
  PP_Bool (*GetUploadProgress)(PP_Resource loader,
                               int64_t* bytes_sent,
                               int64_t* total_bytes_to_be_sent);

  /**
   * GetDownloadProgress is a pointer to a function that returns the current
   * download progress, which is meaningful after Open() has been called.
   * Progress only refers to the response body and does not include the
   * headers.
   *
   * This data is only available if the URLRequestInfo passed to Open() had the
   * PP_URLREQUESTPROPERTY_REPORTDOWNLOADPROGRESS property set to PP_TRUE.
   *
   * @param[in] loader A PP_Resource corresponding to a URLLoader.
   * @param[in] bytes_received The number of bytes received thus far.
   * @parm[in] total_bytes_to_be_received The total number of bytes to be
   * received. The total bytes to be received may be unknown, in which case
   * total_bytes_to_be_received will be set to -1.
   * @return PP_TRUE if the download progress is available, PP_FALSE if it is
   * not available.
   */
  PP_Bool (*GetDownloadProgress)(PP_Resource loader,
                                 int64_t* bytes_received,
                                 int64_t* total_bytes_to_be_received);

  /**
   * GetResponseInfo is a pointer to a function that returns the current
   * URLResponseInfo object.
   *
   * @param[in] instance A PP_Resource corresponding to a URLLoader.
   * @return A PP_Resource corresponding to the URLResponseInfo if successful,
   * 0 if the loader is not a valid resource or if Open() has not been called.
   */
  PP_Resource (*GetResponseInfo)(PP_Resource loader);

  /**
   * ReadResponseBody is a pointer to a function used to read the response
   * body. The size of the buffer must be large enough to hold the specified
   * number of bytes to read.  This function might perform a partial read.
   *
   * @param[in] loader A PP_Resource corresponding to a URLLoader.
   * @param[in/out] buffer A pointer to the buffer for the response body.
   * @param[in] bytes_to_read The number of bytes to read.
   * @param[in] callback A PP_CompletionCallback to run on asynchronous
   * completion. The callback will run if the bytes (full or partial) are
   * read or an error occurs asynchronously. This callback will run only if this
   * function returns PP_OK_COMPLETIONPENDING.
   * @return An int32_t containing the number of bytes read or an error code
   * from pp_errors.h.
   */
  int32_t (*ReadResponseBody)(PP_Resource loader,
                              void* buffer,
                              int32_t bytes_to_read,
                              struct PP_CompletionCallback callback);
  /**
   * FinishStreamingToFile is a pointer to a function used to wait for the
   * response body to be completely downloaded to the file provided by the
   * GetBodyAsFileRef() in the current URLResponseInfo. This function is only
   * used if PP_URLREQUESTPROPERTY_STREAMTOFILE was set on the URLRequestInfo
   * passed to Open().
   *
   * @param[in] loader A PP_Resource corresponding to a URLLoader.
   * @param[in] callback A PP_CompletionCallback to run on asynchronous
   * completion. This callback will run when body is downloaded or an error
   * occurs after FinishStreamingToFile() returns PP_OK_COMPLETIONPENDING.
   * @return An int32_t containing the number of bytes read or an error code
   * from pp_errors.h.
   */
  int32_t (*FinishStreamingToFile)(PP_Resource loader,
                                   struct PP_CompletionCallback callback);

  /**
   * Close is a pointer to a function used to cancel any pending IO and close
   * the URLLoader object. Any pending callbacks will still run, reporting
   * PP_ERROR_ABORTED if pending IO was interrupted.  It is NOT valid to call
   * Open() again after a call to this function.
   *
   * Note: If the URLLoader object is destroyed while it is still open, then it
   * will be implicitly closed so you are not required to call Close().
   *
   * @param[in] loader A PP_Resource corresponding to a URLLoader.
   */
  void (*Close)(PP_Resource loader);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_URL_LOADER_H_ */

