// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pepper_p2p_channel.h"

#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/dev/transport_dev.h"
#include "ppapi/cpp/var.h"

namespace remoting {
namespace protocol {

namespace {

const char kPepperTransportUdpProtocol[] = "udp";

// Maps value returned by Recv() and Send() Pepper methods to net::Error.
int PPErrorToNetError(int result) {
  if (result > 0)
    return result;

  switch (result) {
    case PP_OK:
      return net::OK;
    case PP_OK_COMPLETIONPENDING:
      return net::ERR_IO_PENDING;
    default:
      return net::ERR_FAILED;
  }
}

}  // namespace

PepperP2PChannel::PepperP2PChannel(
    pp::Instance* pp_instance,
    const char* name,
    const IncomingCandidateCallback& candidate_callback)
    : candidate_callback_(candidate_callback),
      get_address_pending_(false),
      read_callback_(NULL),
      write_callback_(NULL) {
  transport_.reset(
      new pp::Transport_Dev(pp_instance, name, kPepperTransportUdpProtocol));
}

bool PepperP2PChannel::Init() {
  // This will return false when the GetNextAddress() returns an
  // error. Particularly it is useful to detect when the P2P Transport
  // API is not supported.
  return ProcessCandidates();
}

PepperP2PChannel::~PepperP2PChannel() {
}

void PepperP2PChannel::AddRemoteCandidate(const std::string& candidate) {
  DCHECK(CalledOnValidThread());
  transport_->ReceiveRemoteAddress(candidate);
}

int PepperP2PChannel::Read(net::IOBuffer* buf, int buf_len,
                           net::CompletionCallback* callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!read_callback_);
  DCHECK(!read_buffer_);

  int result = PPErrorToNetError(transport_->Recv(
      buf->data(), buf_len,
      pp::CompletionCallback(&PepperP2PChannel::ReadCallback, this)));

  if (result == net::ERR_IO_PENDING) {
    read_callback_ = callback;
    read_buffer_ = buf;
  }

  return result;
}

int PepperP2PChannel::Write(net::IOBuffer* buf, int buf_len,
                            net::CompletionCallback* callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!write_callback_);
  DCHECK(!write_buffer_);

  int result = PPErrorToNetError(transport_->Send(
      buf->data(), buf_len,
      pp::CompletionCallback(&PepperP2PChannel::WriteCallback, this)));

  if (result == net::ERR_IO_PENDING) {
    write_callback_ = callback;
    write_buffer_ = buf;
  }

  return result;
}

bool PepperP2PChannel::SetReceiveBufferSize(int32 size) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  return false;
}

bool PepperP2PChannel::SetSendBufferSize(int32 size) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  return false;
}

bool PepperP2PChannel::ProcessCandidates() {
  DCHECK(CalledOnValidThread());
  DCHECK(!get_address_pending_);

  while (true) {
    pp::Var address;
    int result = transport_->GetNextAddress(
        &address,
        pp::CompletionCallback(&PepperP2PChannel::NextAddressCallback, this));
    if (result == PP_OK_COMPLETIONPENDING) {
      get_address_pending_ = true;
      break;
    }

    if (result == PP_OK) {
      candidate_callback_.Run(address.AsString());
    } else {
      LOG(ERROR) << "GetNextAddress() returned an error " << result;
      return false;
    }
  }
  return true;
}

// static
void PepperP2PChannel::NextAddressCallback(void* data, int32_t result) {
  PepperP2PChannel* channel = reinterpret_cast<PepperP2PChannel*>(data);
  DCHECK(channel->CalledOnValidThread());
  channel->get_address_pending_ = false;
  channel->ProcessCandidates();
}

// static
void PepperP2PChannel::ReadCallback(void* data, int32_t result) {
  PepperP2PChannel* channel = reinterpret_cast<PepperP2PChannel*>(data);
  DCHECK(channel->CalledOnValidThread());
  DCHECK(channel->read_callback_);
  DCHECK(channel->read_buffer_);
  net::CompletionCallback* callback = channel->read_callback_;
  channel->read_callback_ = NULL;
  channel->read_buffer_ = NULL;
  callback->Run(result);
}

// static
void PepperP2PChannel::WriteCallback(void* data, int32_t result) {
  PepperP2PChannel* channel = reinterpret_cast<PepperP2PChannel*>(data);
  DCHECK(channel->CalledOnValidThread());
  DCHECK(channel->write_callback_);
  DCHECK(channel->write_buffer_);
  net::CompletionCallback* callback = channel->write_callback_;
  channel->write_callback_ = NULL;
  channel->write_buffer_ = NULL;
  callback->Run(result);
}

}  // namespace protocol
}  // namespace remoting
