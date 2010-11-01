// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_URL_RESPONSE_INFO_DEV_H_
#define PPAPI_CPP_DEV_URL_RESPONSE_INFO_DEV_H_

#include "ppapi/c/dev/ppb_url_response_info_dev.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/var.h"

namespace pp {

class FileRef_Dev;

class URLResponseInfo_Dev : public Resource {
 public:
  // Creates an is_null() URLResponseInfo object.
  URLResponseInfo_Dev() {}

  // This constructor is used when we've gotten a PP_Resource as a return value
  // that has already been addref'ed for us.
  struct PassRef {};
  URLResponseInfo_Dev(PassRef, PP_Resource resource);

  URLResponseInfo_Dev(const URLResponseInfo_Dev& other);

  URLResponseInfo_Dev& operator=(const URLResponseInfo_Dev& other);
  void swap(URLResponseInfo_Dev& other);

  // PPB_URLResponseInfo methods:
  Var GetProperty(PP_URLResponseProperty_Dev property) const;
  FileRef_Dev GetBody() const;

  // Convenient helpers for getting properties:
  Var GetURL() const {
    return GetProperty(PP_URLRESPONSEPROPERTY_URL);
  }
  Var GetRedirectURL() const {
    return GetProperty(PP_URLRESPONSEPROPERTY_REDIRECTURL);
  }
  Var GetRedirectMethod() const {
    return GetProperty(PP_URLRESPONSEPROPERTY_REDIRECTMETHOD);
  }
  int32_t GetStatusCode() const {
    return GetProperty(PP_URLRESPONSEPROPERTY_STATUSCODE).AsInt();
  }
  Var GetStatusLine() const {
    return GetProperty(PP_URLRESPONSEPROPERTY_STATUSLINE);
  }
  Var GetHeaders() const {
    return GetProperty(PP_URLRESPONSEPROPERTY_HEADERS);
  }
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_URL_RESPONSE_INFO_DEV_H_
