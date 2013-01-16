// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/url_request_info_util.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_util.h"
#include "net/http/http_util.h"
#include "ppapi/shared_impl/url_request_info_data.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebData.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebHTTPBody.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "webkit/base/file_path_string_conversions.h"
#include "webkit/glue/weburlrequest_extradata_impl.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppb_file_ref_impl.h"
#include "webkit/plugins/ppapi/ppb_file_system_impl.h"
#include "webkit/plugins/ppapi/resource_helper.h"

using ppapi::URLRequestInfoData;
using ppapi::Resource;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_FileRef_API;
using WebKit::WebData;
using WebKit::WebHTTPBody;
using WebKit::WebString;
using WebKit::WebFrame;
using WebKit::WebURL;
using WebKit::WebURLRequest;

namespace webkit {
namespace ppapi {

namespace {

// Appends the file ref given the Resource pointer associated with it to the
// given HTTP body, returning true on success.
bool AppendFileRefToBody(
    Resource* file_ref_resource,
    int64_t start_offset,
    int64_t number_of_bytes,
    PP_Time expected_last_modified_time,
    WebHTTPBody *http_body) {
  // Get the underlying file ref impl.
  if (!file_ref_resource)
    return false;
  PPB_FileRef_API* file_ref_api = file_ref_resource->AsPPB_FileRef_API();
  if (!file_ref_api)
    return false;
  const PPB_FileRef_Impl* file_ref =
      static_cast<PPB_FileRef_Impl*>(file_ref_api);

  PluginDelegate* plugin_delegate =
      ResourceHelper::GetPluginDelegate(file_ref_resource);
  if (!plugin_delegate)
    return false;

  FilePath platform_path;
  switch (file_ref->GetFileSystemType()) {
    case PP_FILESYSTEMTYPE_LOCALTEMPORARY:
    case PP_FILESYSTEMTYPE_LOCALPERSISTENT:
      // TODO(kinuko): remove this sync IPC when we fully support
      // AppendURLRange for FileSystem URL.
      plugin_delegate->SyncGetFileSystemPlatformPath(
          file_ref->GetFileSystemURL(), &platform_path);
      break;
    case PP_FILESYSTEMTYPE_EXTERNAL:
      platform_path = file_ref->GetSystemPath();
      break;
    default:
      NOTREACHED();
  }
  http_body->appendFileRange(
      webkit_base::FilePathToWebString(platform_path),
      start_offset,
      number_of_bytes,
      expected_last_modified_time);
  return true;
}

// Checks that the request data is valid. Returns false on failure. Note that
// method and header validation is done by the URL loader when the request is
// opened, and any access errors are returned asynchronously.
bool ValidateURLRequestData(const ::ppapi::URLRequestInfoData& data) {
  if (data.prefetch_buffer_lower_threshold < 0 ||
      data.prefetch_buffer_upper_threshold < 0 ||
      data.prefetch_buffer_upper_threshold <=
      data.prefetch_buffer_lower_threshold) {
    return false;
  }
  return true;
}

// Ensures that the file_ref members of the given request info data are
// populated from the resource IDs. Returns true on success.
bool EnsureFileRefObjectsPopulated(::ppapi::URLRequestInfoData* data) {
  // Get the Resource objects for any file refs with only host resource (this
  // is the state of the request as it comes off IPC).
  for (size_t i = 0; i < data->body.size(); ++i) {
    URLRequestInfoData::BodyItem& item = data->body[i];
    if (item.is_file && !item.file_ref) {
      EnterResourceNoLock<PPB_FileRef_API> enter(
          item.file_ref_host_resource.host_resource(), false);
      if (!enter.succeeded())
        return false;
      item.file_ref = enter.resource();
    }
  }
  return true;
}

}  // namespace

bool CreateWebURLRequest(::ppapi::URLRequestInfoData* data,
                         WebFrame* frame,
                         WebURLRequest* dest) {
  // In the out-of-process case, we've received the URLRequestInfoData
  // from the untrusted plugin and done no validation on it. We need to be
  // sure it's not being malicious by checking everything for consistency.
  if (!ValidateURLRequestData(*data) || !EnsureFileRefObjectsPopulated(data))
    return false;

  dest->initialize();
  dest->setTargetType(WebURLRequest::TargetIsObject);
  dest->setURL(frame->document().completeURL(WebString::fromUTF8(
      data->url)));
  dest->setDownloadToFile(data->stream_to_file);
  dest->setReportUploadProgress(data->record_upload_progress);

  if (!data->method.empty())
    dest->setHTTPMethod(WebString::fromUTF8(data->method));

  dest->setFirstPartyForCookies(frame->document().firstPartyForCookies());

  const std::string& headers = data->headers;
  if (!headers.empty()) {
    net::HttpUtil::HeadersIterator it(headers.begin(), headers.end(), "\n\r");
    while (it.GetNext()) {
      dest->addHTTPHeaderField(
          WebString::fromUTF8(it.name()),
          WebString::fromUTF8(it.values()));
    }
  }

  // Append the upload data.
  if (!data->body.empty()) {
    WebHTTPBody http_body;
    http_body.initialize();
    for (size_t i = 0; i < data->body.size(); ++i) {
      const URLRequestInfoData::BodyItem& item = data->body[i];
      if (item.is_file) {
        if (!AppendFileRefToBody(item.file_ref,
                                 item.start_offset,
                                 item.number_of_bytes,
                                 item.expected_last_modified_time,
                                 &http_body))
          return false;
      } else {
        DCHECK(!item.data.empty());
        http_body.appendData(WebData(item.data));
      }
    }
    dest->setHTTPBody(http_body);
  }

  // Add the "Referer" header if there is a custom referrer. Such requests
  // require universal access. For all other requests, "Referer" will be set
  // after header security checks are done in AssociatedURLLoader.
  if (data->has_custom_referrer_url && !data->custom_referrer_url.empty())
    frame->setReferrerForRequest(*dest, GURL(data->custom_referrer_url));

  if (data->has_custom_content_transfer_encoding &&
      !data->custom_content_transfer_encoding.empty()) {
    dest->addHTTPHeaderField(
        WebString::fromUTF8("Content-Transfer-Encoding"),
        WebString::fromUTF8(data->custom_content_transfer_encoding));
  }

  if (data->has_custom_user_agent) {
    dest->setExtraData(new webkit_glue::WebURLRequestExtraDataImpl(
        WebKit::WebReferrerPolicyDefault,  // Ignored.
        WebString::fromUTF8(data->custom_user_agent)));
  }

  return true;
}

bool URLRequestRequiresUniversalAccess(
    const ::ppapi::URLRequestInfoData& data) {
  return
      data.has_custom_referrer_url ||
      data.has_custom_content_transfer_encoding ||
      data.has_custom_user_agent ||
      url_util::FindAndCompareScheme(data.url, "javascript", NULL);
}

}  // namespace ppapi
}  // namespace webkit
