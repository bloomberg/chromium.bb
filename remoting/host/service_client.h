// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SERVICE_CLIENT_H_
#define REMOTING_HOST_SERVICE_CLIENT_H_

#include <string>

#include "base/memory/ref_counted.h"

namespace net {
class URLRequestContextGetter;
}

// A class that gives access to the Chromoting service.
namespace remoting {

class ServiceClient {
 public:
  class Delegate {
   public:
    // Invoked on a successful response.
    virtual void OnSuccess() = 0;
    // Invoked when there is an OAuth error.
    virtual void OnOAuthError() = 0;
    // Invoked when there is a network error or upon receiving an invalid
    // response.
    virtual void OnNetworkError(int response_code) = 0;

   protected:
    virtual ~Delegate() {}
  };
  ServiceClient(net::URLRequestContextGetter* context_getter);
  ~ServiceClient();

  void RegisterHost(const std::string& host_id,
                    const std::string& host_name,
                    const std::string& public_key,
                    const std::string& oauth_access_token,
                    Delegate* delegate);

 private:
  // The guts of the implementation live in this class.
  class Core;
  scoped_refptr<Core> core_;
  DISALLOW_COPY_AND_ASSIGN(ServiceClient);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SERVICE_CLIENT_H_
