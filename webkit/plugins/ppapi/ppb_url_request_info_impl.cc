// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_url_request_info_impl.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_util.h"
#include "net/http/http_util.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_file_ref_api.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHTTPBody.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_file_ref_impl.h"
#include "webkit/plugins/ppapi/ppb_file_system_impl.h"
#include "webkit/plugins/ppapi/resource_helper.h"
#include "webkit/plugins/ppapi/string.h"

using ppapi::StringVar;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_FileRef_API;
using ppapi::thunk::PPB_URLRequestInfo_API;
using WebKit::WebData;
using WebKit::WebHTTPBody;
using WebKit::WebString;
using WebKit::WebFrame;
using WebKit::WebURL;
using WebKit::WebURLRequest;

namespace webkit {
namespace ppapi {

namespace {

const int32_t kDefaultPrefetchBufferUpperThreshold = 100 * 1000 * 1000;
const int32_t kDefaultPrefetchBufferLowerThreshold = 50 * 1000 * 1000;

bool IsValidToken(const std::string& token) {
  size_t length = token.size();
  if (length == 0)
    return false;

  for (size_t i = 0; i < length; i++) {
    char c = token[i];
    if (c >= 127 || c <= 32)
      return false;
    if (c == '(' || c == ')' || c == '<' || c == '>' || c == '@' ||
        c == ',' || c == ';' || c == ':' || c == '\\' || c == '\"' ||
        c == '/' || c == '[' || c == ']' || c == '?' || c == '=' ||
        c == '{' || c == '}')
      return false;
  }
  return true;
}

// These methods are not allowed by the XMLHttpRequest standard.
// http://www.w3.org/TR/XMLHttpRequest/#the-open-method
const char* const kForbiddenHttpMethods[] = {
  "connect",
  "trace",
  "track",
};

// These are the "known" methods in the Webkit XHR implementation. Also see
// the XMLHttpRequest standard.
// http://www.w3.org/TR/XMLHttpRequest/#the-open-method
const char* const kKnownHttpMethods[] = {
  "get",
  "post",
  "put",
  "head",
  "copy",
  "delete",
  "index",
  "lock",
  "m-post",
  "mkcol",
  "move",
  "options",
  "propfind",
  "proppatch",
  "unlock",
};

std::string ValidateMethod(const std::string& method) {
  for (size_t i = 0; i < arraysize(kForbiddenHttpMethods); ++i) {
    if (LowerCaseEqualsASCII(method, kForbiddenHttpMethods[i]))
      return std::string();
  }
  for (size_t i = 0; i < arraysize(kKnownHttpMethods); ++i) {
    if (LowerCaseEqualsASCII(method, kKnownHttpMethods[i])) {
      // Convert the method name to upper case to match Webkit and Firefox's
      // XHR implementation.
      return StringToUpperASCII(std::string(kKnownHttpMethods[i]));
    }
  }
  // Pass through unknown methods that are not forbidden.
  return method;
}

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

}  // namespace

struct PPB_URLRequestInfo_Impl::BodyItem {
  explicit BodyItem(const std::string& data)
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

PPB_URLRequestInfo_Impl::PPB_URLRequestInfo_Impl(PP_Instance instance)
    : Resource(instance),
      stream_to_file_(false),
      follow_redirects_(true),
      record_download_progress_(false),
      record_upload_progress_(false),
      has_custom_referrer_url_(false),
      allow_cross_origin_requests_(false),
      allow_credentials_(false),
      has_custom_content_transfer_encoding_(false),
      prefetch_buffer_upper_threshold_(kDefaultPrefetchBufferUpperThreshold),
      prefetch_buffer_lower_threshold_(kDefaultPrefetchBufferLowerThreshold) {
}

PPB_URLRequestInfo_Impl::~PPB_URLRequestInfo_Impl() {
}

PPB_URLRequestInfo_API* PPB_URLRequestInfo_Impl::AsPPB_URLRequestInfo_API() {
  return this;
}

PP_Bool PPB_URLRequestInfo_Impl::SetProperty(PP_URLRequestProperty property,
                                             PP_Var var) {
  PP_Bool result = PP_FALSE;
  switch (var.type) {
    case PP_VARTYPE_UNDEFINED:
      result = PP_FromBool(SetUndefinedProperty(property));
      break;
    case PP_VARTYPE_BOOL:
      result = PP_FromBool(
          SetBooleanProperty(property, PP_ToBool(var.value.as_bool)));
      break;
    case PP_VARTYPE_INT32:
      result = PP_FromBool(
          SetIntegerProperty(property, var.value.as_int));
      break;
    case PP_VARTYPE_STRING: {
      StringVar* string = StringVar::FromPPVar(var);
      if (string)
        result = PP_FromBool(SetStringProperty(property, string->value()));
      break;
    }
    default:
      break;
  }
  return result;
}

PP_Bool PPB_URLRequestInfo_Impl::AppendDataToBody(const void* data,
                                                  uint32_t len) {
  if (len > 0)
    body_.push_back(BodyItem(std::string(static_cast<const char*>(data), len)));
  return PP_TRUE;
}

PP_Bool PPB_URLRequestInfo_Impl::AppendFileToBody(
    PP_Resource file_ref,
    int64_t start_offset,
    int64_t number_of_bytes,
    PP_Time expected_last_modified_time) {
  // Ignore a call to append nothing.
  if (number_of_bytes == 0)
    return PP_TRUE;

  // Check for bad values.  (-1 means read until end of file.)
  if (start_offset < 0 || number_of_bytes < -1)
    return PP_FALSE;

  EnterResourceNoLock<PPB_FileRef_API> enter(file_ref, true);
  if (enter.failed())
    return PP_FALSE;

  body_.push_back(BodyItem(static_cast<PPB_FileRef_Impl*>(enter.object()),
                           start_offset,
                           number_of_bytes,
                           expected_last_modified_time));
  return PP_TRUE;
}

WebURLRequest PPB_URLRequestInfo_Impl::ToWebURLRequest(WebFrame* frame) const {
  WebURLRequest web_request;
  web_request.initialize();
  web_request.setURL(frame->document().completeURL(WebString::fromUTF8(url_)));
  web_request.setDownloadToFile(stream_to_file_);
  web_request.setReportUploadProgress(record_upload_progress());

  if (!method_.empty())
    web_request.setHTTPMethod(WebString::fromUTF8(method_));

  web_request.setFirstPartyForCookies(
      frame->document().firstPartyForCookies());

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
        FilePath platform_path;
        switch (body_[i].file_ref->file_system()->type()) {
          case PP_FILESYSTEMTYPE_LOCALTEMPORARY:
          case PP_FILESYSTEMTYPE_LOCALPERSISTENT: {
            // TODO(kinuko): remove this sync IPC when we add more generic
            // AppendURLRange solution that works for both Blob/FileSystem URL.
            PluginDelegate* plugin_delegate =
                ResourceHelper::GetPluginDelegate(this);
            if (plugin_delegate) {
              plugin_delegate->SyncGetFileSystemPlatformPath(
                    body_[i].file_ref->GetFileSystemURL(), &platform_path);
            }
            break;
          }
          case PP_FILESYSTEMTYPE_EXTERNAL:
            platform_path = body_[i].file_ref->GetSystemPath();
            break;
          default:
            NOTREACHED();
        }
        http_body.appendFileRange(
            webkit_glue::FilePathToWebString(platform_path),
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
  } else if (!allow_cross_origin_requests_) {
    // Use default, except for cross-origin requests, since 'referer' is not
    // whitelisted and will cause the request to fail.
    frame->setReferrerForRequest(web_request, WebURL());
  }

  if (has_custom_content_transfer_encoding_) {
    if (!custom_content_transfer_encoding_.empty()) {
      web_request.addHTTPHeaderField(
          WebString::fromUTF8("Content-Transfer-Encoding"),
          WebString::fromUTF8(custom_content_transfer_encoding_));
    }
  }

  return web_request;
}

bool PPB_URLRequestInfo_Impl::RequiresUniversalAccess() const {
  return
      has_custom_referrer_url_ ||
      has_custom_content_transfer_encoding_ ||
      url_util::FindAndCompareScheme(url_, "javascript", NULL);
}

bool PPB_URLRequestInfo_Impl::SetUndefinedProperty(
    PP_URLRequestProperty property) {
  switch (property) {
    case PP_URLREQUESTPROPERTY_CUSTOMREFERRERURL:
      has_custom_referrer_url_ = false;
      custom_referrer_url_ = std::string();
      return true;
    case PP_URLREQUESTPROPERTY_CUSTOMCONTENTTRANSFERENCODING:
      has_custom_content_transfer_encoding_ = false;
      custom_content_transfer_encoding_ = std::string();
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
    case PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS:
      allow_cross_origin_requests_ = value;
      return true;
    case PP_URLREQUESTPROPERTY_ALLOWCREDENTIALS:
      allow_credentials_ = value;
      return true;
    default:
      return false;
  }
}

bool PPB_URLRequestInfo_Impl::SetIntegerProperty(PP_URLRequestProperty property,
                                                 int32_t value) {
  switch (property) {
    case PP_URLREQUESTPROPERTY_PREFETCHBUFFERUPPERTHRESHOLD:
      prefetch_buffer_upper_threshold_ = value;
      return true;
    case PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERTHRESHOLD:
      prefetch_buffer_lower_threshold_ = value;
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
      if (!IsValidToken(value))
        return false;
      method_ = ValidateMethod(value);
      return !method_.empty();
    case PP_URLREQUESTPROPERTY_HEADERS:
      if (!AreValidHeaders(value))
        return false;
      headers_ = value;
      return true;
    case PP_URLREQUESTPROPERTY_CUSTOMREFERRERURL:
      has_custom_referrer_url_ = true;
      custom_referrer_url_ = value;
      return true;
    case PP_URLREQUESTPROPERTY_CUSTOMCONTENTTRANSFERENCODING:
      has_custom_content_transfer_encoding_ = true;
      custom_content_transfer_encoding_ = value;
      return true;
    default:
      return false;
  }
}

}  // namespace ppapi
}  // namespace webkit
