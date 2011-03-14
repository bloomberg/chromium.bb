// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_url_request_info_impl.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_util.h"
#include "ppapi/c/pp_var.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHTTPBody.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppb_file_ref_impl.h"
#include "webkit/plugins/ppapi/string.h"
#include "webkit/plugins/ppapi/var.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebData;
using WebKit::WebHTTPBody;
using WebKit::WebString;
using WebKit::WebFrame;
using WebKit::WebURL;
using WebKit::WebURLRequest;

namespace webkit {
namespace ppapi {

namespace {

// A header string containing any of the following fields will cause
// an error. The list comes from the XMLHttpRequest standard.
// http://www.w3.org/TR/XMLHttpRequest/#the-setrequestheader-method
const char* const kForbiddenHeaderFields[] = {
  "accept-charset",
  "accept-encoding",
  "connection",
  "content-length",
  "cookie",
  "cookie2",
  "content-transfer-encoding",
  "date",
  "expect",
  "host",
  "keep-alive",
  "origin",
  "referer",
  "te",
  "trailer",
  "transfer-encoding",
  "upgrade",
  "user-agent",
  "via",
};

bool IsValidHeaderField(const std::string& name) {
  for (size_t i = 0; i < arraysize(kForbiddenHeaderFields); ++i) {
    if (LowerCaseEqualsASCII(name, kForbiddenHeaderFields[i]))
      return false;
  }
  if (StartsWithASCII(name, "proxy-", false))
    return false;
  if (StartsWithASCII(name, "sec-", false))
    return false;

  return true;
}

bool AreValidHeaders(const std::string& headers) {
  net::HttpUtil::HeadersIterator it(headers.begin(), headers.end(), "\n");
  while (it.GetNext()) {
    if (!IsValidHeaderField(it.name()))
      return false;
  }
  return true;
}

PP_Resource Create(PP_Instance instance_id) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;

  PPB_URLRequestInfo_Impl* request = new PPB_URLRequestInfo_Impl(instance);

  return request->GetReference();
}

PP_Bool IsURLRequestInfo(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<PPB_URLRequestInfo_Impl>(resource));
}

PP_Bool SetProperty(PP_Resource request_id,
                    PP_URLRequestProperty property,
                    PP_Var var) {
  scoped_refptr<PPB_URLRequestInfo_Impl> request(
      Resource::GetAs<PPB_URLRequestInfo_Impl>(request_id));
  if (!request)
    return PP_FALSE;

  PP_Bool result = PP_FALSE;
  switch (var.type) {
    case PP_VARTYPE_UNDEFINED:
      result = BoolToPPBool(request->SetUndefinedProperty(property));
      break;
    case PP_VARTYPE_BOOL:
      result = BoolToPPBool(
          request->SetBooleanProperty(property,
                                      PPBoolToBool(var.value.as_bool)));
      break;
    case PP_VARTYPE_STRING: {
      scoped_refptr<StringVar> string(StringVar::FromPPVar(var));
      if (string)
        result = BoolToPPBool(request->SetStringProperty(property,
                                                         string->value()));
      break;
    }
    default:
      break;
  }
  return result;
}

PP_Bool AppendDataToBody(PP_Resource request_id,
                         const char* data,
                         uint32_t len) {
  scoped_refptr<PPB_URLRequestInfo_Impl> request(
      Resource::GetAs<PPB_URLRequestInfo_Impl>(request_id));
  if (!request)
    return PP_FALSE;

  return BoolToPPBool(request->AppendDataToBody(std::string(data, len)));
}

PP_Bool AppendFileToBody(PP_Resource request_id,
                      PP_Resource file_ref_id,
                      int64_t start_offset,
                      int64_t number_of_bytes,
                      PP_Time expected_last_modified_time) {
  scoped_refptr<PPB_URLRequestInfo_Impl> request(
      Resource::GetAs<PPB_URLRequestInfo_Impl>(request_id));
  if (!request)
    return PP_FALSE;

  scoped_refptr<PPB_FileRef_Impl> file_ref(
      Resource::GetAs<PPB_FileRef_Impl>(file_ref_id));
  if (!file_ref)
    return PP_FALSE;

  return BoolToPPBool(request->AppendFileToBody(file_ref,
                                                start_offset,
                                                number_of_bytes,
                                                expected_last_modified_time));
}

const PPB_URLRequestInfo ppb_urlrequestinfo = {
  &Create,
  &IsURLRequestInfo,
  &SetProperty,
  &AppendDataToBody,
  &AppendFileToBody
};

}  // namespace

struct PPB_URLRequestInfo_Impl::BodyItem {
  BodyItem(const std::string& data)
      : data(data),
        start_offset(0),
        number_of_bytes(-1),
        expected_last_modified_time(0.0) {
  }

  BodyItem(PPB_FileRef_Impl* file_ref,
           int64_t start_offset,
           int64_t number_of_bytes,
           PP_Time expected_last_modified_time)
      : file_ref(file_ref),
        start_offset(start_offset),
        number_of_bytes(number_of_bytes),
        expected_last_modified_time(expected_last_modified_time) {
  }

  std::string data;
  scoped_refptr<PPB_FileRef_Impl> file_ref;
  int64_t start_offset;
  int64_t number_of_bytes;
  PP_Time expected_last_modified_time;
};

