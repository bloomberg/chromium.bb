// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_dtls_transport.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/dtls_transport_proxy.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_adapter_cross_thread_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_adapter_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_candidate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_gather_options.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_ice_event.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_ice_event_init.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_transport.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/webrtc/api/dtls_transport_interface.h"
#include "third_party/webrtc/api/jsep_ice_candidate.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/p2p/base/port_allocator.h"
#include "third_party/webrtc/p2p/base/transport_description.h"
#include "third_party/webrtc/pc/ice_server_parsing.h"
#include "third_party/webrtc/pc/webrtc_sdp.h"

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
    rtc::scoped_refptr<webrtc::DtlsTransportInterface> native_transport)
    : ContextClient(context),
      current_state_(webrtc::DtlsTransportState::kNew),
      native_transport_(native_transport),
      proxy_(CreateProxy(context, native_transport, this)) {}

RTCDtlsTransport::~RTCDtlsTransport() {}

String RTCDtlsTransport::state() const {
  return TransportStateToString(current_state_.state());
}

const HeapVector<Member<DOMArrayBuffer>>&
RTCDtlsTransport::getRemoteCertificates() const {
  return remote_certificates_;
}

RTCIceTransport* RTCDtlsTransport::iceTransport() const {
  // TODO(crbug.com/907849): Implement returning an IceTransport
  NOTIMPLEMENTED();
  return nullptr;
}

webrtc::DtlsTransportInterface* RTCDtlsTransport::native_transport() {
  return native_transport_.get();
}

// Implementation of DtlsTransportProxy::Delegate
void RTCDtlsTransport::OnStartCompleted(webrtc::DtlsTransportInformation info) {
  current_state_ = info;
}

void RTCDtlsTransport::OnStateChange(webrtc::DtlsTransportInformation info) {
  // We depend on closed only happening once for safe garbage collection.
  DCHECK(current_state_.state() != webrtc::DtlsTransportState::kClosed);
  current_state_ = info;
  DispatchEvent(*Event::Create(event_type_names::kStatechange));
}

const AtomicString& RTCDtlsTransport::InterfaceName() const {
  return event_target_names::kRTCDtlsTransport;
}

ExecutionContext* RTCDtlsTransport::GetExecutionContext() const {
  return ContextClient::GetExecutionContext();
}

bool RTCDtlsTransport::HasPendingActivity() const {
  // We have to keep the RTCDtlsTransport alive while new notifications
  // may arrive.
  // The closed state is final, so no more events will happen after
  // seeing that state.
  return current_state_.state() != webrtc::DtlsTransportState::kClosed;
}

void RTCDtlsTransport::Trace(Visitor* visitor) {
  visitor->Trace(remote_certificates_);
  EventTargetWithInlineData::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
