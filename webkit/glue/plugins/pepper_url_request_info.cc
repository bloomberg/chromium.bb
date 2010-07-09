// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_url_request_info.h"

#include "base/logging.h"
#include "googleurl/src/gurl.h"
#include "third_party/ppapi/c/pp_var.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "webkit/glue/plugins/pepper_file_ref.h"
#include "webkit/glue/plugins/pepper_plugin_module.h"
#include "webkit/glue/plugins/pepper_string.h"
#include "webkit/glue/plugins/pepper_var.h"

using WebKit::WebString;

namespace pepper {

namespace {

PP_Resource Create(PP_Module module_id) {
  PluginModule* module = PluginModule::FromPPModule(module_id);
  if (!module)
    return 0;

  URLRequestInfo* request = new URLRequestInfo(module);
  request->AddRef();  // AddRef for the caller.

  return request->GetResource();
}

bool IsURLRequestInfo(PP_Resource resource) {
  return !!Resource::GetAs<URLRequestInfo>(resource).get();
}

bool SetProperty(PP_Resource request_id,
                 PP_URLRequestProperty property,
                 PP_Var var) {
  scoped_refptr<URLRequestInfo> request(
      Resource::GetAs<URLRequestInfo>(request_id));
  if (!request.get())
    return false;

  if (var.type == PP_VARTYPE_BOOL)
    return request->SetBooleanProperty(property, var.value.as_bool);

  if (var.type == PP_VARTYPE_STRING)
    return request->SetStringProperty(property, GetString(var)->value());

  return false;
}

bool AppendDataToBody(PP_Resource request_id, PP_Var var) {
  scoped_refptr<URLRequestInfo> request(
      Resource::GetAs<URLRequestInfo>(request_id));
  if (!request.get())
    return false;

  String* data = GetString(var);
  if (!data)
    return false;

  return request->AppendDataToBody(data->value());
}

bool AppendFileToBody(PP_Resource request_id,
                      PP_Resource file_ref_id,
                      int64_t start_offset,
                      int64_t number_of_bytes,
                      PP_Time expected_last_modified_time) {
  scoped_refptr<URLRequestInfo> request(
      Resource::GetAs<URLRequestInfo>(request_id));
  if (!request.get())
    return false;

  scoped_refptr<FileRef> file_ref(Resource::GetAs<FileRef>(file_ref_id));
  if (!file_ref.get())
    return false;

  return request->AppendFileToBody(file_ref,
                                   start_offset,
                                   number_of_bytes,
                                   expected_last_modified_time);
}

const PPB_URLRequestInfo ppb_urlrequestinfo = {
  &Create,
  &IsURLRequestInfo,
  &SetProperty,
  &AppendDataToBody,
  &AppendFileToBody
};

}  // namespace

URLRequestInfo::URLRequestInfo(PluginModule* module)
    : Resource(module) {
  web_request_.initialize();
}

URLRequestInfo::~URLRequestInfo() {
}

// static
const PPB_URLRequestInfo* URLRequestInfo::GetInterface() {
  return &ppb_urlrequestinfo;
}

bool URLRequestInfo::SetBooleanProperty(PP_URLRequestProperty property,
                                        bool value) {
  NOTIMPLEMENTED();  // TODO(darin): Implement me!
  return false;
}

bool URLRequestInfo::SetStringProperty(PP_URLRequestProperty property,
                                       const std::string& value) {
  // TODO(darin): Validate input.  Perhaps at a different layer?
  switch (property) {
    case PP_URLREQUESTPROPERTY_URL:
      // Keep the url in a string instead of a URL object because it might not
      // be complete yet.
      url_ = value;
      return true;
    case PP_URLREQUESTPROPERTY_METHOD:
      web_request_.setHTTPMethod(WebString::fromUTF8(value));
      return true;
    case PP_URLREQUESTPROPERTY_HEADERS:
      // TODO(darin): Support extra request headers
      NOTIMPLEMENTED();
      return false;
    default:
      return false;
  }
}

bool URLRequestInfo::AppendDataToBody(const std::string& data) {
  NOTIMPLEMENTED();  // TODO(darin): Implement me!
  return false;
}

bool URLRequestInfo::AppendFileToBody(FileRef* file_ref,
                                      int64_t start_offset,
                                      int64_t number_of_bytes,
                                      PP_Time expected_last_modified_time) {
  NOTIMPLEMENTED();  // TODO(darin): Implement me!
  return false;
}

}  // namespace pepper
