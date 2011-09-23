// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pepper_transport_socket_adapter.h"

#include "base/logging.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/dev/transport_dev.h"
#include "ppapi/cpp/var.h"

namespace remoting {
namespace protocol {

namespace {

const char kTcpProtocol[] = "tcp";

// Maps value returned by Recv() and Send() Pepper methods to net::Error.
int PPErrorToNetError(int result) {
  if (result > 0)
    return result;

  switch (result) {
    case PP_OK:
      return net::OK;
    case PP_OK_COMPLETIONPENDING:
      return net::ERR_IO_PENDING;
    case PP_ERROR_NOTSUPPORTED:
    case PP_ERROR_NOINTERFACE:
      return net::ERR_NOT_IMPLEMENTED;
    default:
      return net::ERR_FAILED;
  }
}

}  // namespace

PepperTransportSocketAdapter::PepperTransportSocketAdapter(
    pp::Transport_Dev* transport,
    const std::string& name,
    Observer* observer)
    : name_(name),
      observer_(observer),
      transport_(transport),
      connected_(false),
      get_address_pending_(false),
      read_callback_(NULL),
      write_callback_(NULL) {
  callback_factory_.Initialize(this);
}

PepperTransportSocketAdapter::~PepperTransportSocketAdapter() {
  observer_->OnChannelDeleted();
}

void PepperTransportSocketAdapter::AddRemoteCandidate(
    const std::string& candidate) {
  DCHECK(CalledOnValidThread());
  if (transport_.get())
    transport_->ReceiveRemoteAddress(candidate);
}

int PepperTransportSocketAdapter::Read(net::IOBuffer* buf, int buf_len,
                                       net::CompletionCallback* callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!read_callback_);
  DCHECK(!read_buffer_);

  if (!transport_.get())
    return net::ERR_SOCKET_NOT_CONNECTED;

  int result = PPErrorToNetError(transport_->Recv(
      buf->data(), buf_len,
      callback_factory_.NewOptionalCallback(
          &PepperTransportSocketAdapter::OnRead)));

  if (result == net::ERR_IO_PENDING) {
    read_callback_ = callback;
    read_buffer_ = buf;
  }

  return result;
}

int PepperTransportSocketAdapter::Write(net::IOBuffer* buf, int buf_len,
                                        net::CompletionCallback* callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!write_callback_);
  DCHECK(!write_buffer_);

  if (!transport_.get())
    return net::ERR_SOCKET_NOT_CONNECTED;

  int result = PPErrorToNetError(transport_->Send(
      buf->data(), buf_len,
      callback_factory_.NewOptionalCallback(
          &PepperTransportSocketAdapter::OnWrite)));

  if (result == net::ERR_IO_PENDING) {
    write_callback_ = callback;
    write_buffer_ = buf;
  }

  return result;
}

bool PepperTransportSocketAdapter::SetReceiveBufferSize(int32 size) {
  DCHECK(CalledOnValidThread());
  // TODO(sergeyu): Implement this: crbug.com/91439.
  NOTIMPLEMENTED();
  return false;
}

bool PepperTransportSocketAdapter::SetSendBufferSize(int32 size) {
  DCHECK(CalledOnValidThread());
  // TODO(sergeyu): Implement this: crbug.com/91439.
  NOTIMPLEMENTED();
  return false;
}

int PepperTransportSocketAdapter::Connect(net::CompletionCallback* callback) {
  DCHECK(CalledOnValidThread());

  if (!transport_.get())
    return net::ERR_UNEXPECTED;

  connect_callback_ = callback;

  // This will return false when GetNextAddress() returns an
  // error. This helps to detect when the P2P Transport API is not
  // supported.
  int result = ProcessCandidates();
  if (result != net::OK)
    return result;

  result = transport_->Connect(
      callback_factory_.NewRequiredCallback(
          &PepperTransportSocketAdapter::OnConnect));
  DCHECK_EQ(result, PP_OK_COMPLETIONPENDING);

  return net::ERR_IO_PENDING;
}

void PepperTransportSocketAdapter::Disconnect() {
  DCHECK(CalledOnValidThread());
  transport_.reset();
}

bool PepperTransportSocketAdapter::IsConnected() const {
  DCHECK(CalledOnValidThread());
  return connected_;
}

bool PepperTransportSocketAdapter::IsConnectedAndIdle() const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  return false;
}

int PepperTransportSocketAdapter::GetPeerAddress(
    net::AddressList* address) const {
  DCHECK(CalledOnValidThread());

  // We don't have a meaningful peer address, but we can't return an
  // error, so we return a INADDR_ANY instead.
  net::IPAddressNumber ip_address(4);
  *address = net::AddressList::CreateFromIPAddress(ip_address, 0);
  return net::OK;
}

int PepperTransportSocketAdapter::GetLocalAddress(
    net::IPEndPoint* address) const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  return net::ERR_FAILED;
}

const net::BoundNetLog& PepperTransportSocketAdapter::NetLog() const {
  DCHECK(CalledOnValidThread());
  return net_log_;
}

void PepperTransportSocketAdapter::SetSubresourceSpeculation() {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void PepperTransportSocketAdapter::SetOmniboxSpeculation() {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

bool PepperTransportSocketAdapter::WasEverUsed() const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  return true;
}

bool PepperTransportSocketAdapter::UsingTCPFastOpen() const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  return true;
}

int64 PepperTransportSocketAdapter::NumBytesRead() const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  return 0;
}

base::TimeDelta PepperTransportSocketAdapter::GetConnectTimeMicros() const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  return base::TimeDelta();
}


int PepperTransportSocketAdapter::ProcessCandidates() {
  DCHECK(CalledOnValidThread());
  DCHECK(!get_address_pending_);
  DCHECK(transport_.get());

  while (true) {
    pp::Var address;
    int result = transport_->GetNextAddress(
        &address, callback_factory_.NewOptionalCallback(
            &PepperTransportSocketAdapter::OnNextAddress));
    if (result == PP_OK_COMPLETIONPENDING) {
      get_address_pending_ = true;
      break;
    }

    if (result == PP_OK) {
      observer_->OnChannelNewLocalCandidate(address.AsString());
    } else {
      LOG(ERROR) << "GetNextAddress() returned an error " << result;
      return PPErrorToNetError(result);
    }
  }
  return net::OK;
}

void PepperTransportSocketAdapter::OnNextAddress(int32_t result) {
  DCHECK(CalledOnValidThread());

  get_address_pending_ = false;
  ProcessCandidates();
}

void PepperTransportSocketAdapter::OnConnect(int result) {
  DCHECK(CalledOnValidThread());
  DCHECK(connect_callback_);

  if (result == PP_OK)
    connected_ = true;

  net::CompletionCallback* callback = connect_callback_;
  connect_callback_ = NULL;
  callback->Run(PPErrorToNetError(result));
}

void PepperTransportSocketAdapter::OnRead(int32_t result) {
  DCHECK(CalledOnValidThread());
  DCHECK(read_callback_);
  DCHECK(read_buffer_);

  net::CompletionCallback* callback = read_callback_;
  read_callback_ = NULL;
  read_buffer_ = NULL;
  callback->Run(PPErrorToNetError(result));
}

void PepperTransportSocketAdapter::OnWrite(int32_t result) {
  DCHECK(CalledOnValidThread());
  DCHECK(write_callback_);
  DCHECK(write_buffer_);

  net::CompletionCallback* callback = write_callback_;
  write_callback_ = NULL;
  write_buffer_ = NULL;
  callback->Run(PPErrorToNetError(result));
}

}  // namespace protocol
}  // namespace remoting
