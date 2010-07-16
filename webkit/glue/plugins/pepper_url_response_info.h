// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_URL_RESPONSE_INFO_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_URL_RESPONSE_INFO_H_

#include <string>

#include "third_party/ppapi/c/ppb_url_response_info.h"
#include "webkit/glue/plugins/pepper_resource.h"

namespace pepper {

class URLResponseInfo : public Resource {
 public:
  explicit URLResponseInfo(PluginModule* module);
  virtual ~URLResponseInfo();

  // Returns a pointer to the interface implementing PPB_URLResponseInfo that
  // is exposed to the plugin.
  static const PPB_URLResponseInfo* GetInterface();

  // Resource overrides.
  URLResponseInfo* AsURLResponseInfo() { return this; }

  // PPB_URLResponseInfo implementation.
  PP_Var GetProperty(PP_URLResponseProperty property);
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_URL_RESPONSE_INFO_H_
