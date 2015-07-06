// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CHANNEL_SOCKET_ADAPTER_H_
#define REMOTING_PROTOCOL_CHANNEL_SOCKET_ADAPTER_H_

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/threading/thread_checker.h"
#include "net/socket/socket.h"
#include "third_party/webrtc/base/asyncpacketsocket.h"
#include "third_party/webrtc/base/sigslot.h"
#include "third_party/webrtc/base/socketaddress.h"

namespace cricket {
class TransportChannel;
}  // namespace cricket

namespace remoting {
namespace protocol {

// TransportChannelSocketAdapter implements net::Socket interface on
// top of libjingle's TransportChannel. It is used by LibjingleTransportFactory
// to provide net::Socket interface for channels.
class TransportChannelSocketAdapter : public net::Socket,
                                      public sigslot::has_slots<> {
 public:
  // Doesn't take ownership of the |channel|. The |channel| must outlive
  // this adapter.
  explicit TransportChannelSocketAdapter(cricket::TransportChannel* channel);
  ~TransportChannelSocketAdapter() override;

  // Sets callback that should be called when the adapter is being
  // destroyed. The callback is not allowed to touch the adapter, but
  // can do anything else, e.g. destroy the TransportChannel.
  void SetOnDestroyedCallback(const base::Closure& callback);

  // Closes the stream. |error_code| specifies error code that will
  // be returned by Read() and Write() after the stream is closed.
  // Must be called before the session and the channel are destroyed.
  void Close(int error_code);

  // net::Socket interface.
  int Read(net::IOBuffer* buf,
           int buf_len,
           const net::CompletionCallback& callback) override;
  int Write(net::IOBuffer* buf,
            int buf_len,
            const net::CompletionCallback& callback) override;

  int SetReceiveBufferSize(int32 size) override;
  int SetSendBufferSize(int32 size) override;

 private:
  void OnNewPacket(cricket::TransportChannel* channel,
                   const char* data,
                   size_t data_size,
                   const rtc::PacketTime& packet_time,
                   int flags);
  void OnWritableState(cricket::TransportChannel* channel);
  void OnChannelDestroyed(cricket::TransportChannel* channel);

  base::ThreadChecker thread_checker_;

  cricket::TransportChannel* channel_;

  base::Closure destruction_callback_;

  net::CompletionCallback read_callback_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_;

  net::CompletionCallback write_callback_;
  scoped_refptr<net::IOBuffer> write_buffer_;
  int write_buffer_size_;

  int closed_error_code_;

  DISALLOW_COPY_AND_ASSIGN(TransportChannelSocketAdapter);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CHANNEL_SOCKET_ADAPTER_H_
