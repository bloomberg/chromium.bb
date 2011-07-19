// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/weburl_loader_mock_factory.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLResponse.h"
#include "webkit/support/weburl_loader_mock.h"

using WebKit::WebData;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebURLError;
using WebKit::WebURLLoader;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;

struct WebURLLoaderMockFactory::ResponseInfo {
  WebKit::WebURLResponse response;
  FilePath file_path;
};

WebURLLoaderMockFactory::WebURLLoaderMockFactory() {}

WebURLLoaderMockFactory::~WebURLLoaderMockFactory() {}

void WebURLLoaderMockFactory::RegisterURL(const WebURL& url,
                                          const WebURLResponse& response,
                                          const WebString& file_path) {
  ResponseInfo response_info;
  response_info.response = response;
  if (!file_path.isNull() && !file_path.isEmpty()) {
#if defined(OS_POSIX)
    // TODO(jcivelli): On Linux, UTF8 might not be correct.
    response_info.file_path =
        FilePath(static_cast<std::string>(file_path.utf8()));
#elif defined(OS_WIN)
    response_info.file_path =
        FilePath(std::wstring(file_path.data(), file_path.length()));
#endif
    DCHECK(file_util::PathExists(response_info.file_path));
  }

  DCHECK(url_to_reponse_info_.find(url) == url_to_reponse_info_.end());
  url_to_reponse_info_[url] = response_info;
}

void WebURLLoaderMockFactory::UnregisterURL(const WebKit::WebURL& url) {
  URLToResponseMap::iterator iter = url_to_reponse_info_.find(url);
  DCHECK(iter != url_to_reponse_info_.end());
  url_to_reponse_info_.erase(iter);
}

void WebURLLoaderMockFactory::UnregisterAllURLs() {
  url_to_reponse_info_.clear();
}

void WebURLLoaderMockFactory::ServeAsynchronousRequests() {
  // Serving a request might trigger more requests, so we cannot iterate on
  // pending_loaders_ as it might get modified.
  while (!pending_loaders_.empty()) {
    LoaderToRequestMap::iterator iter = pending_loaders_.begin();
    WebURLLoaderMock* loader = iter->first;
    const WebURLRequest& request = iter->second;
    WebURLResponse response;
    WebURLError error;
    WebData data;
    LoadRequest(request, &response, &error, &data);
    loader->ServeAsynchronousRequest(response, data, error);
    pending_loaders_.erase(iter);
  }
}

bool WebURLLoaderMockFactory::IsMockedURL(const WebKit::WebURL& url) {
  return url_to_reponse_info_.find(url) != url_to_reponse_info_.end();
}

void WebURLLoaderMockFactory::CancelLoad(WebURLLoaderMock* loader) {
  LoaderToRequestMap::iterator iter = pending_loaders_.find(loader);
  DCHECK(iter != pending_loaders_.end());
  pending_loaders_.erase(iter);
}

WebURLLoader* WebURLLoaderMockFactory::CreateURLLoader(
    WebURLLoader* default_loader) {
  DCHECK(default_loader);
  return new WebURLLoaderMock(this, default_loader);
}

void WebURLLoaderMockFactory::LoadSynchronously(const WebURLRequest& request,
                                                WebURLResponse* response,
                                                WebURLError* error,
                                                WebData* data) {
  LoadRequest(request, response, error, data);
}

void WebURLLoaderMockFactory::LoadAsynchronouly(const WebURLRequest& request,
                                                WebURLLoaderMock* loader) {
  LoaderToRequestMap::iterator iter = pending_loaders_.find(loader);
  DCHECK(iter == pending_loaders_.end());
  pending_loaders_[loader] = request;
}

void WebURLLoaderMockFactory::LoadRequest(const WebURLRequest& request,
                                          WebURLResponse* response,
                                          WebURLError* error,
                                          WebData* data) {
  URLToResponseMap::const_iterator iter =
      url_to_reponse_info_.find(request.url());
  if (iter == url_to_reponse_info_.end()) {
    // Non mocked URLs should not have been passed to the default URLLoader.
    NOTREACHED();
    return;
  }

  if (!ReadFile(iter->second.file_path, data)) {
    NOTREACHED();
    return;
  }

  *response = iter->second.response;
}

// static
bool WebURLLoaderMockFactory::ReadFile(const FilePath& file_path,
                                       WebData* data) {
  int64 file_size = 0;
  if (!file_util::GetFileSize(file_path, &file_size))
    return false;

  int size = static_cast<int>(file_size);
  scoped_array<char> buffer(new char[size]);
  data->reset();
  int read_count = file_util::ReadFile(file_path, buffer.get(), size);
  if (read_count == -1)
    return false;
  DCHECK(read_count == size);
  data->assign(buffer.get(), size);

  return true;
}
