// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_url_request_info.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_util.h"
#include "third_party/ppapi/c/pp_var.h"
#include "third_party/WebKit/WebKit/chromium/public/WebData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebHTTPBody.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLRequest.h"
#include "webkit/glue/plugins/pepper_file_ref.h"
#include "webkit/glue/plugins/pepper_plugin_module.h"
#include "webkit/glue/plugins/pepper_string.h"
#include "webkit/glue/plugins/pepper_var.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebData;
using WebKit::WebHTTPBody;
using WebKit::WebString;
using WebKit::WebFrame;
using WebKit::WebURL;
using WebKit::WebURLRequest;

namespace pepper {

namespace {

// If any of these request headers are specified, they will not be sent.
// TODO(darin): Add more based on security considerations?
const char* const kIgnoredRequestHeaders[] = {
  "content-length"
};

bool IsIgnoredRequestHeader(const std::string& name) {
  for (size_t i = 0; i < arraysize(kIgnoredRequestHeaders); ++i) {
    if (LowerCaseEqualsASCII(name, kIgnoredRequestHeaders[i]))
      return true;
  }
  return false;
}

PP_Resource Create(PP_Module module_id) {
  PluginModule* module = ResourceTracker::Get()->GetModule(module_id);
  if (!module)
    return 0;

  URLRequestInfo* request = new URLRequestInfo(module);

  return request->GetReference();
}

bool IsURLRequestInfo(PP_Resource resource) {
  return !!Resource::GetAs<URLRequestInfo>(resource);
}

bool SetProperty(PP_Resource request_id,
                 PP_URLRequestProperty_Dev property,
                 PP_Var var) {
  scoped_refptr<URLRequestInfo> request(
      Resource::GetAs<URLRequestInfo>(request_id));
  if (!request)
    return false;

  if (var.type == PP_VARTYPE_BOOL)
    return request->SetBooleanProperty(property, var.value.as_bool);

  if (var.type == PP_VARTYPE_STRING) {
    scoped_refptr<StringVar> string(StringVar::FromPPVar(var));
    if (string)
      return request->SetStringProperty(property, string->value());
  }

  return false;
}

bool AppendDataToBody(PP_Resource request_id, const char* data, uint32_t len) {
  scoped_refptr<URLRequestInfo> request(
      Resource::GetAs<URLRequestInfo>(request_id));
  if (!request)
    return false;

  return request->AppendDataToBody(std::string(data, len));
}

bool AppendFileToBody(PP_Resource request_id,
                      PP_Resource file_ref_id,
                      int64_t start_offset,
                      int64_t number_of_bytes,
                      PP_Time expected_last_modified_time) {
  scoped_refptr<URLRequestInfo> request(
      Resource::GetAs<URLRequestInfo>(request_id));
  if (!request)
    return false;

  scoped_refptr<FileRef> file_ref(Resource::GetAs<FileRef>(file_ref_id));
  if (!file_ref)
    return false;

  return request->AppendFileToBody(file_ref,
                                   start_offset,
                                   number_of_bytes,
                                   expected_last_modified_time);
}

const PPB_URLRequestInfo_Dev ppb_urlrequestinfo = {
  &Create,
  &IsURLRequestInfo,
  &SetProperty,
  &AppendDataToBody,
  &AppendFileToBody
};

}  // namespace

struct URLRequestInfo::BodyItem {
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

URLRequestInfo::URLRequestInfo(PluginModule* module)
    : Resource(module),
      stream_to_file_(false) {
}

URLRequestInfo::~URLRequestInfo() {
}

// static
const PPB_URLRequestInfo_Dev* URLRequestInfo::GetInterface() {
  return &ppb_urlrequestinfo;
}

bool URLRequestInfo::SetBooleanProperty(PP_URLRequestProperty_Dev property,
                                        bool value) {
  switch (property) {
    case PP_URLREQUESTPROPERTY_STREAMTOFILE:
      stream_to_file_ = value;
      return true;
    default:
      NOTIMPLEMENTED();  // TODO(darin): Implement me!
      return false;
  }
}

bool URLRequestInfo::SetStringProperty(PP_URLRequestProperty_Dev property,
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
      headers_ = value;
      return true;
    default:
      return false;
  }
}

bool URLRequestInfo::AppendDataToBody(const std::string& data) {
  if (!data.empty())
    body_.push_back(BodyItem(data));
  return true;
}

bool URLRequestInfo::AppendFileToBody(FileRef* file_ref,
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

WebURLRequest URLRequestInfo::ToWebURLRequest(WebFrame* frame) const {
  WebURLRequest web_request;
  web_request.initialize();
  web_request.setURL(frame->document().completeURL(WebString::fromUTF8(url_)));
  web_request.setDownloadToFile(stream_to_file_);

  if (!method_.empty())
    web_request.setHTTPMethod(WebString::fromUTF8(method_));

  if (!headers_.empty()) {
    net::HttpUtil::HeadersIterator it(headers_.begin(), headers_.end(), "\n");
    while (it.GetNext()) {
      if (!IsIgnoredRequestHeader(it.name())) {
        web_request.addHTTPHeaderField(
            WebString::fromUTF8(it.name()),
            WebString::fromUTF8(it.values()));
      }
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

  frame->setReferrerForRequest(web_request, WebURL());  // Use default.
  return web_request;
}

}  // namespace pepper
