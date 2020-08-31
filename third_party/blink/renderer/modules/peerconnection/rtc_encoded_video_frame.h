// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_VIDEO_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_VIDEO_FRAME_H_

#include <stdint.h>

#include <memory>

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace webrtc {
class TransformableVideoFrameInterface;
}  // namespace webrtc

namespace blink {

class DOMArrayBuffer;
class RTCEncodedVideoFrameDelegate;
class RTCEncodedVideoFrameMetadata;

class MODULES_EXPORT RTCEncodedVideoFrame final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit RTCEncodedVideoFrame(
      std::unique_ptr<webrtc::TransformableVideoFrameInterface> webrtc_frame);
  explicit RTCEncodedVideoFrame(
      scoped_refptr<RTCEncodedVideoFrameDelegate> delegate);

  // rtc_encoded_video_frame.idl implementation.
  String type() const;
  // Returns the RTP Packet Timestamp for this frame.
  uint64_t timestamp() const;
  DOMArrayBuffer* data() const;
  RTCEncodedVideoFrameMetadata* getMetadata() const;
  DOMArrayBuffer* additionalData() const;
  void setData(DOMArrayBuffer*);
  uint32_t synchronizationSource() const;
  String toString() const;

  scoped_refptr<RTCEncodedVideoFrameDelegate> Delegate() const;
  void SyncDelegate() const;

  // Returns and transfers ownership of the internal WebRTC frame
  // backing this RTCEncodedVideoFrame, neutering all RTCEncodedVideoFrames
  // backed by that internal WebRTC frame.
  std::unique_ptr<webrtc::TransformableVideoFrameInterface> PassWebRtcFrame();

  void Trace(Visitor*) override;

 private:
  const scoped_refptr<RTCEncodedVideoFrameDelegate> delegate_;

  // Exposes encoded frame data from |delegate_|.
  mutable Member<DOMArrayBuffer> frame_data_;
  // Exposes additional data from |delegate_|.
  mutable Member<DOMArrayBuffer> additional_data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_VIDEO_FRAME_H_
