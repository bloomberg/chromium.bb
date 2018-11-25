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
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_adapter_cross_thread_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_adapter_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_proxy.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_candidate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_gather_options.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_ice_event.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_ice_event_init.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_transport.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/webrtc/api/jsepicecandidate.h"
#include "third_party/webrtc/api/peerconnectioninterface.h"
#include "third_party/webrtc/p2p/base/portallocator.h"
#include "third_party/webrtc/p2p/base/transportdescription.h"
#include "third_party/webrtc/pc/iceserverparsing.h"
#include "third_party/webrtc/pc/webrtcsdp.h"

namespace blink {

RTCDtlsTransport* RTCDtlsTransport::Create(ExecutionContext* context) {
  return new RTCDtlsTransport(context);
}

RTCDtlsTransport::RTCDtlsTransport(ExecutionContext* context)
    : ContextClient(context) {}

RTCDtlsTransport::~RTCDtlsTransport() {}

String RTCDtlsTransport::state() const {
  NOTIMPLEMENTED();
  return "";
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

const AtomicString& RTCDtlsTransport::InterfaceName() const {
  return event_target_names::kRTCDtlsTransport;
}

ExecutionContext* RTCDtlsTransport::GetExecutionContext() const {
  return ContextClient::GetExecutionContext();
}

void RTCDtlsTransport::Trace(Visitor* visitor) {
  visitor->Trace(remote_certificates_);
  EventTargetWithInlineData::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
