// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CHANNEL_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_CHANNEL_AUTHENTICATOR_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/completion_callback.h"

namespace net {
class DrainableIOBuffer;
class GrowableIOBuffer;
class SSLSocket;
}  // namespace net

namespace remoting {
namespace protocol {

class ChannelAuthenticator : public base::NonThreadSafe {
 public:
  enum Result {
    SUCCESS,
    FAILURE,
  };

  typedef base::Callback<void(Result)> DoneCallback;

  ChannelAuthenticator() { }
  virtual ~ChannelAuthenticator() { }

  // Starts authentication of the |socket|. |done_callback| is called
  // when authentication is finished. Caller retains ownership of
  // |socket|. |shared_secret| is a shared secret that we use to
  // authenticate the channel.
  virtual void Authenticate(net::SSLSocket* socket,
                            const DoneCallback& done_callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChannelAuthenticator);
};

class HostChannelAuthenticator : public ChannelAuthenticator {
 public:
  HostChannelAuthenticator(const std::string& shared_secret);
  virtual ~HostChannelAuthenticator();

  // ChannelAuthenticator overrides.
  virtual void Authenticate(net::SSLSocket* socket,
                            const DoneCallback& done_callback) OVERRIDE;

 private:
  void DoAuthRead();
  void OnAuthBytesRead(int result);
  bool HandleAuthBytesRead(int result);
  bool VerifyAuthBytes(const std::string& received_auth_bytes);

  std::string shared_secret_;
  std::string auth_bytes_;
  net::SSLSocket* socket_;
  DoneCallback done_callback_;

  scoped_refptr<net::GrowableIOBuffer> auth_read_buf_;

  net::OldCompletionCallbackImpl<HostChannelAuthenticator> auth_read_callback_;

  DISALLOW_COPY_AND_ASSIGN(HostChannelAuthenticator);
};

class ClientChannelAuthenticator : public ChannelAuthenticator {
 public:
  ClientChannelAuthenticator(const std::string& shared_secret);
  virtual ~ClientChannelAuthenticator();

  // ChannelAuthenticator overrides.
  virtual void Authenticate(net::SSLSocket* socket,
                            const DoneCallback& done_callback) OVERRIDE;

 private:
  void DoAuthWrite();
  void OnAuthBytesWritten(int result);
  bool HandleAuthBytesWritten(int result);

  std::string shared_secret_;
  net::SSLSocket* socket_;
  DoneCallback done_callback_;

  scoped_refptr<net::DrainableIOBuffer> auth_write_buf_;

  net::OldCompletionCallbackImpl<ClientChannelAuthenticator>
      auth_write_callback_;

  DISALLOW_COPY_AND_ASSIGN(ClientChannelAuthenticator);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CHANNEL_AUTHENTICATOR_H_
