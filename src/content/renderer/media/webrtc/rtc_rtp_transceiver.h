// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_RTC_RTP_TRANSCEIVER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_RTC_RTP_TRANSCEIVER_H_

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "content/renderer/media/webrtc/rtc_rtp_receiver.h"
#include "content/renderer/media/webrtc/rtc_rtp_sender.h"
#include "content/renderer/media/webrtc/webrtc_media_stream_track_adapter_map.h"
#include "third_party/blink/public/platform/web_rtc_rtp_transceiver.h"
#include "third_party/webrtc/api/rtp_transceiver_interface.h"

namespace content {

// This class represents the state of a transceiver; a snapshot of what a
// webrtc-layer transceiver looked like when it was inspected on the signaling
// thread such that this information can be moved to the main thread in a single
// PostTask. It is used to surface state changes to make the blink-layer
// transceiver up-to-date.
//
// Blink objects live on the main thread and webrtc objects live on the
// signaling thread. If multiple asynchronous operations begin execution on the
// main thread they are posted and executed in order on the signaling thread.
// For example, operation A and operation B are called in JavaScript. When A is
// done on the signaling thread, webrtc object states will be updated. A
// callback is posted to the main thread so that blink objects can be updated to
// match the result of operation A. But if callback A tries to inspect the
// webrtc objects from the main thread this requires posting back to the
// signaling thread and waiting, which also includes waiting for the previously
// posted task: operation B. Inspecting the webrtc object like this does not
// guarantee you to get the state of operation A.
//
// As such, all state changes associated with an operation have to be surfaced
// in the same callback. This includes copying any states into a separate object
// so that it can be inspected on the main thread without any additional thread
// hops.
//
// The RtpTransceiverState is a snapshot of what the
// webrtc::RtpTransceiverInterface looked like when the RtpTransceiverState was
// created on the signaling thread. It also takes care of initializing sender
// and receiver states, including their track adapters such that we have access
// to a blink track corresponding to the webrtc tracks of the sender and
// receiver.
//
// Except for initialization logic and operator=(), the RtpTransceiverState is
// immutable and only accessible on the main thread.
//
// TODO(hbos): [Onion Soup] When the transceiver implementation is moved to
// blink this will be part of the blink transceiver instead of the content
// transceiver. https://crbug.com/787254
class CONTENT_EXPORT RtpTransceiverState {
 public:
  RtpTransceiverState(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner,
      scoped_refptr<webrtc::RtpTransceiverInterface> webrtc_transceiver,
      base::Optional<RtpSenderState> sender_state,
      base::Optional<RtpReceiverState> receiver_state,
      base::Optional<std::string> mid,
      bool stopped,
      webrtc::RtpTransceiverDirection direction,
      base::Optional<webrtc::RtpTransceiverDirection> current_direction,
      base::Optional<webrtc::RtpTransceiverDirection> fired_direction);
  RtpTransceiverState(RtpTransceiverState&&);
  RtpTransceiverState(const RtpTransceiverState&) = delete;
  ~RtpTransceiverState();

  // This is intended to be used for moving the object from the signaling thread
  // to the main thread and as such has no thread checks. Once moved to the main
  // this should only be invoked on the main thread.
  RtpTransceiverState& operator=(RtpTransceiverState&&);
  RtpTransceiverState& operator=(const RtpTransceiverState&) = delete;

  bool is_initialized() const;
  void Initialize();

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner() const;
  scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner() const;
  scoped_refptr<webrtc::RtpTransceiverInterface> webrtc_transceiver() const;
  const base::Optional<RtpSenderState>& sender_state() const;
  RtpSenderState MoveSenderState();
  const base::Optional<RtpReceiverState>& receiver_state() const;
  RtpReceiverState MoveReceiverState();
  base::Optional<std::string> mid() const;
  bool stopped() const;
  webrtc::RtpTransceiverDirection direction() const;
  void set_direction(webrtc::RtpTransceiverDirection);
  base::Optional<webrtc::RtpTransceiverDirection> current_direction() const;
  base::Optional<webrtc::RtpTransceiverDirection> fired_direction() const;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner_;
  scoped_refptr<webrtc::RtpTransceiverInterface> webrtc_transceiver_;
  bool is_initialized_;
  base::Optional<RtpSenderState> sender_state_;
  base::Optional<RtpReceiverState> receiver_state_;
  base::Optional<std::string> mid_;
  bool stopped_;
  webrtc::RtpTransceiverDirection direction_;
  base::Optional<webrtc::RtpTransceiverDirection> current_direction_;
  base::Optional<webrtc::RtpTransceiverDirection> fired_direction_;
};

// Used to surface |webrtc::RtpTransceiverInterface| to blink. Multiple
// |RTCRtpTransceiver|s could reference the same webrtc transceiver; |id| is
// unique per webrtc transceiver.
// Its methods are accessed on the main thread, internally also performs
// operations on the signaling thread.
// TODO(hbos): [Onion Soup] Remove the content layer versions of this class and
// rely on webrtc directly from blink. Requires coordination with senders and
// receivers. https://crbug.com/787254
class CONTENT_EXPORT RTCRtpTransceiver : public blink::WebRTCRtpTransceiver {
 public:
  static uintptr_t GetId(
      const webrtc::RtpTransceiverInterface* webrtc_transceiver);

  RTCRtpTransceiver(
      scoped_refptr<webrtc::PeerConnectionInterface> native_peer_connection,
      scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_map,
      RtpTransceiverState state);
  RTCRtpTransceiver(const RTCRtpTransceiver& other);
  ~RTCRtpTransceiver() override;

  RTCRtpTransceiver& operator=(const RTCRtpTransceiver& other);
  std::unique_ptr<RTCRtpTransceiver> ShallowCopy() const;

  const RtpTransceiverState& state() const;
  void set_state(RtpTransceiverState state);
  RTCRtpSender* content_sender();
  RTCRtpReceiver* content_receiver();

  blink::WebRTCRtpTransceiverImplementationType ImplementationType()
      const override;
  uintptr_t Id() const override;
  blink::WebString Mid() const override;
  std::unique_ptr<blink::WebRTCRtpSender> Sender() const override;
  std::unique_ptr<blink::WebRTCRtpReceiver> Receiver() const override;
  bool Stopped() const override;
  webrtc::RtpTransceiverDirection Direction() const override;
  void SetDirection(webrtc::RtpTransceiverDirection direction) override;
  base::Optional<webrtc::RtpTransceiverDirection> CurrentDirection()
      const override;
  base::Optional<webrtc::RtpTransceiverDirection> FiredDirection()
      const override;

 private:
  class RTCRtpTransceiverInternal;
  struct RTCRtpTransceiverInternalTraits;

  scoped_refptr<RTCRtpTransceiverInternal> internal_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_RTC_RTP_TRANSCEIVER_H_
