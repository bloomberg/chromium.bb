// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_underlying_source.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

// Frames should not be queued at all. We allow queuing a few frames to deal
// with transient slowdowns. Specified as a negative number of frames since
// queuing is reported by the stream controller as a negative desired size.
const int RTCEncodedAudioUnderlyingSource::kMinQueueDesiredSize = -60;

RTCEncodedAudioUnderlyingSource::RTCEncodedAudioUnderlyingSource(
    ScriptState* script_state,
    base::OnceClosure disconnect_callback,
    bool is_receiver)
    : UnderlyingSourceBase(script_state),
      script_state_(script_state),
      disconnect_callback_(std::move(disconnect_callback)),
      is_receiver_(is_receiver) {
  DCHECK(disconnect_callback_);
}

ScriptPromise RTCEncodedAudioUnderlyingSource::pull(ScriptState* script_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // WebRTC is a push source without backpressure support, so nothing to do
  // here.
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise RTCEncodedAudioUnderlyingSource::Cancel(ScriptState* script_state,
                                                      ScriptValue reason) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (disconnect_callback_)
    std::move(disconnect_callback_).Run();
  return ScriptPromise::CastUndefined(script_state);
}

void RTCEncodedAudioUnderlyingSource::Trace(Visitor* visitor) {
  visitor->Trace(script_state_);
  UnderlyingSourceBase::Trace(visitor);
}

void RTCEncodedAudioUnderlyingSource::OnFrameFromSource(
    std::unique_ptr<webrtc::TransformableFrameInterface> webrtc_frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // If the source is canceled or there are too many queued frames,
  // drop the new frame.
  if (!disconnect_callback_ || !Controller() ||
      Controller()->DesiredSize() <= kMinQueueDesiredSize) {
    return;
  }

  RTCEncodedAudioFrame* encoded_frame = nullptr;
  if (is_receiver_) {
    // Receivers produce frames as webrtc::TransformableAudioFrameInterface,
    // which allows exposing the CSRCs.
    std::unique_ptr<webrtc::TransformableAudioFrameInterface> audio_frame =
        base::WrapUnique(static_cast<webrtc::TransformableAudioFrameInterface*>(
            webrtc_frame.release()));
    encoded_frame =
        MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(audio_frame));
  } else {
    encoded_frame =
        MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(webrtc_frame));
  }
  Controller()->Enqueue(encoded_frame);
}

void RTCEncodedAudioUnderlyingSource::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (disconnect_callback_)
    std::move(disconnect_callback_).Run();

  if (Controller())
    Controller()->Close();
}

}  // namespace blink
