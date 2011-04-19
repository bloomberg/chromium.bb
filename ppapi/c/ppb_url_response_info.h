/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PPB_URL_RESPONSE_INFO_H_
#define PPAPI_C_PPB_URL_RESPONSE_INFO_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"

/**
 * @file
 * This file defines the PPB_URLResponseInfo API for examining URL
 * responses.
 */

/**
 *
 * @addtogroup Enums
 * @{
 */

/**
 * This enumeration contains properties set on a URL response.
 */
// TODO(viettrungluu) Explain each property in more detail (e.g. note how the
// full URL in the response corresponds to the relative URL in the original
// request).
typedef enum {
  /** This corresponds to a string (PP_VARTYPE_STRING). */
  PP_URLRESPONSEPROPERTY_URL,

  /** This corresponds to a string (PP_VARTYPE_STRING).*/
  PP_URLRESPONSEPROPERTY_REDIRECTURL,

  /** This corresponds to a string (PP_VARTYPE_STRING).*/
  PP_URLRESPONSEPROPERTY_REDIRECTMETHOD,

  /** This corresponds to an int32 (PP_VARETYPE_INT32)*/
  PP_URLRESPONSEPROPERTY_STATUSCODE,

  /** This corresponds to a string (PP_VARTYPE_STRING).*/
  PP_URLRESPONSEPROPERTY_STATUSLINE,

  /** This corresponds to a string(PP_VARTYPE_STRING), \n delimited */
  PP_URLRESPONSEPROPERTY_HEADERS
} PP_URLResponseProperty;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_URLResponseProperty, 4);
/**
 * @}
 */

#define PPB_URLRESPONSEINFO_INTERFACE "PPB_URLResponseInfo;0.1"

/**
 *
 * @addtogroup Interfaces
 * @{
 */

/**
 * The PPB_URLResponseInfo interface contains pointers to functions for
 * examining URL responses. Refer to PPB_URLLoader for further
 * information.
 */
struct PPB_URLResponseInfo {

  /**
   * IsURLResponseInfo is a pointer to a function that determines if a
   * response is a URLResponseInfo.
   *
   * @param[in] resource A PP_Resource corresponding to a URLResponseInfo.
   * @return PP_TRUE if the resource is a URLResponseInfo,
   * PP_FALSE if the resource is invalid or some type other than
   * URLResponseInfo.
   */
  PP_Bool (*IsURLResponseInfo)(PP_Resource resource);

  /**
   * GetProperty is a pointer to a function that gets a response property.
   *
   * @param[in] request A PP_Resource corresponding to a URLResponseInfo.
   * @param[in] property A PP_URLResponseProperty identifying the type of
   * property in the response.
   * @return A PP_Var containing the response property value if successful,
   * PP_VARTYPE_VOID if an input parameter is invalid.
   */
  struct PP_Var (*GetProperty)(PP_Resource response,
                               PP_URLResponseProperty property);

  /**
   * GetBodyAsFileRef is a pointer to a function that returns a FileRef
   * pointing to the file containing the response body.  This
   * is only valid if PP_URLREQUESTPROPERTY_STREAMTOFILE was set on the
   * URLRequestInfo used to produce this response.  This file remains valid
   * until the URLLoader associated with this URLResponseInfo is closed or
   * destroyed.
   *
   * @param[in] request A PP_Resource corresponding to a URLResponseInfo.
   * @return A PP_Resource corresponding to a FileRef if successful, 0 if
   * PP_URLREQUESTPROPERTY_STREAMTOFILE was not requested or if the URLLoader
   * has not been opened yet.
   */
  PP_Resource (*GetBodyAsFileRef)(PP_Resource response);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_URL_RESPONSE_INFO_H_ */

