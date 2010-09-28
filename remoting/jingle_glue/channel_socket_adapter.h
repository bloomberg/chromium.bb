// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_CHANNEL_SOCKET_ADAPTER_H_
#define REMOTING_JINGLE_GLUE_CHANNEL_SOCKET_ADAPTER_H_

#include "net/socket/socket.h"
#include "third_party/libjingle/source/talk/base/socketaddress.h"
#include "third_party/libjingle/source/talk/base/sigslot.h"

namespace cricket {
class TransportChannel;
}  // namespace cricket

namespace remoting {

// TransportChannelSocketAdapter implements net::Socket interface on
// top of libjingle's TransportChannel. It is used by JingleChromotingConnection
// to provide net::Socket interface for channels.
class TransportChannelSocketAdapter : public net::Socket,
                                      public sigslot::has_slots<> {
 public:
  // TransportChannel object is always owned by the corresponding session.
  explicit TransportChannelSocketAdapter(cricket::TransportChannel* channel);
  virtual ~TransportChannelSocketAdapter();

  // Closes the stream. |error_code| specifies error code that will
  // be returned by Read() and Write() after the stream is closed.
  // Must be called before the session and the channel are destroyed.
  void Close(int error_code);

  // Socket interface.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   net::CompletionCallback* callback);
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    net::CompletionCallback* callback);

  virtual bool SetReceiveBufferSize(int32 size);
  virtual bool SetSendBufferSize(int32 size);

 private:
  void OnNewPacket(cricket::TransportChannel* channel,
                   const char* data, size_t data_size);
  void OnWritableState(cricket::TransportChannel* channel);
  void OnChannelDestroyed(cricket::TransportChannel* channel);

  cricket::TransportChannel* channel_;

  bool read_pending_;
  net::CompletionCallback* read_callback_;  // Not owned.
  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_;

  bool write_pending_;
  net::CompletionCallback* write_callback_;  // Not owned.
  scoped_refptr<net::IOBuffer> write_buffer_;
  int write_buffer_size_;

  int closed_error_code_;

  DISALLOW_COPY_AND_ASSIGN(TransportChannelSocketAdapter);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_CHANNEL_SOCKET_ADAPTER_H_
