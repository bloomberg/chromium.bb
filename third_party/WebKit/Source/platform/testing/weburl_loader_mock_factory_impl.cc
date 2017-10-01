// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/weburl_loader_mock_factory_impl.h"

#include <stdint.h>

#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/testing/weburl_loader_mock.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/FilePathConversion.h"
#include "public/platform/Platform.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"

namespace blink {

std::unique_ptr<WebURLLoaderMockFactory> WebURLLoaderMockFactory::Create() {
  return WTF::WrapUnique(new WebURLLoaderMockFactoryImpl(nullptr));
}

WebURLLoaderMockFactoryImpl::WebURLLoaderMockFactoryImpl(
    TestingPlatformSupport* platform)
    : platform_(platform) {}

WebURLLoaderMockFactoryImpl::~WebURLLoaderMockFactoryImpl() {}

std::unique_ptr<WebURLLoader> WebURLLoaderMockFactoryImpl::CreateURLLoader(
    std::unique_ptr<WebURLLoader> default_loader) {
  return std::make_unique<WebURLLoaderMock>(this, std::move(default_loader));
}

void WebURLLoaderMockFactoryImpl::RegisterURL(const WebURL& url,
                                              const WebURLResponse& response,
                                              const WebString& file_path) {
  ResponseInfo response_info;
  response_info.response = response;
  if (!file_path.IsNull() && !file_path.IsEmpty()) {
    response_info.file_path = blink::WebStringToFilePath(file_path);
    DCHECK(base::PathExists(response_info.file_path))
        << response_info.file_path.MaybeAsASCII() << " does not exist.";
  }

  DCHECK(url_to_response_info_.find(url) == url_to_response_info_.end());
  url_to_response_info_.Set(url, response_info);
}

void WebURLLoaderMockFactoryImpl::RegisterErrorURL(
    const WebURL& url,
    const WebURLResponse& response,
    const WebURLError& error) {
  DCHECK(url_to_response_info_.find(url) == url_to_response_info_.end());
  RegisterURL(url, response, WebString());
  url_to_error_info_.Set(url, error);
}

void WebURLLoaderMockFactoryImpl::UnregisterURL(const blink::WebURL& url) {
  URLToResponseMap::iterator iter = url_to_response_info_.find(url);
  DCHECK(iter != url_to_response_info_.end());
  url_to_response_info_.erase(iter);

  URLToErrorMap::iterator error_iter = url_to_error_info_.find(url);
  if (error_iter != url_to_error_info_.end())
    url_to_error_info_.erase(error_iter);
}

void WebURLLoaderMockFactoryImpl::RegisterURLProtocol(
    const WebString& protocol,
    const WebURLResponse& response,
    const WebString& file_path) {
  DCHECK(protocol.ContainsOnlyASCII());

  ResponseInfo response_info;
  response_info.response = response;
  if (!file_path.IsNull() && !file_path.IsEmpty()) {
    response_info.file_path = blink::WebStringToFilePath(file_path);
    DCHECK(base::PathExists(response_info.file_path))
        << response_info.file_path.MaybeAsASCII() << " does not exist.";
  }

  DCHECK(protocol_to_response_info_.find(protocol) ==
         protocol_to_response_info_.end());
  protocol_to_response_info_.Set(protocol, response_info);
}

void WebURLLoaderMockFactoryImpl::UnregisterURLProtocol(
    const WebString& protocol) {
  ProtocolToResponseMap::iterator iter =
      protocol_to_response_info_.find(protocol);
  DCHECK(iter != protocol_to_response_info_.end());
  protocol_to_response_info_.erase(iter);
}

void WebURLLoaderMockFactoryImpl::UnregisterAllURLsAndClearMemoryCache() {
  url_to_response_info_.clear();
  url_to_error_info_.clear();
  protocol_to_response_info_.clear();
  GetMemoryCache()->EvictResources();
}

void WebURLLoaderMockFactoryImpl::ServeAsynchronousRequests() {
  // Serving a request might trigger more requests, so we cannot iterate on
  // pending_loaders_ as it might get modified.
  while (!pending_loaders_.IsEmpty()) {
    LoaderToRequestMap::iterator iter = pending_loaders_.begin();
    WeakPtr<WebURLLoaderMock> loader(iter->key->GetWeakPtr());
    const WebURLRequest request = iter->value;
    pending_loaders_.erase(loader.get());

    WebURLResponse response;
    WebURLError error;
    WebData data;
    LoadRequest(request.Url(), &response, &error, &data);
    // Follow any redirects while the loader is still active.
    while (response.HttpStatusCode() >= 300 &&
           response.HttpStatusCode() < 400) {
      WebURL new_url = loader->ServeRedirect(request, response);
      RunUntilIdle();
      if (!loader || loader->is_cancelled() || loader->is_deferred())
        break;
      LoadRequest(new_url, &response, &error, &data);
    }
    // Serve the request if the loader is still active.
    if (loader && !loader->is_cancelled() && !loader->is_deferred()) {
      loader->ServeAsynchronousRequest(delegate_, response, data, error);
      RunUntilIdle();
    }
  }
  RunUntilIdle();
}

bool WebURLLoaderMockFactoryImpl::IsMockedURL(const blink::WebURL& url) {
  WebURLError error;
  ResponseInfo response_info;
  return LookupURL(url, &error, &response_info);
}

void WebURLLoaderMockFactoryImpl::CancelLoad(WebURLLoaderMock* loader) {
  pending_loaders_.erase(loader);
}

void WebURLLoaderMockFactoryImpl::LoadSynchronously(
    const WebURLRequest& request,
    WebURLResponse* response,
    WebURLError* error,
    WebData* data,
    int64_t* encoded_data_length) {
  LoadRequest(request.Url(), response, error, data);
  *encoded_data_length = data->size();
}

void WebURLLoaderMockFactoryImpl::LoadAsynchronouly(
    const WebURLRequest& request,
    WebURLLoaderMock* loader) {
  DCHECK(!pending_loaders_.Contains(loader));
  pending_loaders_.Set(loader, request);
}

void WebURLLoaderMockFactoryImpl::RunUntilIdle() {
  if (platform_)
    platform_->RunUntilIdle();
  else
    base::RunLoop().RunUntilIdle();
}

void WebURLLoaderMockFactoryImpl::LoadRequest(const WebURL& url,
                                              WebURLResponse* response,
                                              WebURLError* error,
                                              WebData* data) {
  ResponseInfo response_info;
  if (!LookupURL(url, error, &response_info)) {
    // Non mocked URLs should not have been passed to the default URLLoader.
    NOTREACHED();
    return;
  }

  if (!error->reason && !ReadFile(response_info.file_path, data)) {
    NOTREACHED();
    return;
  }

  *response = response_info.response;
}

bool WebURLLoaderMockFactoryImpl::LookupURL(const WebURL& url,
                                            WebURLError* error,
                                            ResponseInfo* response_info) {
  URLToErrorMap::const_iterator error_iter = url_to_error_info_.find(url);
  if (error_iter != url_to_error_info_.end())
    *error = error_iter->value;

  URLToResponseMap::const_iterator iter = url_to_response_info_.find(url);
  if (iter != url_to_response_info_.end()) {
    *response_info = iter->value;
    return true;
  }

  for (const auto& key_value_pair : protocol_to_response_info_) {
    String protocol = key_value_pair.key;
    if (url.ProtocolIs(protocol.Ascii().data())) {
      *response_info = key_value_pair.value;
      return true;
    }
  }

  return false;
}

// static
bool WebURLLoaderMockFactoryImpl::ReadFile(const base::FilePath& file_path,
                                           WebData* data) {
  // If the path is empty then we return an empty file so tests can simulate
  // requests without needing to actually load files.
  if (file_path.empty())
    return true;

  std::string buffer;
  if (!base::ReadFileToString(file_path, &buffer))
    return false;

  data->Assign(buffer.data(), buffer.size());
  return true;
}

} // namespace blink
