// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_URL_REQUEST_CONTEXT_OWNER_H_
#define SERVICES_NETWORK_URL_REQUEST_CONTEXT_OWNER_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "net/url_request/url_request_context_getter.h"

class PrefService;

namespace network {

class COMPONENT_EXPORT(NETWORK_SERVICE) NetworkURLRequestContextGetter
    : public net::URLRequestContextGetter {
 public:
  explicit NetworkURLRequestContextGetter(
      std::unique_ptr<net::URLRequestContext> url_request_context);

  // net::URLRequestContextGetter implementation:
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

  void NotifyContextShuttingDown();

 protected:
  ~NetworkURLRequestContextGetter() override;

 private:
  bool shutdown_ = false;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
};

// This owns a net::URLRequestContext and other state that's used with it.
struct COMPONENT_EXPORT(NETWORK_SERVICE) URLRequestContextOwner {
  URLRequestContextOwner();
  URLRequestContextOwner(
      std::unique_ptr<PrefService> pref_service,
      std::unique_ptr<net::URLRequestContext> url_request_context);
  ~URLRequestContextOwner();
  URLRequestContextOwner(URLRequestContextOwner&& other);
  URLRequestContextOwner& operator=(URLRequestContextOwner&& other);

  // This needs to be destroyed after the URLRequestContext.
  std::unique_ptr<PrefService> pref_service;

  scoped_refptr<NetworkURLRequestContextGetter> url_request_context_getter;
};

}  // namespace network

#endif  // SERVICES_NETWORK_URL_REQUEST_CONTEXT_OWNER_H_
