// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_THIRD_PARTY_HOST_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_THIRD_PARTY_HOST_AUTHENTICATOR_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "remoting/protocol/third_party_authenticator_base.h"

namespace remoting {

class RsaKeyPair;

namespace protocol {

// Implements the host side of the third party authentication mechanism.
// The host authenticator sends the |token_url| and |scope| obtained from the
// |TokenValidator| to the client, and expects a |token| in response.
// Once that token is received, it calls |TokenValidator| asynchronously to
// validate it, and exchange it for a |shared_secret|. Once the |TokenValidator|
// returns, the host uses the |shared_secret| to create an underlying
// |V2Authenticator|, which is used to establish the encrypted connection.
class ThirdPartyHostAuthenticator : public ThirdPartyAuthenticatorBase {
 public:
  class TokenValidator {
   public:
    // Callback passed to |ValidateThirdPartyToken|, and called once the host
    // authentication finishes. |shared_secret| should be used by the host to
    // create a V2Authenticator. In case of failure, the callback is called with
    // an empty |shared_secret|.
    typedef base::Callback<void(
                const std::string& shared_secret)> TokenValidatedCallback;

    virtual ~TokenValidator() {}

    // Validates |token| with the server and exchanges it for a |shared_secret|.
    // |token_validated_callback| is called when the host authentication ends,
    // in the same thread |ValidateThirdPartyToken| was originally called.
    // The request is canceled if this object is destroyed.
    virtual void ValidateThirdPartyToken(
        const std::string& token,
        const TokenValidatedCallback& token_validated_callback) = 0;

    // URL sent to the client, to be used by its |TokenFetcher| to get a token.
    virtual const GURL& token_url() const = 0;

    // Space-separated list of connection attributes the host must send to the
    // client, and require the token received in response to match.
    virtual const std::string& token_scope() const = 0;
  };

  // Creates a third-party host authenticator. |local_cert| and |key_pair| are
  // used by the underlying V2Authenticator to create the SSL channels.
  // |token_validator| contains the token parameters to be sent to the client
  // and is used to obtain the shared secret.
  ThirdPartyHostAuthenticator(const std::string& local_cert,
                              scoped_refptr<RsaKeyPair> key_pair,
                              scoped_ptr<TokenValidator> token_validator);
  virtual ~ThirdPartyHostAuthenticator();

 protected:
  // ThirdPartyAuthenticator implementation.
  virtual void ProcessTokenMessage(
      const buzz::XmlElement* message,
      const base::Closure& resume_callback) OVERRIDE;
  virtual void AddTokenElements(buzz::XmlElement* message) OVERRIDE;

 private:
  void OnThirdPartyTokenValidated(const buzz::XmlElement* message,
                                  const base::Closure& resume_callback,
                                  const std::string& shared_secret);

  std::string local_cert_;
  scoped_refptr<RsaKeyPair> key_pair_;
  scoped_ptr<TokenValidator> token_validator_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_THIRD_PARTY_HOST_AUTHENTICATOR_H_
