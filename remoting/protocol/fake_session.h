// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FAKE_SESSION_H_
#define REMOTING_PROTOCOL_FAKE_SESSION_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_callback.h"
#include "net/socket/socket.h"
#include "net/socket/stream_socket.h"
#include "remoting/protocol/session.h"

class MessageLoop;

namespace remoting {
namespace protocol {

extern const char kTestJid[];

// FakeSocket implement net::Socket interface for FakeConnection. All data
// written to FakeSocket is stored in a buffer returned by written_data().
// Read() reads data from another buffer that can be set with AppendInputData().
// Pending reads are supported, so if there is a pending read AppendInputData()
// calls the read callback.
//
// Two fake sockets can be connected to each other using the
// PairWith() method, e.g.: a->PairWith(b). After this all data
// written to |a| can be read from |b| and vica versa. Two connected
// sockets |a| and |b| must be created and used on the same thread.
class FakeSocket : public net::StreamSocket {
 public:
  FakeSocket();
  virtual ~FakeSocket();

  const std::string& written_data() const { return written_data_; }

  void AppendInputData(const std::vector<char>& data);
  void PairWith(FakeSocket* peer_socket);
  int input_pos() const { return input_pos_; }
  bool read_pending() const { return read_pending_; }

  // net::Socket implementation.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   const net::CompletionCallback& callback) OVERRIDE;
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    const net::CompletionCallback& callback) OVERRIDE;

  virtual bool SetReceiveBufferSize(int32 size) OVERRIDE;
  virtual bool SetSendBufferSize(int32 size) OVERRIDE;

  // net::StreamSocket interface.
  virtual int Connect(const net::CompletionCallback& callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual bool IsConnectedAndIdle() const OVERRIDE;
  virtual int GetPeerAddress(net::IPEndPoint* address) const OVERRIDE;
  virtual int GetLocalAddress(net::IPEndPoint* address) const OVERRIDE;
  virtual const net::BoundNetLog& NetLog() const OVERRIDE;
  virtual void SetSubresourceSpeculation() OVERRIDE;
  virtual void SetOmniboxSpeculation() OVERRIDE;
  virtual bool WasEverUsed() const OVERRIDE;
  virtual bool UsingTCPFastOpen() const OVERRIDE;
  virtual int64 NumBytesRead() const OVERRIDE;
  virtual base::TimeDelta GetConnectTimeMicros() const OVERRIDE;
  virtual net::NextProto GetNegotiatedProtocol() const OVERRIDE;

 private:
  bool read_pending_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_;
  net::CompletionCallback read_callback_;
  base::WeakPtr<FakeSocket> peer_socket_;

  std::string written_data_;
  std::string input_data_;
  int input_pos_;

  net::BoundNetLog net_log_;

  MessageLoop* message_loop_;
  base::WeakPtrFactory<FakeSocket> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeSocket);
};

// FakeUdpSocket is similar to FakeSocket but behaves as UDP socket. All written
// packets are stored separetely in written_packets(). AppendInputPacket() adds
// one packet that will be returned by Read().
class FakeUdpSocket : public net::Socket {
 public:
  FakeUdpSocket();
  virtual ~FakeUdpSocket();

  const std::vector<std::string>& written_packets() const {
    return written_packets_;
  }

  void AppendInputPacket(const char* data, int data_size);
  int input_pos() const { return input_pos_; }

  // net::Socket implementation.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   const net::CompletionCallback& callback) OVERRIDE;
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    const net::CompletionCallback& callback) OVERRIDE;

  virtual bool SetReceiveBufferSize(int32 size) OVERRIDE;
  virtual bool SetSendBufferSize(int32 size) OVERRIDE;

 private:
  bool read_pending_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_;
  net::CompletionCallback read_callback_;

  std::vector<std::string> written_packets_;
  std::vector<std::string> input_packets_;
  int input_pos_;

  MessageLoop* message_loop_;

  DISALLOW_COPY_AND_ASSIGN(FakeUdpSocket);
};

// FakeSession is a dummy protocol::Session that uses FakeSocket for all
// channels.
class FakeSession : public Session {
 public:
  FakeSession();
  virtual ~FakeSession();

  const StateChangeCallback& state_change_callback() {
    return state_change_callback_;
  }

  void set_message_loop(MessageLoop* message_loop) {
    message_loop_ = message_loop;
  }

  void set_error(ErrorCode error) { error_ = error; }

  bool is_closed() const { return closed_; }

  FakeSocket* GetStreamChannel(const std::string& name);
  FakeUdpSocket* GetDatagramChannel(const std::string& name);

  // Session implementation.
  virtual void SetStateChangeCallback(
      const StateChangeCallback& callback) OVERRIDE;

  virtual void SetRouteChangeCallback(
      const RouteChangeCallback& callback) OVERRIDE;

  virtual ErrorCode error() OVERRIDE;

  virtual void CreateStreamChannel(
      const std::string& name, const StreamChannelCallback& callback) OVERRIDE;
  virtual void CreateDatagramChannel(
      const std::string& name,
      const DatagramChannelCallback& callback) OVERRIDE;
  virtual void CancelChannelCreation(const std::string& name) OVERRIDE;

  virtual const std::string& jid() OVERRIDE;

  virtual const CandidateSessionConfig* candidate_config() OVERRIDE;
  virtual const SessionConfig& config() OVERRIDE;
  virtual void set_config(const SessionConfig& config) OVERRIDE;

  virtual void Close() OVERRIDE;

 public:
  StateChangeCallback state_change_callback_;
  RouteChangeCallback route_change_callback_;
  scoped_ptr<const CandidateSessionConfig> candidate_config_;
  SessionConfig config_;
  MessageLoop* message_loop_;

  std::map<std::string, FakeSocket*> stream_channels_;
  std::map<std::string, FakeUdpSocket*> datagram_channels_;

  std::string jid_;

  ErrorCode error_;
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(FakeSession);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_FAKE_SESSION_H_
