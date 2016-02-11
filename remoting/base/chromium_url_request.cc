// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/chromium_url_request.h"

#include "base/callback_helpers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

namespace remoting {

ChromiumUrlRequest::ChromiumUrlRequest(
    scoped_refptr<net::URLRequestContextGetter> url_context,
    const std::string& url) {
  url_fetcher_ = net::URLFetcher::Create(GURL(url), net::URLFetcher::GET, this);
  url_fetcher_->SetRequestContext(url_context.get());
}

ChromiumUrlRequest::~ChromiumUrlRequest() {}

void ChromiumUrlRequest::AddHeader(const std::string& value) {
  url_fetcher_->AddExtraRequestHeader(value);
}

void ChromiumUrlRequest::Start(const OnResultCallback& on_result_callback) {
  DCHECK(!on_result_callback.is_null());
  DCHECK(on_result_callback_.is_null());

  on_result_callback_ = on_result_callback;
  url_fetcher_->Start();
}

void ChromiumUrlRequest::OnURLFetchComplete(
    const net::URLFetcher* url_fetcher) {
  DCHECK_EQ(url_fetcher, url_fetcher_.get());

  Result result;
  result.success =
      url_fetcher_->GetResponseCode() != net::URLFetcher::RESPONSE_CODE_INVALID;
  if (result.success) {
    result.status = url_fetcher_->GetResponseCode();
    url_fetcher_->GetResponseAsString(&result.response_body);
  }

  DCHECK(!on_result_callback_.is_null());
  base::ResetAndReturn(&on_result_callback_).Run(result);
}

ChromiumUrlRequestFactory::ChromiumUrlRequestFactory(
    scoped_refptr<net::URLRequestContextGetter> url_context)
    : url_context_(url_context) {}
ChromiumUrlRequestFactory::~ChromiumUrlRequestFactory() {}

scoped_ptr<UrlRequest> ChromiumUrlRequestFactory::CreateUrlRequest(
    const std::string& url) {
  return make_scoped_ptr(new ChromiumUrlRequest(url_context_, url));
}

}  // namespace remoting
