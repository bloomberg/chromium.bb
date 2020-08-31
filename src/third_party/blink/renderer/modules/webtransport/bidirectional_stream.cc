// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/bidirectional_stream.h"

#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "third_party/blink/renderer/modules/webtransport/quic_transport.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

BidirectionalStream::BidirectionalStream(
    ScriptState* script_state,
    QuicTransport* quic_transport,
    uint32_t stream_id,
    mojo::ScopedDataPipeProducerHandle outgoing_producer,
    mojo::ScopedDataPipeConsumerHandle incoming_consumer)
    : outgoing_stream_(
          MakeGarbageCollected<OutgoingStream>(script_state,
                                               this,
                                               std::move(outgoing_producer))),
      incoming_stream_(MakeGarbageCollected<IncomingStream>(
          script_state,
          WTF::Bind(&BidirectionalStream::OnIncomingStreamAbort,
                    WrapWeakPersistent(this)),
          std::move(incoming_consumer))),
      quic_transport_(quic_transport),
      stream_id_(stream_id) {}

void BidirectionalStream::Init() {
  outgoing_stream_->Init();
  incoming_stream_->Init();
}

void BidirectionalStream::OnIncomingStreamClosed(bool fin_received) {
  incoming_stream_->OnIncomingStreamClosed(fin_received);
  // TODO(ricea): Review this behaviour when adding detail to the specification.
  if (!sent_fin_) {
    outgoing_stream_->Reset();
  }
}

void BidirectionalStream::Reset() {
  outgoing_stream_->Reset();
  incoming_stream_->Reset();
}

void BidirectionalStream::ContextDestroyed() {
  outgoing_stream_->ContextDestroyed();
  incoming_stream_->ContextDestroyed();
}

void BidirectionalStream::SendFin() {
  quic_transport_->SendFin(stream_id_);
  sent_fin_ = true;
  // The IncomingStream will be closed on the network service side.
}

void BidirectionalStream::OnOutgoingStreamAbort() {
  DCHECK(!sent_fin_);
  quic_transport_->ForgetStream(stream_id_);
  incoming_stream_->Reset();
}

void BidirectionalStream::Trace(Visitor* visitor) {
  visitor->Trace(outgoing_stream_);
  visitor->Trace(incoming_stream_);
  visitor->Trace(quic_transport_);
  ScriptWrappable::Trace(visitor);
  WebTransportStream::Trace(visitor);
  OutgoingStream::Client::Trace(visitor);
}

void BidirectionalStream::OnIncomingStreamAbort() {
  quic_transport_->ForgetStream(stream_id_);
  outgoing_stream_->Reset();
}

}  // namespace blink
