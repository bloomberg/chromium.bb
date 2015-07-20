// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FAKE_STREAM_SOCKET_H_
#define REMOTING_PROTOCOL_FAKE_STREAM_SOCKET_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_callback.h"
#include "remoting/protocol/p2p_stream_socket.h"
#include "remoting/protocol/stream_channel_factory.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace remoting {
namespace protocol {

// FakeStreamSocket implement P2PStreamSocket interface. All data written to
// FakeStreamSocket is stored in a buffer returned by written_data(). Read()
// reads data from another buffer that can be set with AppendInputData().
// Pending reads are supported, so if there is a pending read AppendInputData()
// calls the read callback.
//
// Two fake sockets can be connected to each other using the
// PairWith() method, e.g.: a->PairWith(b). After this all data
// written to |a| can be read from |b| and vice versa. Two connected
// sockets |a| and |b| must be created and used on the same thread.
class FakeStreamSocket : public P2PStreamSocket {
 public:
  FakeStreamSocket();
  ~FakeStreamSocket() override;

  // Returns all data written to the socket.
  const std::string& written_data() const { return written_data_; }

  // Sets maximum number of bytes written by each Write() call.
  void set_write_limit(int write_limit) { write_limit_ = write_limit; }

  // Enables asynchronous Write().
  void set_async_write(bool async_write) { async_write_ = async_write; }

  // Set error codes for the next Write() call. Once returned the
  // value is automatically reset to net::OK .
  void set_next_write_error(int error) { next_write_error_ = error; }

  // Appends |data| to the read buffer.
  void AppendInputData(const std::string& data);

  // Causes Read() to fail with |error| once the read buffer is exhausted. If
  // there is a currently pending Read, it is interrupted.
  void AppendReadError(int error);

  // Pairs the socket with |peer_socket|. Deleting either of the paired sockets
  // unpairs them.
  void PairWith(FakeStreamSocket* peer_socket);

  // Current input position in bytes.
  int input_pos() const { return input_pos_; }

  // True if a Read() call is currently pending.
  bool read_pending() const { return !read_callback_.is_null(); }

  base::WeakPtr<FakeStreamSocket> GetWeakPtr();

  // P2PStreamSocket interface.
  int Read(const scoped_refptr<net::IOBuffer>& buf, int buf_len,
           const net::CompletionCallback& callback) override;
  int Write(const scoped_refptr<net::IOBuffer>& buf, int buf_len,
            const net::CompletionCallback& callback) override;

 private:
  void DoAsyncWrite(const scoped_refptr<net::IOBuffer>& buf, int buf_len,
                    const net::CompletionCallback& callback);
  void DoWrite(const scoped_refptr<net::IOBuffer>& buf, int buf_len);

  bool async_write_;
  bool write_pending_;
  int write_limit_;
  int next_write_error_;

  int next_read_error_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_;
  net::CompletionCallback read_callback_;
  base::WeakPtr<FakeStreamSocket> peer_socket_;

  std::string written_data_;
  std::string input_data_;
  int input_pos_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtrFactory<FakeStreamSocket> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeStreamSocket);
};

// StreamChannelFactory that creates FakeStreamSocket.
class FakeStreamChannelFactory : public StreamChannelFactory {
 public:
  FakeStreamChannelFactory();
  ~FakeStreamChannelFactory() override;

  void set_asynchronous_create(bool asynchronous_create) {
    asynchronous_create_ = asynchronous_create;
  }

  void set_fail_create(bool fail_create) { fail_create_ = fail_create; }

  FakeStreamSocket* GetFakeChannel(const std::string& name);

  // ChannelFactory interface.
  void CreateChannel(const std::string& name,
                     const ChannelCreatedCallback& callback) override;
  void CancelChannelCreation(const std::string& name) override;

 private:
  void NotifyChannelCreated(scoped_ptr<FakeStreamSocket> owned_channel,
                            const std::string& name,
                            const ChannelCreatedCallback& callback);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  bool asynchronous_create_;
  std::map<std::string, base::WeakPtr<FakeStreamSocket> > channels_;

  bool fail_create_;

  base::WeakPtrFactory<FakeStreamChannelFactory> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeStreamChannelFactory);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_FAKE_STREAM_SOCKET_H_
