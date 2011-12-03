// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_url_response_info_impl.h"

#include "base/logging.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/var.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLResponse.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_file_ref_impl.h"
#include "webkit/plugins/ppapi/resource_helper.h"
#include "webkit/glue/webkit_glue.h"

using ppapi::StringVar;
using ppapi::thunk::PPB_URLResponseInfo_API;
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

PPB_URLResponseInfo_Impl::PPB_URLResponseInfo_Impl(PP_Instance instance)
    : Resource(instance),
      status_code_(-1) {
}

PPB_URLResponseInfo_Impl::~PPB_URLResponseInfo_Impl() {
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
  if (!file_path.isEmpty()) {
    body_ = PPB_FileRef_Impl::CreateExternal(
        pp_instance(), webkit_glue::WebStringToFilePath(file_path));
  }
  return true;
}

PPB_URLResponseInfo_API* PPB_URLResponseInfo_Impl::AsPPB_URLResponseInfo_API() {
  return this;
}

PP_Var PPB_URLResponseInfo_Impl::GetProperty(PP_URLResponseProperty property) {
  PluginModule* plugin_module = ResourceHelper::GetPluginModule(this);
  if (!plugin_module)
    return PP_MakeUndefined();
  PP_Module pp_module = plugin_module->pp_module();
  switch (property) {
    case PP_URLRESPONSEPROPERTY_URL:
      return StringVar::StringToPPVar(pp_module, url_);
    case PP_URLRESPONSEPROPERTY_REDIRECTURL:
      if (IsRedirect(status_code_))
        return StringVar::StringToPPVar(pp_module, redirect_url_);
      break;
    case PP_URLRESPONSEPROPERTY_REDIRECTMETHOD:
      if (IsRedirect(status_code_))
        return StringVar::StringToPPVar(pp_module, status_text_);
      break;
    case PP_URLRESPONSEPROPERTY_STATUSCODE:
      return PP_MakeInt32(status_code_);
    case PP_URLRESPONSEPROPERTY_STATUSLINE:
      return StringVar::StringToPPVar(pp_module, status_text_);
    case PP_URLRESPONSEPROPERTY_HEADERS:
      return StringVar::StringToPPVar(pp_module, headers_);
  }
  // The default is to return an undefined PP_Var.
  return PP_MakeUndefined();
}

PP_Resource PPB_URLResponseInfo_Impl::GetBodyAsFileRef() {
  if (!body_.get())
    return 0;
  return body_->GetReference();
}

}  // namespace ppapi
}  // namespace webkit

