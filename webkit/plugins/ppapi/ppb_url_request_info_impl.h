// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_URL_REQUEST_INFO_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_URL_REQUEST_INFO_IMPL_H_

#include "base/memory/ref_counted.h"
#include "ppapi/thunk/ppb_url_request_info_api.h"
#include "ppapi/shared_impl/url_request_info_impl.h"
#include "webkit/plugins/webkit_plugins_export.h"

namespace WebKit {
class WebFrame;
class WebHTTPBody;
class WebURLRequest;
}

namespace webkit {
namespace ppapi {

class WEBKIT_PLUGINS_EXPORT PPB_URLRequestInfo_Impl :
    public ::ppapi::URLRequestInfoImpl {
 public:
  explicit PPB_URLRequestInfo_Impl(
      PP_Instance instance,
      const ::ppapi::PPB_URLRequestInfo_Data& data);
  virtual ~PPB_URLRequestInfo_Impl();

  // Creates the WebKit URL request from the current request info. Returns
  // true on success, false if the request is invalid (in which case *dest may
  // be partially initialized).
  bool ToWebURLRequest(WebKit::WebFrame* frame,
                       WebKit::WebURLRequest* dest);

  // Whether universal access is required to use this request.
  bool RequiresUniversalAccess() const;

 private:
  friend class URLRequestInfoTest;

  // Checks that the request data is valid. Returns false on failure. Note that
  // method and header validation is done by the URL loader when the request is
  // opened, and any access errors are returned asynchronously.
  bool ValidateData();

  // Appends the file ref given the Resource pointer associated with it to the
  // given HTTP body, returning true on success.
  bool AppendFileRefToBody(::ppapi::Resource* file_ref_resource,
                           int64_t start_offset,
                           int64_t number_of_bytes,
                           PP_Time expected_last_modified_time,
                           WebKit::WebHTTPBody *http_body);

  DISALLOW_COPY_AND_ASSIGN(PPB_URLRequestInfo_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_URL_REQUEST_INFO_IMPL_H_
