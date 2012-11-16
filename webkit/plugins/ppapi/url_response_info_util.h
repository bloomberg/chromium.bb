// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_URL_RESPONSE_INFO_UTIL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_URL_RESPONSE_INFO_UTIL_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/shared_impl/url_response_info_data.h"

namespace WebKit {
class WebURLResponse;
}

namespace webkit {
namespace ppapi {

// The returned object will have one plugin reference to the "body_as_file_ref"
// if it's non-null. It's expected that the result of this function will be
// passed to the plugin.
::ppapi::URLResponseInfoData DataFromWebURLResponse(
    PP_Instance pp_instance,
    const WebKit::WebURLResponse& response);

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_URL_RESPONSE_INFO_UTIL_H_
