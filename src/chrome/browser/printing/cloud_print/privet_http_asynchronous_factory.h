// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_HTTP_ASYNCHRONOUS_FACTORY_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_HTTP_ASYNCHRONOUS_FACTORY_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"

namespace net {
class HostPortPair;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace cloud_print {

class PrivetHTTPClient;

class PrivetHTTPResolution {
 public:
  using ResultCallback =
      base::OnceCallback<void(std::unique_ptr<PrivetHTTPClient>)>;

  virtual ~PrivetHTTPResolution() {}

  virtual void Start(const net::HostPortPair& address,
                     ResultCallback callback) = 0;
};

class PrivetHTTPAsynchronousFactory {
 public:
  using ResultCallback = PrivetHTTPResolution::ResultCallback;

  virtual ~PrivetHTTPAsynchronousFactory() {}

  static std::unique_ptr<PrivetHTTPAsynchronousFactory> CreateInstance(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  virtual std::unique_ptr<PrivetHTTPResolution> CreatePrivetHTTP(
      const std::string& service_name) = 0;
};

}  // namespace cloud_print

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_HTTP_ASYNCHRONOUS_FACTORY_H_
