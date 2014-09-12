// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_SSL_HMAC_CHANNEL_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_SSL_HMAC_CHANNEL_AUTHENTICATOR_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "remoting/protocol/channel_authenticator.h"

namespace net {
class DrainableIOBuffer;
class GrowableIOBuffer;
class SSLSocket;
class TransportSecurityState;
}  // namespace net

namespace remoting {

class RsaKeyPair;

namespace protocol {

// SslHmacChannelAuthenticator implements ChannelAuthenticator that
// secures channels using SSL and authenticates them with a shared
// secret HMAC.
class SslHmacChannelAuthenticator : public ChannelAuthenticator,
                                    public base::NonThreadSafe {
 public:
  enum LegacyMode {
    NONE,
    SEND_ONLY,
    RECEIVE_ONLY,
  };

  // CreateForClient() and CreateForHost() create an authenticator
  // instances for client and host. |auth_key| specifies shared key
  // known by both host and client. In case of V1Authenticator the
  // |auth_key| is set to access code. For EKE-based authentication
  // |auth_key| is the key established using EKE over the signaling
  // channel.
  static scoped_ptr<SslHmacChannelAuthenticator> CreateForClient(
      const std::string& remote_cert,
      const std::string& auth_key);

  static scoped_ptr<SslHmacChannelAuthenticator> CreateForHost(
      const std::string& local_cert,
      scoped_refptr<RsaKeyPair> key_pair,
      const std::string& auth_key);

  virtual ~SslHmacChannelAuthenticator();

  // ChannelAuthenticator interface.
  virtual void SecureAndAuthenticate(
      scoped_ptr<net::StreamSocket> socket,
      const DoneCallback& done_callback) OVERRIDE;

 private:
  SslHmacChannelAuthenticator(const std::string& auth_key);

  bool is_ssl_server();

  void OnConnected(int result);

  void WriteAuthenticationBytes(bool* callback_called);
  void OnAuthBytesWritten(int result);
  bool HandleAuthBytesWritten(int result, bool* callback_called);

  void ReadAuthenticationBytes();
  void OnAuthBytesRead(int result);
  bool HandleAuthBytesRead(int result);
  bool VerifyAuthBytes(const std::string& received_auth_bytes);

  void CheckDone(bool* callback_called);
  void NotifyError(int error);
  void CallDoneCallback(int error, scoped_ptr<net::StreamSocket> socket);

  // The mutual secret used for authentication.
  std::string auth_key_;

  // Used in the SERVER mode only.
  std::string local_cert_;
  scoped_refptr<RsaKeyPair> local_key_pair_;

  // Used in the CLIENT mode only.
  std::string remote_cert_;
  scoped_ptr<net::TransportSecurityState> transport_security_state_;

  scoped_ptr<net::SSLSocket> socket_;
  DoneCallback done_callback_;

  scoped_refptr<net::DrainableIOBuffer> auth_write_buf_;
  scoped_refptr<net::GrowableIOBuffer> auth_read_buf_;

  DISALLOW_COPY_AND_ASSIGN(SslHmacChannelAuthenticator);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_SSL_HMAC_CHANNEL_AUTHENTICATOR_H_
