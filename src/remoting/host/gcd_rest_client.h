// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_GCD_REST_CLIENT_H_
#define REMOTING_HOST_GCD_REST_CLIENT_H_

#include <memory>
#include <queue>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/clock.h"
#include "remoting/base/oauth_token_getter.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
struct ResourceRequest;
}  // namespace network

namespace remoting {

// A client for calls to the GCD REST API.
class GcdRestClient {
 public:
  // Result of a GCD call.
  enum Result {
    SUCCESS,
    NETWORK_ERROR,
    NO_SUCH_HOST,
    OTHER_ERROR,
  };

  typedef base::Callback<void(Result result)> ResultCallback;

  // Note: |token_getter| must outlive this object.
  GcdRestClient(
      const std::string& gcd_base_url,
      const std::string& gcd_device_id,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      OAuthTokenGetter* token_getter);

  ~GcdRestClient();

  // Tests whether is object is currently running a request.  Only one
  // request at a time may be pending.
  bool HasPendingRequest() { return !!resource_request_ || !!url_loader_; }

  // Sends a 'patchState' request to the GCD API.  Constructs and
  // sends an appropriate JSON message M where |patch_details| becomes
  // the value of M.patches[0].patch.
  void PatchState(std::unique_ptr<base::DictionaryValue> patch_details,
                  const GcdRestClient::ResultCallback& callback);

  void SetClockForTest(base::Clock* clock);

 private:
  void OnTokenReceived(OAuthTokenGetter::Status status,
                       const std::string& user_email,
                       const std::string& access_token);
  void FinishCurrentRequest(Result result);

  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  std::string gcd_base_url_;
  std::string gcd_device_id_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  OAuthTokenGetter* token_getter_;
  base::Clock* clock_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  std::unique_ptr<network::ResourceRequest> resource_request_;
  std::string patch_string_;
  ResultCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GcdRestClient);
};

}  // namespace remoting

#endif  // REMOTING_HOST_GCD_REST_CLIENT_H_
