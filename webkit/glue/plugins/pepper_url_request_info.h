// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_URL_REQUEST_INFO_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_URL_REQUEST_INFO_H_

#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "third_party/ppapi/c/ppb_url_request_info.h"
#include "webkit/glue/plugins/pepper_file_ref.h"
#include "webkit/glue/plugins/pepper_resource.h"

namespace WebKit {
class WebFrame;
class WebURLRequest;
}

namespace pepper {

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

  WebKit::WebURLRequest ToWebURLRequest(WebKit::WebFrame* frame) const;

 private:
  struct BodyItem {
    BodyItem(const std::string& data)
        : data(data),
          start_offset(0),
          number_of_bytes(-1),
          expected_last_modified_time(0.0) {
    }

    BodyItem(FileRef* file_ref,
             int64_t start_offset,
             int64_t number_of_bytes,
             PP_Time expected_last_modified_time)
        : file_ref(file_ref),
          start_offset(start_offset),
          number_of_bytes(number_of_bytes),
          expected_last_modified_time(expected_last_modified_time) {
    }

    std::string data;
    scoped_refptr<FileRef> file_ref;
    int64_t start_offset;
    int64_t number_of_bytes;
    PP_Time expected_last_modified_time;
  };

  typedef std::vector<BodyItem> Body;

  std::string url_;
  std::string method_;
  std::string headers_;
  Body body_;
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_URL_REQUEST_INFO_H_
