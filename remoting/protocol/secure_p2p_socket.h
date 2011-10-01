// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implement a secure P2P socket according to the W3C spec
//
// "Video conferencing and peer-to-peer communication"
// http://www.whatwg.org/specs/web-apps/current-work/complete/video-conferencing-and-peer-to-peer-communication.html#peer-to-peer-connections
//
// This class operates on an establish socket to perform encryption for P2P
// connection. This class does not perform chunking for outgoing buffers, all
// outgoing buffers have to be 44 bytes smaller than MTU to allow space for
// header to support encryption.

#ifndef REMOTING_PROTOCOL_SECURE_P2P_SOCKET_H_
#define REMOTING_PROTOCOL_SOCKET_P2P_SOCKET_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "crypto/encryptor.h"
#include "crypto/hmac.h"
#include "net/socket/socket.h"

namespace crypto {
class SymmetricKey;
}  // namespace crypto

namespace net {
class IOBufferWithSize;
}  // namespace net

namespace remoting {
namespace protocol {

class SecureP2PSocket : public net::Socket {
 public:
  // Construct a secured P2P socket using |socket| as the underlying
  // socket. Ownership of |socket| is transfered to this object.
  SecureP2PSocket(net::Socket* socket, const std::string& ice_key);
  virtual ~SecureP2PSocket();

  // Socket implementation.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   net::OldCompletionCallback* callback);
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    net::OldCompletionCallback* callback);
  virtual bool SetReceiveBufferSize(int32 size);
  virtual bool SetSendBufferSize(int32 size);

 private:
  int ReadInternal();
  void ReadDone(int err);
  void WriteDone(int err);
  int DecryptBuffer(int size);

  scoped_ptr<net::Socket> socket_;

  uint64 write_seq_;
  uint64 read_seq_;

  net::OldCompletionCallback* user_read_callback_;
  scoped_refptr<net::IOBuffer> user_read_buf_;
  int user_read_buf_len_;

  net::OldCompletionCallback* user_write_callback_;
  int user_write_buf_len_;

  scoped_ptr<net::OldCompletionCallback> read_callback_;
  scoped_refptr<net::IOBufferWithSize> read_buf_;

  scoped_ptr<net::OldCompletionCallback> write_callback_;

  scoped_ptr<crypto::SymmetricKey> mask_key_;
  crypto::HMAC msg_hasher_;
  crypto::Encryptor encryptor_;

  DISALLOW_COPY_AND_ASSIGN(SecureP2PSocket);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_SOCKET_P2P_SOCKET_H_
