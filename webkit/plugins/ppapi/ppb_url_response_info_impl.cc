// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_url_response_info_impl.h"

#include "base/logging.h"
#include "ppapi/c/pp_var.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLResponse.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_file_ref_impl.h"
#include "webkit/plugins/ppapi/var.h"
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

PP_Bool IsURLResponseInfo(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<PPB_URLResponseInfo_Impl>(resource));
}

PP_Var GetProperty(PP_Resource response_id,
                   PP_URLResponseProperty property) {
  scoped_refptr<PPB_URLResponseInfo_Impl> response(
      Resource::GetAs<PPB_URLResponseInfo_Impl>(response_id));
  if (!response)
    return PP_MakeUndefined();

  return response->GetProperty(property);
}

PP_Resource GetBody(PP_Resource response_id) {
  scoped_refptr<PPB_URLResponseInfo_Impl> response(
      Resource::GetAs<PPB_URLResponseInfo_Impl>(response_id));
  if (!response.get())
    return 0;

  PPB_FileRef_Impl* body = response->body();
  if (!body)
    return 0;
  body->AddRef();  // AddRef for the caller.

  return body->GetReference();
}

const PPB_URLResponseInfo ppb_urlresponseinfo = {
  &IsURLResponseInfo,
  &GetProperty,
  &GetBody
};

bool IsRedirect(int32_t status) {
  return status >= 300 && status <= 399;
}

}  // namespace

PPB_URLResponseInfo_Impl::PPB_URLResponseInfo_Impl(PluginInstance* instance)
    : Resource(instance),
      status_code_(-1) {
}

PPB_URLResponseInfo_Impl::~PPB_URLResponseInfo_Impl() {
}

// static
const PPB_URLResponseInfo* PPB_URLResponseInfo_Impl::GetInterface() {
  return &ppb_urlresponseinfo;
}

PPB_URLResponseInfo_Impl*
PPB_URLResponseInfo_Impl::AsPPB_URLResponseInfo_Impl() {
  return this;
}

PP_Var PPB_URLResponseInfo_Impl::GetProperty(PP_URLResponseProperty property) {
  switch (property) {
    case PP_URLRESPONSEPROPERTY_URL:
      return StringVar::StringToPPVar(instance()->module(), url_);
    case PP_URLRESPONSEPROPERTY_REDIRECTURL:
      if (IsRedirect(status_code_))
        return StringVar::StringToPPVar(instance()->module(), redirect_url_);
      break;
    case PP_URLRESPONSEPROPERTY_REDIRECTMETHOD:
      if (IsRedirect(status_code_))
        return StringVar::StringToPPVar(instance()->module(), status_text_);
      break;
    case PP_URLRESPONSEPROPERTY_STATUSCODE:
      return PP_MakeInt32(status_code_);
    case PP_URLRESPONSEPROPERTY_STATUSLINE:
      return StringVar::StringToPPVar(instance()->module(), status_text_);
    case PP_URLRESPONSEPROPERTY_HEADERS:
      return StringVar::StringToPPVar(instance()->module(), headers_);
  }
  // The default is to return an undefined PP_Var.
  return PP_MakeUndefined();
}

bool PPB_URLResponseInfo_Impl::Initialize(const WebURLResponse& response) {
  url_ = response.url().spec();
  status_code_ = response.httpStatusCode();
  status_text_ = response.httpStatusText().utf8();
  if (IsRedirect(status_code_)) {
    redirect_url_ = response.httpHeaderField(
        WebString::fromUTF8("Location")).utf8();
  }

  HeaderFlattener flattener;
  response.visitHTTPHeaderFields(&flattener);
  headers_ = flattener.buffer();

  WebString file_path = response.downloadFilePath();
  if (!file_path.isEmpty())
    body_ = new PPB_FileRef_Impl(instance(),
                                 webkit_glue::WebStringToFilePath(file_path));
  return true;
}

}  // namespace ppapi
}  // namespace webkit

