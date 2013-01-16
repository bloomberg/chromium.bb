// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/url_response_info_util.h"

#include "base/logging.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLResponse.h"
#include "webkit/base/file_path_string_conversions.h"
#include "webkit/plugins/ppapi/ppb_file_ref_impl.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebHTTPHeaderVisitor;
using WebKit::WebString;
using WebKit::WebURLResponse;

namespace webkit {
namespace ppapi {

namespace {

class HeaderFlattener : public WebHTTPHeaderVisitor {
 public:
  const std::string& buffer() const { return buffer_; }

  virtual void visitHeader(const WebString& name, const WebString& value) {
    if (!buffer_.empty())
      buffer_.append("\n");
    buffer_.append(name.utf8());
    buffer_.append(": ");
    buffer_.append(value.utf8());
  }

 private:
  std::string buffer_;
};

bool IsRedirect(int32_t status) {
  return status >= 300 && status <= 399;
}

}  // namespace

::ppapi::URLResponseInfoData DataFromWebURLResponse(
    PP_Instance pp_instance,
    const WebURLResponse& response) {
  ::ppapi::URLResponseInfoData data;

  data.url = response.url().spec();
  data.status_code = response.httpStatusCode();
  data.status_text = response.httpStatusText().utf8();
  if (IsRedirect(data.status_code)) {
    data.redirect_url = response.httpHeaderField(
        WebString::fromUTF8("Location")).utf8();
  }

  HeaderFlattener flattener;
  response.visitHTTPHeaderFields(&flattener);
  data.headers = flattener.buffer();

  WebString file_path = response.downloadFilePath();
  if (!file_path.isEmpty()) {
    scoped_refptr<PPB_FileRef_Impl> file_ref(
        PPB_FileRef_Impl::CreateExternal(
            pp_instance,
            webkit_base::WebStringToFilePath(file_path),
            std::string()));
    data.body_as_file_ref = file_ref->GetCreateInfo();
    file_ref->GetReference();  // The returned data has one ref for the plugin.
  }
  return data;
}

}  // namespace ppapi
}  // namespace webkit
