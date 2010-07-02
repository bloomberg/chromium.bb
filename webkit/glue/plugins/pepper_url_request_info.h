// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_URL_REQUEST_INFO_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_URL_REQUEST_INFO_H_

#include <string>

#include "third_party/ppapi/c/ppb_url_request_info.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLRequest.h"
#include "webkit/glue/plugins/pepper_resource.h"

namespace pepper {

class FileRef;

class URLRequestInfo : public Resource {
 public:
  explicit URLRequestInfo(PluginModule* module);
  virtual ~URLRequestInfo();

  // Returns a pointer to the interface implementing PPB_URLRequestInfo that is
  // exposed to the plugin.
  static const PPB_URLRequestInfo* GetInterface();

  // Resource overrides.
  URLRequestInfo* AsURLRequestInfo() { return this; }

  // PPB_URLRequestInfo implementation.
  bool SetBooleanProperty(PP_URLRequestProperty property, bool value);
  bool SetStringProperty(PP_URLRequestProperty property,
                         const std::string& value);
  bool AppendDataToBody(const std::string& data);
  bool AppendFileToBody(FileRef* file_ref,
                        int64_t start_offset,
                        int64_t number_of_bytes,
                        PP_Time expected_last_modified_time);

  const WebKit::WebURLRequest& web_request() const {
    return web_request_;
  }
  std::string url() const { return url_; }

 private:
  std::string url_;
  WebKit::WebURLRequest web_request_;
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_URL_REQUEST_INFO_H_
