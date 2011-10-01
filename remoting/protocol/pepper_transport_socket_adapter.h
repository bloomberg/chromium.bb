// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PEPPER_TRANSPORT_SOCKET_ADAPTER_H_
#define REMOTING_PROTOCOL_PEPPER_TRANSPORT_SOCKET_ADAPTER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/net_log.h"
#include "net/socket/stream_socket.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/completion_callback.h"

namespace pp {
class Instance;
class Transport_Dev;
}  // namespace pp

namespace remoting {
namespace protocol {

// This class implements net::StreamSocket interface on top of the the
// Pepper P2P Transport API.
class PepperTransportSocketAdapter : public base::NonThreadSafe,
                                     public net::StreamSocket {
 public:
  // Observer is used to notify about new local candidates and
  // deletion of the adapter.
  class Observer {
   public:
    Observer() { }
    virtual ~Observer() { }
    virtual void OnChannelDeleted() = 0;
    virtual void OnChannelNewLocalCandidate(const std::string& candidate) = 0;
  };

  PepperTransportSocketAdapter(pp::Transport_Dev* transport,
                               const std::string& name,
                               Observer* observer);
  virtual ~PepperTransportSocketAdapter();

  const std::string& name() { return name_; }

  // Adds candidate received from the peer.
  void AddRemoteCandidate(const std::string& candidate);

  // net::Socket interface.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   net::OldCompletionCallback* callback) OVERRIDE;
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    net::OldCompletionCallback* callback) OVERRIDE;
  virtual bool SetReceiveBufferSize(int32 size) OVERRIDE;
  virtual bool SetSendBufferSize(int32 size) OVERRIDE;

  // net::StreamSocket interface.
  virtual int Connect(net::OldCompletionCallback* callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual bool IsConnectedAndIdle() const OVERRIDE;
  virtual int GetPeerAddress(net::AddressList* address) const OVERRIDE;
  virtual int GetLocalAddress(net::IPEndPoint* address) const OVERRIDE;
  virtual const net::BoundNetLog& NetLog() const OVERRIDE;
  virtual void SetSubresourceSpeculation() OVERRIDE;
  virtual void SetOmniboxSpeculation() OVERRIDE;
  virtual bool WasEverUsed() const OVERRIDE;
  virtual bool UsingTCPFastOpen() const OVERRIDE;
  virtual int64 NumBytesRead() const OVERRIDE;
  virtual base::TimeDelta GetConnectTimeMicros() const OVERRIDE;

 private:
  // Callbacks for PPAPI calls.
  void OnConnect(int result);
  void OnNextAddress(int32_t result);
  void OnRead(int32_t result);
  void OnWrite(int32_t result);

  int ProcessCandidates();

  std::string name_;
  Observer* observer_;

  scoped_ptr<pp::Transport_Dev> transport_;

  net::OldCompletionCallback* connect_callback_;
  bool connected_;

  bool get_address_pending_;

  net::OldCompletionCallback* read_callback_;
  scoped_refptr<net::IOBuffer> read_buffer_;

  net::OldCompletionCallback* write_callback_;
  scoped_refptr<net::IOBuffer> write_buffer_;

  net::BoundNetLog net_log_;

  pp::CompletionCallbackFactory<PepperTransportSocketAdapter> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperTransportSocketAdapter);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PEPPER_TRANSPORT_SOCKET_ADAPTER_H_
