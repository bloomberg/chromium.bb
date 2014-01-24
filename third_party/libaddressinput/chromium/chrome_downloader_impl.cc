// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/chromium/chrome_downloader_impl.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "url/gurl.h"

ChromeDownloaderImpl::ChromeDownloaderImpl(net::URLRequestContextGetter* getter)
    : getter_(getter) {}

ChromeDownloaderImpl::~ChromeDownloaderImpl() {
  STLDeleteContainerPairPointers(requests_.begin(), requests_.end());
}

void ChromeDownloaderImpl::Download(
    const std::string& url,
    scoped_ptr<Callback> downloaded) {
  net::URLFetcher* fetcher =
      net::URLFetcher::Create(GURL(url), net::URLFetcher::GET, this);
  fetcher->SetLoadFlags(
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher->SetRequestContext(getter_);

  requests_[fetcher] = new Request(url, downloaded.Pass());
  fetcher->Start();
}

void ChromeDownloaderImpl::OnURLFetchComplete(const net::URLFetcher* source) {
  std::map<const net::URLFetcher*, Request*>::iterator request =
      requests_.find(source);
  DCHECK(request != requests_.end());

  bool ok = source->GetResponseCode() == net::HTTP_OK;
  scoped_ptr<std::string> data(new std::string());
  if (ok)
    source->GetResponseAsString(&*data);
  (*request->second->callback)(ok, request->second->url, data.Pass());

  delete request->first;
  delete request->second;
  requests_.erase(request);
}

ChromeDownloaderImpl::Request::Request(const std::string& url,
                                       scoped_ptr<Callback> callback)
    : url(url),
      callback(callback.Pass()) {}
