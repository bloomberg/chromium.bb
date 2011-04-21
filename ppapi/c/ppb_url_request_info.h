/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
 * This file defines the PPB_URLRequestInfo API for creating and manipulating
 * URL requests. This API is used in conjunction with PPB_URLLoader.
 */

/**
 * @addtogroup Enums
 * @{
 */

/**
 * This enumeration contains properties that can be set on a URL request.
 */
typedef enum {
  /** This corresponds to a string (PP_VARTYPE_STRING). */
  PP_URLREQUESTPROPERTY_URL,

  /**
   * This corresponds to a string (PP_VARTYPE_STRING); either POST or GET.
   * Refer to the
   * <a href="http://www.w3.org/Protocols/rfc2616/rfc2616-sec5.html">HTTP
   * Methods</a> documentation for further information.
   *
   */
  PP_URLREQUESTPROPERTY_METHOD,

  /**
   * This corresponds to a string (PP_VARTYPE_STRING); \n delimited.
   * Refer to the
   * <a href="http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html"Header
   * Field Definitions</a> documentaiton for further information.
   */
  PP_URLREQUESTPROPERTY_HEADERS,

  /**
   * This corresponds to a PP_Bool (PP_VARTYPE_BOOL; default=PP_FALSE).
   * Set this value to PP_TRUE if you want to download the data to a file. Use
   * PPB_URLLoader.FinishStreamingToFile() to complete the download.
   */
  PP_URLREQUESTPROPERTY_STREAMTOFILE,

  /**
   * This corresponds to a PP_Bool (PP_VARTYPE_BOOL; default=PP_TRUE).
   * Set this value to PP_FALSE if you want to use
   * PPB_URLLoader.FollowRedirects() to follow the redirects only after
   * examining redirect headers.
   */
  PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS,

  /**
   * This corresponds to a PP_Bool (PP_VARTYPE_BOOL; default=PP_FALSE).
   * Set this value to PP_TRUE if you want to be able to poll the download
   * progress using PPB_URLLoader.GetDownloadProgress().
   */
  PP_URLREQUESTPROPERTY_RECORDDOWNLOADPROGRESS,

  /**
   * This corresponds to a PP_Bool (default=PP_FALSE).
   * Set this value to PP_TRUE if you want to be able to poll the upload
   * progress using PPB_URLLoader.GetUplaodProgress().
   */
  PP_URLREQUESTPROPERTY_RECORDUPLOADPROGRESS,

  /**
   * This corresponds to a string (PP_VARTYPE_STRING) or may be undefined
   * (PP_VARTYPE_UNDEFINED; default).
   * Set it to a string to set a custom referrer (if empty, the referrer header
   * will be omitted), or to undefined to use the default referrer. Only loaders
   * with universal access (only available on trusted implementations) will
   * accept URLRequestInfo objects that try to set a custom referrer; if given
   * to a loader without universal access, PP_ERROR_BADARGUMENT will result.
   */
  PP_URLREQUESTPROPERTY_CUSTOMREFERRERURL,

  /**
   * This corresponds to a PP_Bool (PP_VARTYPE_BOOL; default=PP_FALSE).
   * Whether cross-origin requests are allowed. Cross-origin requests are made
   * using the CORS (Cross-Origin Resource Sharing) algorithm to check whether
   * the request should be allowed. For the complete CORS algorithm, refer to
   * the <a href="http://www.w3.org/TR/access-control">Cross-Origin Resource
   * Sharing</a> documentation.
   */
  PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS,

  /**
   * This corresponds to a PP_Bool (PP_VARTYPE_BOOL; default=PP_FALSE).
   * Whether HTTP credentials are sent with cross-origin requests. If false,
   * no credentials are sent with the request and cookies are ignored in the
   * response. If the request is not cross-origin, this property is ignored.
   */
  PP_URLREQUESTPROPERTY_ALLOWCREDENTIALS,

  /**
   * This corresponds to a string (PP_VARTYPE_STRING) or may be undefined
   * (PP_VARTYPE_UNDEFINED; default).
   * Set it to a string to set a custom content-transfer-encoding header (if
   * empty, that header will be omitted), or to undefined to use the default (if
   * any). Only loaders with universal access (only available on trusted
   * implementations) will accept URLRequestInfo objects that try to set a
   * custom content transfer encoding; if given to a loader without universal
   * access, PP_ERROR_BADARGUMENT will result.
   */
  PP_URLREQUESTPROPERTY_CUSTOMCONTENTTRANSFERENCODING
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

/**
 * The PPB_URLRequestInfo interface contains pointers to functions for creating
 * and handling URL requests. Refer to PPB_URLLoader for further information.
 */
struct PPB_URLRequestInfo {
  /**
   * Create is a pointer to a function that creates a new URLRequestInfo
   * object.
   *
   * @param[in] instance A PP_Instance indentifying one instance of a module.
   * @return A PP_Resource identifying the URLRequestInfo if successful, 0 if
   * the instance is invalid.
   */
  PP_Resource (*Create)(PP_Instance instance);

  /**
   * IsURLRequestInfo is a pointer to a function that determines if a resource
   * is a URLRequestInfo.
   *
   * @param[in] resource A PP_Resource corresponding to a URLRequestInfo.
   * @return PP_TRUE if the resource is a URLRequestInfo,
   * PP_FALSE if the resource is invalid or some type other than
   * URLRequestInfo.
   */
  PP_Bool (*IsURLRequestInfo)(PP_Resource resource);

  /**
   * SetProperty is a pointer to a function that sets a request property. The
   * value of the property must be the correct type according to the property
   * being set.
   *
   * @param[in] request A PP_Resource corresponding to a URLRequestInfo.
   * @param[in] property A PP_URLRequestProperty identifying the
   * property to set.
   * @param[in] value A PP_Var containing the property value.
   * @return PP_TRUE if successful, PP_FALSE if any of the
   * parameters are invalid.
   */
  PP_Bool (*SetProperty)(PP_Resource request,
                         PP_URLRequestProperty property,
                         struct PP_Var value);

  /**
   * AppendDataToBody is a pointer to a function that appends data to the
   * request body. A Content-Length request header will be automatically
   * generated.
   *
   * @param[in] request A PP_Resource corresponding to a URLRequestInfo.
   * @param[in] data A pointer to a buffer holding the data.
   * @param[in] len The length, in bytes, of the data.
   * @return PP_TRUE if successful, PP_FALSE if any of the
   * parameters are invalid.
   *
   *
   */
  PP_Bool (*AppendDataToBody)(PP_Resource request,
                              const void* data,
                              uint32_t len);

  /**
   * AppendFileToBody is a pointer to a function used to append a file, to be
   * uploaded, to the request body. A content-length request header will be
   * automatically generated.
   *
   * @param[in] request A PP_Resource corresponding to a URLRequestInfo.
   * @param[in] file_ref A PP_Resource containing the file reference.
   * @param[in] start_offset An optional starting point offset within the
   * file.
   * @param[in] number_of_bytes An optional number of bytes of the file to
   * be included. If number_of_bytes is -1, then the sub-range to upload
   * extends to the end of the file.
   * @param[in] expected_last_modified_time An optional (non-zero) last
   * modified time stamp used to validate that the file was not modified since
   * the given time before it was uploaded. The upload will fail with an error
   * code of PP_ERROR_FILECHANGED if the file has been modified since the given
   * time. If expected_last_modified_time is 0, then no validation is
   * performed.
   * @return PP_TRUE if successful, PP_FALSE if any of the
   * parameters are invalid.
   */
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

