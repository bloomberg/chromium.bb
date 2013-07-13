// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_THIRD_PARTY_CLIENT_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_THIRD_PARTY_CLIENT_AUTHENTICATOR_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/third_party_authenticator_base.h"
#include "url/gurl.h"

class GURL;

namespace remoting {
namespace protocol {

// Implements the client side of the third party authentication mechanism.
// The client authenticator expects a |token_url| and |scope| in the first
// message from the host, then calls the |TokenFetcher| asynchronously to
// request a |token| and |shared_secret| from that url. If the server requires
// interactive authentication, the |TokenFetcher| implementation will show the
// appropriate UI. Once the |TokenFetcher| returns, the client sends the |token|
// to the host, and uses the |shared_secret| to create an underlying
// |V2Authenticator|, which is used to establish the encrypted connection.
class ThirdPartyClientAuthenticator : public ThirdPartyAuthenticatorBase {
 public:
  class TokenFetcher {
   public:
    // Callback passed to |FetchThirdPartyToken|, and called once the client
    // authentication finishes. |token| is an opaque string that should be sent
    // directly to the host. |shared_secret| should be used by the client to
    // create a V2Authenticator. In case of failure, the callback is called with
    // an empty |token| and |shared_secret|.
    typedef base::Callback<void(
        const std::string& token,
        const std::string& shared_secret)> TokenFetchedCallback;

    virtual ~TokenFetcher() {}

    // Fetches a third party token from |token_url|. |host_public_key| is sent
    // to the server so it can later authenticate the host. |scope| is a string
    // with a space-separated list of attributes for this connection (e.g.
    // "hostjid:abc@example.com/123 clientjid:def@example.org/456".
    // |token_fetched_callback| is called when the client authentication ends,
    // in the same thread |FetchThirdPartyToken| was originally called.
    // The request is canceled if the TokenFetcher is destroyed.
    virtual void FetchThirdPartyToken(
        const GURL& token_url,
        const std::string& scope,
        const TokenFetchedCallback& token_fetched_callback) = 0;
  };

  // Creates a third-party client authenticator for the host with the given
  // |host_public_key|. |token_fetcher| is used to get the authentication token.
  explicit ThirdPartyClientAuthenticator(
      scoped_ptr<TokenFetcher> token_fetcher);
  virtual ~ThirdPartyClientAuthenticator();

 protected:
  // ThirdPartyAuthenticator implementation.
  virtual void ProcessTokenMessage(
      const buzz::XmlElement* message,
      const base::Closure& resume_callback) OVERRIDE;
  virtual void AddTokenElements(buzz::XmlElement* message) OVERRIDE;

 private:
  void OnThirdPartyTokenFetched(const base::Closure& resume_callback,
                                const std::string& third_party_token,
                                const std::string& shared_secret);

  std::string token_;
  scoped_ptr<TokenFetcher> token_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(ThirdPartyClientAuthenticator);
};


}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_THIRD_PARTY_CLIENT_AUTHENTICATOR_H_
