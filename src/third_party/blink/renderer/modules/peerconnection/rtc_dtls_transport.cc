// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_dtls_transport.h"

#include <memory>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/dtls_transport_proxy.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_transport.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/webrtc/api/dtls_transport_interface.h"
#include "third_party/webrtc/api/peer_connection_interface.h"

namespace blink {

namespace {
String TransportStateToString(webrtc::DtlsTransportState state) {
  switch (state) {
    case webrtc::DtlsTransportState::kNew:
      return String("new");
      break;
    case webrtc::DtlsTransportState::kConnecting:
      return String("connecting");
      break;
    case webrtc::DtlsTransportState::kConnected:
      return String("connected");
      break;
    case webrtc::DtlsTransportState::kClosed:
      return String("closed");
      break;
    case webrtc::DtlsTransportState::kFailed:
      return String("failed");
      break;
    default:
      NOTREACHED();
      return String("failed");
      break;
  }
}

std::unique_ptr<DtlsTransportProxy> CreateProxy(
    ExecutionContext* context,
    webrtc::DtlsTransportInterface* native_transport,
    DtlsTransportProxy::Delegate* delegate) {
  LocalFrame* frame = To<Document>(context)->GetFrame();
  scoped_refptr<base::SingleThreadTaskRunner> proxy_thread =
      frame->GetTaskRunner(TaskType::kNetworking);
  scoped_refptr<base::SingleThreadTaskRunner> host_thread =
      Platform::Current()->GetWebRtcWorkerThread();
  return DtlsTransportProxy::Create(*frame, proxy_thread, host_thread,
                                    native_transport, delegate);
}

}  // namespace

RTCDtlsTransport::RTCDtlsTransport(
    ExecutionContext* context,
    rtc::scoped_refptr<webrtc::DtlsTransportInterface> native_transport,
    RTCIceTransport* ice_transport)
    : ContextClient(context),
      current_state_(webrtc::DtlsTransportState::kNew),
      native_transport_(native_transport),
      proxy_(CreateProxy(context, native_transport, this)),
      ice_transport_(ice_transport) {}

RTCDtlsTransport::~RTCDtlsTransport() {}

String RTCDtlsTransport::state() const {
  if (closed_from_owner_) {
    return TransportStateToString(webrtc::DtlsTransportState::kClosed);
  }
  return TransportStateToString(current_state_.state());
}

RTCIceTransport* RTCDtlsTransport::iceTransport() const {
  return ice_transport_;
}

webrtc::DtlsTransportInterface* RTCDtlsTransport::native_transport() {
  return native_transport_.get();
}

void RTCDtlsTransport::ChangeState(webrtc::DtlsTransportInformation info) {
  DCHECK(current_state_.state() != webrtc::DtlsTransportState::kClosed);
  current_state_ = info;
}

void RTCDtlsTransport::Close() {
  closed_from_owner_ = true;
  if (current_state_.state() != webrtc::DtlsTransportState::kClosed) {
    DispatchEvent(*Event::Create(event_type_names::kStatechange));
  }
  ice_transport_->stop();
}

// Implementation of DtlsTransportProxy::Delegate
void RTCDtlsTransport::OnStartCompleted(webrtc::DtlsTransportInformation info) {
  current_state_ = info;
}

void RTCDtlsTransport::OnStateChange(webrtc::DtlsTransportInformation info) {
  // We depend on closed only happening once for safe garbage collection.
  DCHECK(current_state_.state() != webrtc::DtlsTransportState::kClosed);
  current_state_ = info;
  if (!closed_from_owner_) {
    DispatchEvent(*Event::Create(event_type_names::kStatechange));
  }
}

const AtomicString& RTCDtlsTransport::InterfaceName() const {
  return event_target_names::kRTCDtlsTransport;
}

ExecutionContext* RTCDtlsTransport::GetExecutionContext() const {
  return ContextClient::GetExecutionContext();
}

void RTCDtlsTransport::Trace(Visitor* visitor) {
  visitor->Trace(ice_transport_);
  DtlsTransportProxy::Delegate::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
