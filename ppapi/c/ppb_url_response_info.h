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
 * This file defines the <code>PPB_URLResponseInfo</code> API for examining URL
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
  /** This corresponds to a string (<code>PP_VARTYPE_STRING</code>). */
  PP_URLRESPONSEPROPERTY_URL,

  /** This corresponds to a string (<code>PP_VARTYPE_STRING</code>).*/
  PP_URLRESPONSEPROPERTY_REDIRECTURL,

  /** This corresponds to a string (<code>PP_VARTYPE_STRING</code>).*/
  PP_URLRESPONSEPROPERTY_REDIRECTMETHOD,

  /** This corresponds to an int32 (<code>PP_VARETYPE_INT32</code>)*/
  PP_URLRESPONSEPROPERTY_STATUSCODE,

  /** This corresponds to a string (<code>PP_VARTYPE_STRING</code>).*/
  PP_URLRESPONSEPROPERTY_STATUSLINE,

  /**
   * This corresponds to a string(<code>PP_VARTYPE_STRING</code>),
   * \n delimited
   */
  PP_URLRESPONSEPROPERTY_HEADERS
} PP_URLResponseProperty;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_URLResponseProperty, 4);
/**
 * @}
 */

#define PPB_URLRESPONSEINFO_INTERFACE_0_1 "PPB_URLResponseInfo;0.1"
#define PPB_URLRESPONSEINFO_INTERFACE PPB_URLRESPONSEINFO_INTERFACE_0_1

/**
 *
 * @addtogroup Interfaces
 * @{
 */

/**
 * The PPB_URLResponseInfo interface contains APIs for
 * examining URL responses. Refer to <code>PPB_URLLoader</code> for further
 * information.
 */
struct PPB_URLResponseInfo {

  /**
   * IsURLResponseInfo() determines if a response is a
   * <code>URLResponseInfo</code>.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to a
   * <code>URLResponseInfo</code>.
   *
   * @return <code>PP_TRUE</code> if the resource is a
   * <code>URLResponseInfo</code>, <code>PP_FALSE</code> if the resource is
   * invalid or some type other than <code>URLResponseInfo</code>.
   */
  PP_Bool (*IsURLResponseInfo)(PP_Resource resource);

  /**
   * GetProperty() gets a response property.
   *
   * @param[in] request A <code>PP_Resource</code> corresponding to a
   * <code>URLResponseInfo</code>.
   * @param[in] property A <code>PP_URLResponseProperty</code> identifying
   * the type of property in the response.
   *
   * @return A <code>PP_Var</code> containing the response property value if
   * successful, <code>PP_VARTYPE_VOID</code> if an input parameter is invalid.
   */
  struct PP_Var (*GetProperty)(PP_Resource response,
                               PP_URLResponseProperty property);

  /**
   * GetBodyAsFileRef() returns a FileRef pointing to the file containing the
   * response body.  This is only valid if
   * <code>PP_URLREQUESTPROPERTY_STREAMTOFILE</code> was set on the
   * <code>URLRequestInfo</code> used to produce this response.  This file
   * remains valid until the <code>URLLoader</code> associated with this
   * <code>URLResponseInfo is closed or destroyed.
   *
   * @param[in] request A <code>PP_Resource</code> corresponding to a
   * <code>URLResponseInfo</code>.
   *
   * @return A <code>PP_Resource</code> corresponding to a FileRef if
   * successful, 0 if <code>PP_URLREQUESTPROPERTY_STREAMTOFILE</code> was not
   * requested or if the <code>URLLoader</code> has not been opened yet.
   */
  PP_Resource (*GetBodyAsFileRef)(PP_Resource response);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_URL_RESPONSE_INFO_H_ */

