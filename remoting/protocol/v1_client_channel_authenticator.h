// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_V1_CLIENT_CHANNEL_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_V1_CLIENT_CHANNEL_AUTHENTICATOR_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/completion_callback.h"
#include "remoting/protocol/channel_authenticator.h"

namespace net {
class CertVerifier;
class DrainableIOBuffer;
class GrowableIOBuffer;
class SSLClientSocket;
}  // namespace net

namespace remoting {
namespace protocol {

class V1ClientChannelAuthenticator : public ChannelAuthenticator,
                                         public base::NonThreadSafe {
 public:
  V1ClientChannelAuthenticator(const std::string& host_cert,
                               const std::string& shared_secret);
  virtual ~V1ClientChannelAuthenticator();

  // ChannelAuthenticator implementation.
  virtual void SecureAndAuthenticate(
      net::StreamSocket* socket, const DoneCallback& done_callback) OVERRIDE;

 private:
  void OnConnected(int result);
  void WriteAuthenticationBytes();
  void OnAuthBytesWritten(int result);
  bool HandleAuthBytesWritten(int result);

  std::string host_cert_;
  std::string shared_secret_;
  scoped_ptr<net::SSLClientSocket> socket_;
  DoneCallback done_callback_;

  scoped_ptr<net::CertVerifier> cert_verifier_;
  scoped_refptr<net::DrainableIOBuffer> auth_write_buf_;

  net::OldCompletionCallbackImpl<V1ClientChannelAuthenticator>
      connect_callback_;
  net::OldCompletionCallbackImpl<V1ClientChannelAuthenticator>
      auth_write_callback_;

  DISALLOW_COPY_AND_ASSIGN(V1ClientChannelAuthenticator);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_V1_CLIENT_CHANNEL_AUTHENTICATOR_H_