PPB_URLRequestInfo_Impl::PPB_URLRequestInfo_Impl(PluginInstance* instance)
    : Resource(instance),
      stream_to_file_(false),
      follow_redirects_(true),
      record_download_progress_(false),
      record_upload_progress_(false),
      has_custom_referrer_url_(false) {
}

PPB_URLRequestInfo_Impl::~PPB_URLRequestInfo_Impl() {
}

// static
const PPB_URLRequestInfo* PPB_URLRequestInfo_Impl::GetInterface() {
  return &ppb_urlrequestinfo;
}

PPB_URLRequestInfo_Impl* PPB_URLRequestInfo_Impl::AsPPB_URLRequestInfo_Impl() {
  return this;
}

bool PPB_URLRequestInfo_Impl::SetUndefinedProperty(
    PP_URLRequestProperty property) {
  switch (property) {
    case PP_URLREQUESTPROPERTY_CUSTOMREFERRERURL:
      has_custom_referrer_url_ = false;
      custom_referrer_url_ = std::string();
      return true;
    default:
      return false;
  }
}

bool PPB_URLRequestInfo_Impl::SetBooleanProperty(PP_URLRequestProperty property,
                                                 bool value) {
  switch (property) {
    case PP_URLREQUESTPROPERTY_STREAMTOFILE:
      stream_to_file_ = value;
      return true;
    case PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS:
      follow_redirects_ = value;
      return true;
    case PP_URLREQUESTPROPERTY_RECORDDOWNLOADPROGRESS:
      record_download_progress_ = value;
      return true;
    case PP_URLREQUESTPROPERTY_RECORDUPLOADPROGRESS:
      record_upload_progress_ = value;
      return true;
    default:
      return false;
  }
}

bool PPB_URLRequestInfo_Impl::SetStringProperty(PP_URLRequestProperty property,
                                                const std::string& value) {
  // TODO(darin): Validate input.  Perhaps at a different layer?
  switch (property) {
    case PP_URLREQUESTPROPERTY_URL:
      url_ = value;  // NOTE: This may be a relative URL.
      return true;
    case PP_URLREQUESTPROPERTY_METHOD:
      method_ = value;
      return true;
    case PP_URLREQUESTPROPERTY_HEADERS:
      if (!AreValidHeaders(value))
        return false;
      headers_ = value;
      return true;
    case PP_URLREQUESTPROPERTY_CUSTOMREFERRERURL:
      has_custom_referrer_url_ = true;
      custom_referrer_url_ = value;
      return true;
    default:
      return false;
  }
}

bool PPB_URLRequestInfo_Impl::AppendDataToBody(const std::string& data) {
  if (!data.empty())
    body_.push_back(BodyItem(data));
  return true;
}

bool PPB_URLRequestInfo_Impl::AppendFileToBody(
    PPB_FileRef_Impl* file_ref,
    int64_t start_offset,
    int64_t number_of_bytes,
    PP_Time expected_last_modified_time) {
  // Ignore a call to append nothing.
  if (number_of_bytes == 0)
    return true;

  // Check for bad values.  (-1 means read until end of file.)
  if (start_offset < 0 || number_of_bytes < -1)
    return false;

  body_.push_back(BodyItem(file_ref,
                           start_offset,
                           number_of_bytes,
                           expected_last_modified_time));
  return true;
}

WebURLRequest PPB_URLRequestInfo_Impl::ToWebURLRequest(WebFrame* frame) const {
  WebURLRequest web_request;
  web_request.initialize();
  web_request.setURL(frame->document().completeURL(WebString::fromUTF8(url_)));
  web_request.setDownloadToFile(stream_to_file_);
  web_request.setReportUploadProgress(record_upload_progress());

  if (!method_.empty())
    web_request.setHTTPMethod(WebString::fromUTF8(method_));

  if (!headers_.empty()) {
    net::HttpUtil::HeadersIterator it(headers_.begin(), headers_.end(), "\n");
    while (it.GetNext()) {
      web_request.addHTTPHeaderField(
          WebString::fromUTF8(it.name()),
          WebString::fromUTF8(it.values()));
    }
  }

  if (!body_.empty()) {
    WebHTTPBody http_body;
    http_body.initialize();
    for (size_t i = 0; i < body_.size(); ++i) {
      if (body_[i].file_ref) {
        http_body.appendFileRange(
            webkit_glue::FilePathToWebString(
                body_[i].file_ref->GetSystemPath()),
            body_[i].start_offset,
            body_[i].number_of_bytes,
            body_[i].expected_last_modified_time);
      } else {
        DCHECK(!body_[i].data.empty());
        http_body.appendData(WebData(body_[i].data));
      }
    }
    web_request.setHTTPBody(http_body);
  }

  if (has_custom_referrer_url_) {
    if (!custom_referrer_url_.empty())
      frame->setReferrerForRequest(web_request, GURL(custom_referrer_url_));
  } else {
    frame->setReferrerForRequest(web_request, WebURL());  // Use default.
  }

  return web_request;
}

bool PPB_URLRequestInfo_Impl::RequiresUniversalAccess() const {
  return has_custom_referrer_url_;
}

}  // namespace ppapi
}  // namespace webkit
