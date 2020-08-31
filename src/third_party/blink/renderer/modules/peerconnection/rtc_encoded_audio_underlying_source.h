// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_AUDIO_UNDERLYING_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_AUDIO_UNDERLYING_SOURCE_H_

#include "base/threading/thread_checker.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace webrtc {
class TransformableFrameInterface;
}  // namespace webrtc

namespace blink {

class MODULES_EXPORT RTCEncodedAudioUnderlyingSource
    : public UnderlyingSourceBase {
 public:
  explicit RTCEncodedAudioUnderlyingSource(
      ScriptState*,
      base::OnceClosure disconnect_callback,
      bool is_receiver);

  // UnderlyingSourceBase
  ScriptPromise pull(ScriptState*) override;
  ScriptPromise Cancel(ScriptState*, ScriptValue reason) override;

  void OnFrameFromSource(std::unique_ptr<webrtc::TransformableFrameInterface>);
  void Close();

  void Trace(Visitor*) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(RTCEncodedAudioUnderlyingSourceTest,
                           QueuedFramesAreDroppedWhenOverflow);
  static const int kMinQueueDesiredSize;

  const Member<ScriptState> script_state_;
  base::OnceClosure disconnect_callback_;
  // Indicates if this source is for a receiver. Receiver sources
  // expose CSRCs.
  const bool is_receiver_;
  THREAD_CHECKER(thread_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_AUDIO_UNDERLYING_SOURCE_H_
