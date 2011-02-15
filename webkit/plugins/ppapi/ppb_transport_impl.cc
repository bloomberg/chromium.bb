// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_transport_impl.h"

#include "ppapi/c/dev/ppb_transport_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "third_party/libjingle/source/talk/p2p/base/p2ptransportchannel.h"
#include "third_party/libjingle/source/talk/p2p/client/httpportallocator.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/var.h"

namespace webkit {
namespace ppapi {

namespace {

PP_Resource CreateTransport(PP_Instance instance_id, const char* name,
                            const char* proto) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;

  scoped_refptr<PPB_Transport_Impl> t(new PPB_Transport_Impl(instance));
  if (!t->Init(name, proto))
    return 0;

  return t->GetReference();
}

PP_Bool IsTransport(PP_Resource resource) {
  return BoolToPPBool(Resource::GetAs<PPB_Transport_Impl>(resource) != NULL);
}

PP_Bool IsWritable(PP_Resource resource) {
  scoped_refptr<PPB_Transport_Impl> t(
      Resource::GetAs<PPB_Transport_Impl>(resource));
  return BoolToPPBool((t.get()) ? t->IsWritable() : false);
}

int32_t Connect(PP_Resource resource, PP_CompletionCallback callback) {
  scoped_refptr<PPB_Transport_Impl> t(
      Resource::GetAs<PPB_Transport_Impl>(resource));
  return (t.get()) ? t->Connect(callback) : PP_ERROR_BADRESOURCE;
}

int32_t GetNextAddress(PP_Resource resource, PP_Var* address,
                       PP_CompletionCallback callback) {
  scoped_refptr<PPB_Transport_Impl> t(
      Resource::GetAs<PPB_Transport_Impl>(resource));
  return (t.get())? t->GetNextAddress(address, callback) : PP_ERROR_BADRESOURCE;
}

int32_t ReceiveRemoteAddress(PP_Resource resource, PP_Var address) {
  scoped_refptr<PPB_Transport_Impl> t(
      Resource::GetAs<PPB_Transport_Impl>(resource));
  return (t.get())? t->ReceiveRemoteAddress(address) : PP_ERROR_BADRESOURCE;
}

int32_t Recv(PP_Resource resource, void* data, uint32_t len,
             PP_CompletionCallback callback) {
  scoped_refptr<PPB_Transport_Impl> t(
      Resource::GetAs<PPB_Transport_Impl>(resource));
  return (t.get())? t->Recv(data, len, callback) : PP_ERROR_BADRESOURCE;
}

int32_t Send(PP_Resource resource, const void* data, uint32_t len,
             PP_CompletionCallback callback) {
  scoped_refptr<PPB_Transport_Impl> t(
      Resource::GetAs<PPB_Transport_Impl>(resource));
  return (t.get())? t->Send(data, len, callback) : PP_ERROR_BADRESOURCE;
}

// Disconnects from the remote peer.
int32_t Close(PP_Resource resource) {
  scoped_refptr<PPB_Transport_Impl> t(
      Resource::GetAs<PPB_Transport_Impl>(resource));
  return (t.get())? t->Close() : PP_ERROR_BADRESOURCE;
}

const PPB_Transport_Dev ppb_transport = {
  &CreateTransport,
  &IsTransport,
  &IsWritable,
  &Connect,
  &GetNextAddress,
  &ReceiveRemoteAddress,
  &Recv,
  &Send,
  &Close,
};

}  // namespace

PPB_Transport_Impl::PPB_Transport_Impl(PluginInstance* instance)
    : Resource(instance),
      network_manager_(new talk_base::NetworkManager()),
      allocator_(new cricket::HttpPortAllocator(network_manager_.get(), "")) {
  std::vector<talk_base::SocketAddress> stun_hosts;
  stun_hosts.push_back(talk_base::SocketAddress("stun.l.google.com", 19302));
  allocator_->SetStunHosts(stun_hosts);
  // TODO(sergeyu): Use port allocator that works inside sandbox.
}

PPB_Transport_Impl::~PPB_Transport_Impl() {
}

const PPB_Transport_Dev* PPB_Transport_Impl::GetInterface() {
  return &ppb_transport;
}

PPB_Transport_Impl* PPB_Transport_Impl::AsPPB_Transport_Impl() {
  return this;
}

bool PPB_Transport_Impl::Init(const char* name, const char* proto) {
  // For now, always create http://www.google.com/transport/p2p .
  channel_.reset(new cricket::P2PTransportChannel(
      name, "", NULL, allocator_.get()));
  channel_->SignalRequestSignaling.connect(
      this, &PPB_Transport_Impl::OnRequestSignaling);
  channel_->SignalWritableState.connect(
      this, &PPB_Transport_Impl::OnWriteableState);
  channel_->SignalCandidateReady.connect(
      this, &PPB_Transport_Impl::OnCandidateReady);
  channel_->SignalReadPacket.connect(
      this, &PPB_Transport_Impl::OnReadPacket);
  return true;
}

bool PPB_Transport_Impl::IsWritable() const {
  return channel_->writable();
}

int32_t PPB_Transport_Impl::Connect(PP_CompletionCallback callback) {
  // TODO(juberti): Fail if we're already connected.
  if (connect_callback_.get() && !connect_callback_->completed())
    return PP_ERROR_INPROGRESS;

  channel_->Connect();

  PP_Resource resource_id = GetReferenceNoAddRef();
  CHECK(resource_id);
  connect_callback_ = new TrackedCompletionCallback(
      instance()->module()->GetCallbackTracker(), resource_id, callback);
  return PP_ERROR_WOULDBLOCK;
}

int32_t PPB_Transport_Impl::GetNextAddress(PP_Var* address,
                                           PP_CompletionCallback callback) {
  if (next_address_callback_.get() && !next_address_callback_->completed())
    return PP_ERROR_INPROGRESS;

  if (!local_candidates_.empty()) {
    Serialize(local_candidates_.front(), address);
    local_candidates_.pop_front();
    return PP_OK;
  }

  PP_Resource resource_id = GetReferenceNoAddRef();
  CHECK(resource_id);
  next_address_callback_ = new TrackedCompletionCallback(
      instance()->module()->GetCallbackTracker(), resource_id, callback);
  return PP_ERROR_WOULDBLOCK;
}

int32_t PPB_Transport_Impl::ReceiveRemoteAddress(PP_Var address) {
  cricket::Candidate candidate;
  if (!Deserialize(address, &candidate)) {
    return PP_ERROR_FAILED;
  }

  channel_->OnCandidate(candidate);
  return PP_OK;
}

int32_t PPB_Transport_Impl::Recv(void* data, uint32_t len,
                                 PP_CompletionCallback callback) {
  if (recv_callback_.get() && !recv_callback_->completed())
    return PP_ERROR_INPROGRESS;

  // TODO(juberti): Should we store packets that are received when
  // no callback is installed?

  recv_buffer_ = data;
  recv_buffer_size_ = len;

  PP_Resource resource_id = GetReferenceNoAddRef();
  CHECK(resource_id);
  recv_callback_ = new TrackedCompletionCallback(
      instance()->module()->GetCallbackTracker(), resource_id, callback);
  return PP_ERROR_WOULDBLOCK;
}

int32_t PPB_Transport_Impl::Send(const void* data, uint32_t len,
                                 PP_CompletionCallback callback) {
  return channel_->SendPacket(static_cast<const char*>(data), len);
}

int32_t PPB_Transport_Impl::Close() {
  channel_->Reset();
  instance()->module()->GetCallbackTracker()->AbortAll();
  return PP_OK;
}

void PPB_Transport_Impl::OnRequestSignaling() {
  channel_->OnSignalingReady();
}

void PPB_Transport_Impl::OnCandidateReady(
    cricket::TransportChannelImpl* channel,
    const cricket::Candidate& candidate) {
  // Store the candidate first before calling the callback.
  local_candidates_.push_back(candidate);

  if (next_address_callback_.get() && next_address_callback_->completed()) {
    scoped_refptr<TrackedCompletionCallback> callback;
    callback.swap(next_address_callback_);
    callback->Run(PP_OK);
  }
}

void PPB_Transport_Impl::OnWriteableState(cricket::TransportChannel* channel) {
  if (connect_callback_.get() && connect_callback_->completed()) {
    scoped_refptr<TrackedCompletionCallback> callback;
    callback.swap(connect_callback_);
    callback->Run(PP_OK);
  }
}

void PPB_Transport_Impl::OnReadPacket(cricket::TransportChannel* channel,
                                      const char* data, size_t len) {
  if (recv_callback_.get() && recv_callback_->completed()) {
    scoped_refptr<TrackedCompletionCallback> callback;
    callback.swap(recv_callback_);

    if (len <= recv_buffer_size_) {
      memcpy(recv_buffer_, data, len);
      callback->Run(PP_OK);
    } else {
      callback->Run(PP_ERROR_FAILED);
    }
  }
  // TODO(sergeyu): Buffer incoming packet if there is no pending read.
}

bool PPB_Transport_Impl::Serialize(const cricket::Candidate& candidate,
                                   PP_Var* address) {
  // TODO(juberti): Come up with a real wire format!
  std::string blob = candidate.ToString();
  Var::PluginReleasePPVar(*address);
  *address = StringVar::StringToPPVar(instance()->module(), blob);
  return true;
}

bool PPB_Transport_Impl::Deserialize(PP_Var address,
                                     cricket::Candidate* candidate) {
  // TODO(juberti): Implement this.
  return false;
}

}  // namespace ppapi
}  // namespace webkit
