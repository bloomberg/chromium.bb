// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_LIBADDRESSINPUT_CHROMIUM_CHROME_DOWNLOADER_IMPL_H_
#define THIRD_PARTY_LIBADDRESSINPUT_CHROMIUM_CHROME_DOWNLOADER_IMPL_H_

#include <map>
#include <string>

#include "base/memory/scoped_vector.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/downloader.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace autofill {

// A class for downloading rules to let libaddressinput validate international
// addresses.
class ChromeDownloaderImpl : public ::i18n::addressinput::Downloader,
                             public net::URLFetcherDelegate {
 public:
  explicit ChromeDownloaderImpl(net::URLRequestContextGetter* getter);
  virtual ~ChromeDownloaderImpl();

  // ::i18n::addressinput::Downloader:
  virtual void Download(const std::string& url,
                        const Callback& downloaded) const OVERRIDE;

  // net::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 private:
  struct Request {
    Request(const std::string& url,
            scoped_ptr<net::URLFetcher> fetcher,
            const Callback& callback);

    std::string url;
    // The data that's received.
    std::string data;
    // The object that manages retrieving the data.
    scoped_ptr<net::URLFetcher> fetcher;
    const Callback& callback;
  };

  // Non-const version of Download().
  void DoDownload(const std::string& url, const Callback& downloaded);

  net::URLRequestContextGetter* const getter_;  // weak

  // Maps from active url fetcher to request metadata. The value is owned.
  std::map<const net::URLFetcher*, Request*> requests_;

  DISALLOW_COPY_AND_ASSIGN(ChromeDownloaderImpl);
};

}  // namespace autofill

#endif  // THIRD_PARTY_LIBADDRESSINPUT_CHROMIUM_CHROME_DOWNLOADER_IMPL_H_
