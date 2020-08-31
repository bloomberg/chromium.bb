// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_HTTP_ASYNCHRONOUS_FACTORY_IMPL_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_HTTP_ASYNCHRONOUS_FACTORY_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/printing/cloud_print/privet_http.h"
#include "chrome/browser/printing/cloud_print/privet_http_asynchronous_factory.h"

namespace local_discovery {
class EndpointResolver;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace cloud_print {

class PrivetHTTPAsynchronousFactoryImpl : public PrivetHTTPAsynchronousFactory {
 public:
  explicit PrivetHTTPAsynchronousFactoryImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~PrivetHTTPAsynchronousFactoryImpl() override;

  std::unique_ptr<PrivetHTTPResolution> CreatePrivetHTTP(
      const std::string& service_name) override;

 private:
  class ResolutionImpl : public PrivetHTTPResolution {
   public:
    ResolutionImpl(
        const std::string& service_name,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
    ~ResolutionImpl() override;

    void Start(const net::HostPortPair& address,
               ResultCallback callback) override;

   private:
    void ResolveComplete(ResultCallback callback,
                         const net::IPEndPoint& endpoint);
    std::string name_;
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
    std::unique_ptr<local_discovery::EndpointResolver> endpoint_resolver_;

    DISALLOW_COPY_AND_ASSIGN(ResolutionImpl);
  };

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrivetHTTPAsynchronousFactoryImpl);
};

}  // namespace cloud_print

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_HTTP_ASYNCHRONOUS_FACTORY_IMPL_H_
