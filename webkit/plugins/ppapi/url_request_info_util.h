// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_URL_REQUEST_INFO_UTIL_H_
#define WEBKIT_PLUGINS_PPAPI_URL_REQUEST_INFO_UTIL_H_

#include "base/memory/ref_counted.h"
#include "webkit/plugins/webkit_plugins_export.h"

namespace ppapi {
struct URLRequestInfoData;
}

namespace WebKit {
class WebFrame;
class WebURLRequest;
}

namespace webkit {
namespace ppapi {

// Creates the WebKit URL request from the current request info. Returns true
// on success, false if the request is invalid (in which case *dest may be
// partially initialized). Any upload files with only resource IDs (no file ref
// pointers) will be populated by this function on success.
WEBKIT_PLUGINS_EXPORT bool CreateWebURLRequest(
    ::ppapi::URLRequestInfoData* data,
    WebKit::WebFrame* frame,
    WebKit::WebURLRequest* dest);

// Returns true if universal access is required to use the given request.
WEBKIT_PLUGINS_EXPORT bool URLRequestRequiresUniversalAccess(
    const ::ppapi::URLRequestInfoData& data);

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_URL_REQUEST_INFO_UTIL_H_
