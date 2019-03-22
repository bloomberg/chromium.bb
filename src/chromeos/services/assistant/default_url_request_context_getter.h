// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The file comes from Google Home(cast) implementation.

#ifndef CHROMEOS_SERVICES_ASSISTANT_DEFAULT_URL_REQUEST_CONTEXT_GETTER_H_
#define CHROMEOS_SERVICES_ASSISTANT_DEFAULT_URL_REQUEST_CONTEXT_GETTER_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "net/url_request/url_request_context_getter.h"

namespace chromeos {
namespace assistant {

// A default URLRequestContextGetter implementation for creating a URL request
// context for HTTP-related communications in the voice UI client.
//
// URL request context will have no HTTP caching.
//
// Instance of this class should be kept and reused for as long as possible.
// Some internal objects, such as HostResolver are not always safe to destroy
// and may cause random crashes (b/30282661).
class DefaultURLRequestContextGetter : public ::net::URLRequestContextGetter {
 public:
  // Creates a new task runner thread with the given name.
  explicit DefaultURLRequestContextGetter(
      const std::string& network_thread_name);

  // Uses the provided |network_task_runner| as the network task runner.
  explicit DefaultURLRequestContextGetter(
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner);

  // net::URLRequestContextGetter implementation:
  ::net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

  void SetProxyConfiguration(const std::string& proxy_server,
                             const std::string& bypass_list);

 private:
  ~DefaultURLRequestContextGetter() override;

  void CreateContext();

  void SetProxyConfigurationInternal(const std::string& proxy_server,
                                     const std::string& bypass_list);

  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  std::unique_ptr<::net::URLRequestContext> request_context_;

  DISALLOW_COPY_AND_ASSIGN(DefaultURLRequestContextGetter);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_DEFAULT_URL_REQUEST_CONTEXT_GETTER_H_
