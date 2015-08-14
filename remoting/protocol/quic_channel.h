// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_QUIC_CHANNEL_H_
#define REMOTING_PROTOCOL_QUIC_CHANNEL_H_

#include "net/quic/p2p/quic_p2p_stream.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/protocol/p2p_stream_socket.h"

namespace net {
class DrainableIOBuffer;
}  // namespace net

namespace remoting {
namespace protocol {

// QuicChannel implements P2PStreamSocket interface for a QuicP2PStream.
class QuicChannel : public net::QuicP2PStream::Delegate,
                    public P2PStreamSocket {
 public:
  QuicChannel(net::QuicP2PStream* stream,
              const base::Closure& on_destroyed_callback);
  ~QuicChannel() override;

  const std::string& name() { return name_; }

  // P2PStreamSocket interface.
  int Read(const scoped_refptr<net::IOBuffer>& buffer,
           int buffer_len,
           const net::CompletionCallback& callback) override;
  int Write(const scoped_refptr<net::IOBuffer>& buffer,
            int buffer_len,
            const net::CompletionCallback& callback) override;

 protected:
  void SetName(const std::string& name);

  // Owned by QuicSession.
  net::QuicP2PStream* stream_;

 private:
  // net::QuicP2PStream::Delegate interface.
  void OnDataReceived(const char* data, int length) override;
  void OnClose(net::QuicErrorCode error) override;

  base::Closure on_destroyed_callback_;

  std::string name_;

  CompoundBuffer data_received_;

  net::CompletionCallback read_callback_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_ = 0;

  int error_ = 0;

  DISALLOW_COPY_AND_ASSIGN(QuicChannel);
};

// Client side of a channel. Sends the |name| specified in the constructor to
// the peer.
class QuicClientChannel : public QuicChannel {
 public:
  QuicClientChannel(net::QuicP2PStream* stream,
                    const base::Closure& on_destroyed_callback,
                    const std::string& name);
  ~QuicClientChannel() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicClientChannel);
};

// Host side of a channel. Receives name from the peer after ReceiveName is
// called. Read() can be called only after the name is received.
class QuicServerChannel : public QuicChannel {
 public:
  QuicServerChannel(net::QuicP2PStream* stream,
                    const base::Closure& on_destroyed_callback);
  ~QuicServerChannel() override;

  // Must be called after the constructor to receive channel name.
  // |name_received_callback| must use QuicChannel::name() to get the name.
  // Empty name() indicates failure to receive it.
  void ReceiveName(const base::Closure& name_received_callback);

 private:
  void OnNameSizeReadResult(int result);
  void ReadNameLoop(int result);
  void OnNameReadResult(int result);

  base::Closure name_received_callback_;
  uint8_t name_length_ = 0;
  scoped_refptr<net::DrainableIOBuffer> name_read_buffer_;

  DISALLOW_COPY_AND_ASSIGN(QuicServerChannel);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_QUIC_CHANNEL_H_
