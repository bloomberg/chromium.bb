// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_NET_CERT_NET_FETCHER_IMPL_H_
#define NET_CERT_NET_CERT_NET_FETCHER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"
#include "net/cert/cert_net_fetcher.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace net {

class URLRequestContext;

// A CertNetFetcher that issues requests through the provided
// URLRequestContext. The URLRequestContext must stay valid until the returned
// CertNetFetcher's Shutdown method is called. The CertNetFetcher is to be
// created and shutdown on the network thread. Its Fetch methods are to be used
// on a *different* thread, since it gives a blocking interface to URL fetching.
class NET_EXPORT CertNetFetcherImpl : public CertNetFetcher {
 public:
  class AsyncCertNetFetcherImpl;
  class RequestCore;
  struct RequestParams;

  // Creates the CertNetFetcherImpl. SetURLRequestContext must be called before
  // the fetcher can be used.
  CertNetFetcherImpl();

  // Set the URLRequestContext this fetcher should use.
  // |context_| must stay valid until Shutdown() is called.
  void SetURLRequestContext(URLRequestContext* context);

  // CertNetFetcher impl:
  void Shutdown() override;
  std::unique_ptr<Request> FetchCaIssuers(const GURL& url,
                                          int timeout_milliseconds,
                                          int max_response_bytes) override;
  std::unique_ptr<Request> FetchCrl(const GURL& url,
                                    int timeout_milliseconds,
                                    int max_response_bytes) override;
  WARN_UNUSED_RESULT std::unique_ptr<Request> FetchOcsp(
      const GURL& url,
      int timeout_milliseconds,
      int max_response_bytes) override;

 private:
  ~CertNetFetcherImpl() override;

  void DoFetchOnNetworkSequence(std::unique_ptr<RequestParams> request_params,
                                scoped_refptr<RequestCore> request);

  std::unique_ptr<Request> DoFetch(
      std::unique_ptr<RequestParams> request_params);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  // Not owned. |context_| must stay valid until Shutdown() is called.
  URLRequestContext* context_ = nullptr;
  std::unique_ptr<AsyncCertNetFetcherImpl> impl_;
};

}  // namespace net

#endif  // NET_CERT_NET_CERT_NET_FETCHER_IMPL_H_
