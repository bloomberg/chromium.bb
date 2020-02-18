// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CLIENT_CERTIFICATE_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_CLIENT_CERTIFICATE_DELEGATE_H_

#include "base/macros.h"

namespace net {
class SSLPrivateKey;
class X509Certificate;
}

namespace content {

// A delegate interface for selecting a client certificate for use with a
// network request. If the delegate is destroyed without calling
// ContinueWithCertificate, the certificate request will be aborted.
class ClientCertificateDelegate {
 public:
  ClientCertificateDelegate() {}
  virtual ~ClientCertificateDelegate() {}

  // Continue the request with |cert| and matching |key|. |cert| may be nullptr
  // to continue without supplying a certificate. This decision will be
  // remembered for future requests to the domain.
  virtual void ContinueWithCertificate(
      scoped_refptr<net::X509Certificate> cert,
      scoped_refptr<net::SSLPrivateKey> key) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientCertificateDelegate);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CLIENT_CERTIFICATE_DELEGATE_H_
