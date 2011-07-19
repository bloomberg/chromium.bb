// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PEPPER_P2P_CHANNEL_H_
#define REMOTING_PROTOCOL_PEPPER_P2P_CHANNEL_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/socket/socket.h"
#include "ppapi/c/pp_stdint.h"

namespace pp {
class Instance;
class Transport_Dev;
}  // namespace pp

namespace remoting {
namespace protocol {

// This class create P2PChannel based on the Pepper P2P Transport API
// and provides net::Socket interface on top of it.
class PepperP2PChannel : public base::NonThreadSafe,
                         public net::Socket {
 public:
  // TODO(sergeyu): Use cricket::Candidate instead of strings to
  // represent candidates.
  typedef base::Callback<void(const std::string&)> IncomingCandidateCallback;

  PepperP2PChannel(pp::Instance* pp_instance, const char* name,
                   const IncomingCandidateCallback& candidate_callback);
  virtual ~PepperP2PChannel();

  bool Init();

  // Adds candidate received from the peer.
  void AddRemoteCandidate(const std::string& candidate);

  // net::Socket interface.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   net::CompletionCallback* callback);
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    net::CompletionCallback* callback);
  virtual bool SetReceiveBufferSize(int32 size);
  virtual bool SetSendBufferSize(int32 size);

 private:
  // Callbacks for PPAPI calls.
  static void NextAddressCallback(void* data, int32_t result);
  static void ReadCallback(void* data, int32_t result);
  static void WriteCallback(void* data, int32_t result);

  bool ProcessCandidates();

  IncomingCandidateCallback candidate_callback_;

  scoped_ptr<pp::Transport_Dev> transport_;

  bool get_address_pending_;

  net::CompletionCallback* read_callback_;
  scoped_refptr<net::IOBuffer> read_buffer_;

  net::CompletionCallback* write_callback_;
  scoped_refptr<net::IOBuffer> write_buffer_;

  DISALLOW_COPY_AND_ASSIGN(PepperP2PChannel);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PEPPER_P2P_CHANNEL_H_
