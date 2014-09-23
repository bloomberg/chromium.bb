// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FAKE_DATAGRAM_SOCKET_H_
#define REMOTING_PROTOCOL_FAKE_DATAGRAM_SOCKET_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_callback.h"
#include "net/socket/socket.h"
#include "remoting/protocol/datagram_channel_factory.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace remoting {
namespace protocol {

// FakeDatagramSocket implement net::StreamSocket interface. All data written to
// FakeDatagramSocket is stored in a buffer returned by written_packets().
// Read() reads data from another buffer that can be set with
// AppendInputPacket(). Pending reads are supported, so if there is a pending
// read AppendInputPacket() calls the read callback.
//
// Two fake sockets can be connected to each other using the
// PairWith() method, e.g.: a->PairWith(b). After this all data
// written to |a| can be read from |b| and vice versa. Two connected
// sockets |a| and |b| must be created and used on the same thread.
class FakeDatagramSocket : public net::Socket {
 public:
  FakeDatagramSocket();
  virtual ~FakeDatagramSocket();

  const std::vector<std::string>& written_packets() const {
    return written_packets_;
  }

  void AppendInputPacket(const std::string& data);

  // Current position in the input in number of packets, i.e. number of finished
  // Read() calls.
  int input_pos() const { return input_pos_; }

  // Pairs the socket with |peer_socket|. Deleting either of the paired sockets
  // unpairs them.
  void PairWith(FakeDatagramSocket* peer_socket);

  base::WeakPtr<FakeDatagramSocket> GetWeakPtr();

  // net::Socket implementation.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   const net::CompletionCallback& callback) OVERRIDE;
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    const net::CompletionCallback& callback) OVERRIDE;
  virtual int SetReceiveBufferSize(int32 size) OVERRIDE;
  virtual int SetSendBufferSize(int32 size) OVERRIDE;

 private:
  int CopyReadData(net::IOBuffer* buf, int buf_len);

  base::WeakPtr<FakeDatagramSocket> peer_socket_;

  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_;
  net::CompletionCallback read_callback_;

  std::vector<std::string> written_packets_;
  std::vector<std::string> input_packets_;
  int input_pos_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtrFactory<FakeDatagramSocket> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeDatagramSocket);
};

class FakeDatagramChannelFactory : public DatagramChannelFactory {
 public:
  FakeDatagramChannelFactory();
  virtual ~FakeDatagramChannelFactory();

  void set_asynchronous_create(bool asynchronous_create) {
    asynchronous_create_ = asynchronous_create;
  }

  void set_fail_create(bool fail_create) { fail_create_ = fail_create; }

  // Pair with |peer_factory|. Once paired the factory will be automatically
  // pairing created sockets with the sockets with the same name from the peer
  // factory.
  void PairWith(FakeDatagramChannelFactory* peer_factory);

  // Can be used to retrieve FakeDatagramSocket created by this factory, e.g. to
  // feed data into it. The caller doesn't get ownership of the result. Returns
  // NULL if the socket doesn't exist.
  FakeDatagramSocket* GetFakeChannel(const std::string& name);

  // DatagramChannelFactory interface.
  virtual void CreateChannel(const std::string& name,
                             const ChannelCreatedCallback& callback) OVERRIDE;
  virtual void CancelChannelCreation(const std::string& name) OVERRIDE;

 private:
  typedef std::map<std::string, base::WeakPtr<FakeDatagramSocket> > ChannelsMap;

  void NotifyChannelCreated(scoped_ptr<FakeDatagramSocket> owned_socket,
                            const std::string& name,
                            const ChannelCreatedCallback& callback);

  base::WeakPtr<FakeDatagramChannelFactory> peer_factory_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  bool asynchronous_create_;
  ChannelsMap channels_;

  bool fail_create_;

  base::WeakPtrFactory<FakeDatagramChannelFactory> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeDatagramChannelFactory);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_FAKE_DATAGRAM_SOCKET_H_
