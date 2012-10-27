// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PAM_AUTHORIZATION_FACTORY_POSIX_H_
#define REMOTING_HOST_PAM_AUTHORIZATION_FACTORY_POSIX_H_

#include "remoting/protocol/authenticator.h"

#include "base/memory/scoped_ptr.h"

// PamAuthorizationFactory abuses the AuthenticatorFactory interface to apply
// PAM-based authorization on top of some underlying authentication scheme.

namespace remoting {

class PamAuthorizationFactory : public protocol::AuthenticatorFactory {
 public:
  PamAuthorizationFactory(
      scoped_ptr<protocol::AuthenticatorFactory> underlying);
  virtual ~PamAuthorizationFactory();

  virtual scoped_ptr<protocol::Authenticator> CreateAuthenticator(
      const std::string& local_jid,
      const std::string& remote_jid,
      const buzz::XmlElement* first_message) OVERRIDE;

 private:
  scoped_ptr<protocol::AuthenticatorFactory> underlying_;
};

}  // namespace remoting

#endif
