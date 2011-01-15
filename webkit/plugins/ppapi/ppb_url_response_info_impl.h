// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_URL_RESPONSE_INFO_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_URL_RESPONSE_INFO_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "ppapi/c/ppb_url_response_info.h"
#include "webkit/plugins/ppapi/resource.h"

namespace WebKit {
class WebURLResponse;
}

namespace webkit {
namespace ppapi {

class PPB_FileRef_Impl;

class PPB_URLResponseInfo_Impl : public Resource {
 public:
  explicit PPB_URLResponseInfo_Impl(PluginInstance* instance);
  virtual ~PPB_URLResponseInfo_Impl();

  // Returns a pointer to the interface implementing PPB_URLResponseInfo that
  // is exposed to the plugin.
  static const PPB_URLResponseInfo* GetInterface();

  // Resource overrides.
  virtual PPB_URLResponseInfo_Impl* AsPPB_URLResponseInfo_Impl();

  // PPB_URLResponseInfo implementation.
  PP_Var GetProperty(PP_URLResponseProperty property);

  bool Initialize(const WebKit::WebURLResponse& response);

  PPB_FileRef_Impl* body() { return body_; }

  std::string redirect_url() { return redirect_url_; }

 private:
  std::string url_;
  std::string headers_;
  int32_t status_code_;
  std::string status_text_;
  std::string redirect_url_;
  scoped_refptr<PPB_FileRef_Impl> body_;

  DISALLOW_COPY_AND_ASSIGN(PPB_URLResponseInfo_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_URL_RESPONSE_INFO_IMPL_H_
