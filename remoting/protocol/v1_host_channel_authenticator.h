// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_V1_HOST_CHANNEL_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_V1_HOST_CHANNEL_AUTHENTICATOR_H_

#include "remoting/protocol/channel_authenticator.h"

#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/completion_callback.h"

namespace crypto {
class RSAPrivateKey;
}  // namespace crypto

namespace net {
class GrowableIOBuffer;
class SSLServerSocket;
class SSLSocket;
}  // namespace net

namespace remoting {
namespace protocol {

class V1HostChannelAuthenticator : public ChannelAuthenticator,
                                       public base::NonThreadSafe  {
 public:
  // Caller retains ownership of |local_private_key|. It must exist
  // while this object exists.
  V1HostChannelAuthenticator(const std::string& local_cert,
                                 crypto::RSAPrivateKey* local_private_key,
                                 const std::string& shared_secret);
  virtual ~V1HostChannelAuthenticator();

  // ChannelAuthenticator interface.
  virtual void SecureAndAuthenticate(
      net::StreamSocket* socket, const DoneCallback& done_callback) OVERRIDE;

 private:
  void OnConnected(int result);
  void DoAuthRead();
  void OnAuthBytesRead(int result);
  bool HandleAuthBytesRead(int result);
  bool VerifyAuthBytes(const std::string& received_auth_bytes);

  std::string local_cert_;
  crypto::RSAPrivateKey* local_private_key_;
  std::string shared_secret_;
  scoped_ptr<net::SSLServerSocket> socket_;
  DoneCallback done_callback_;

  scoped_refptr<net::GrowableIOBuffer> auth_read_buf_;

  net::OldCompletionCallbackImpl<V1HostChannelAuthenticator>
      connect_callback_;
  net::OldCompletionCallbackImpl<V1HostChannelAuthenticator>
      auth_read_callback_;

  DISALLOW_COPY_AND_ASSIGN(V1HostChannelAuthenticator);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_V1_HOST_CHANNEL_AUTHENTICATOR_H_
