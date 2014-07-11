// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/chromium/chrome_downloader_impl.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "url/gurl.h"

namespace autofill {

namespace {

// A URLFetcherResponseWriter that writes into a provided buffer.
class UnownedStringWriter : public net::URLFetcherResponseWriter {
 public:
  UnownedStringWriter(std::string* data) : data_(data) {}
  virtual ~UnownedStringWriter() {}

  virtual int Initialize(const net::CompletionCallback& callback) OVERRIDE {
    data_->clear();
    return net::OK;
  }

  virtual int Write(net::IOBuffer* buffer,
                    int num_bytes,
                    const net::CompletionCallback& callback) OVERRIDE {
    data_->append(buffer->data(), num_bytes);
    return num_bytes;
  }

  virtual int Finish(const net::CompletionCallback& callback) OVERRIDE {
    return net::OK;
  }

 private:
  std::string* data_;  // weak reference.

  DISALLOW_COPY_AND_ASSIGN(UnownedStringWriter);
};

}  // namespace

ChromeDownloaderImpl::ChromeDownloaderImpl(net::URLRequestContextGetter* getter)
    : getter_(getter) {}

ChromeDownloaderImpl::~ChromeDownloaderImpl() {
  STLDeleteValues(&requests_);
}

void ChromeDownloaderImpl::Download(const std::string& url,
                                    const Callback& downloaded) const {
  const_cast<ChromeDownloaderImpl*>(this)->DoDownload(url, downloaded);
}

void ChromeDownloaderImpl::OnURLFetchComplete(const net::URLFetcher* source) {
  std::map<const net::URLFetcher*, Request*>::iterator request =
      requests_.find(source);
  DCHECK(request != requests_.end());

  bool ok = source->GetResponseCode() == net::HTTP_OK;
  scoped_ptr<std::string> data(new std::string());
  if (ok)
    data->swap(request->second->data);
  request->second->callback(ok, request->second->url, data.release());

  delete request->second;
  requests_.erase(request);
}

ChromeDownloaderImpl::Request::Request(const std::string& url,
                                       scoped_ptr<net::URLFetcher> fetcher,
                                       const Callback& callback)
    : url(url),
      fetcher(fetcher.Pass()),
      callback(callback) {}

void ChromeDownloaderImpl::DoDownload(const std::string& url,
                                      const Callback& downloaded) {
  GURL resource(url);
  if (!resource.SchemeIsSecure()) {
    downloaded(false, url, NULL);
    return;
  }

  scoped_ptr<net::URLFetcher> fetcher(
      net::URLFetcher::Create(resource, net::URLFetcher::GET, this));
  fetcher->SetLoadFlags(
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher->SetRequestContext(getter_);

  Request* request = new Request(url, fetcher.Pass(), downloaded);
  request->fetcher->SaveResponseWithWriter(
      scoped_ptr<net::URLFetcherResponseWriter>(
          new UnownedStringWriter(&request->data)));
  requests_[request->fetcher.get()] = request;
  request->fetcher->Start();
}

}  // namespace autofill
