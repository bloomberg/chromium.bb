// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_PEER_CONNECTION_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_PEER_CONNECTION_HANDLER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/peerconnection/peer_connection_tracker.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/peerconnection/media_stream_track_metrics.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_receiver_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_sender_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/speed_limit_uma_listener.h"
#include "third_party/blink/renderer/modules/peerconnection/thermal_resource.h"
#include "third_party/blink/renderer/modules/peerconnection/thermal_uma_listener.h"
#include "third_party/blink/renderer/modules/peerconnection/transceiver_state_surfacer.h"
#include "third_party/blink/renderer/modules/peerconnection/webrtc_media_stream_track_adapter_map.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_peer_connection_handler_client.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_session_description_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_session_description_request.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_stats_request.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_stats_response_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/api/rtc_error.h"
#include "third_party/webrtc/api/rtp_transceiver_interface.h"
#include "third_party/webrtc/api/stats/rtc_stats.h"
#include "third_party/webrtc/api/stats/rtc_stats_collector_callback.h"

namespace blink {
class PeerConnectionDependencyFactory;
class PeerConnectionTracker;
class RTCAnswerOptionsPlatform;
class RTCLegacyStats;
class RTCOfferOptionsPlatform;
class RTCPeerConnectionHandlerClient;
class RTCSessionDescriptionInit;
class RTCVoidRequest;
class SetLocalDescriptionRequest;
class WebLocalFrame;

// Helper class for passing pre-parsed session descriptions to functions.
// Create a ParsedSessionDescription by calling one of the Parse functions.
// The function allows you to access its input SDP string, as well as the
// parsed form. If parse failed, description() returns null, and error()
// returns an error description. The class can't be modified or reused,
// but can be neutered by calling release().
class MODULES_EXPORT ParsedSessionDescription {
 public:
  static ParsedSessionDescription Parse(const RTCSessionDescriptionInit*);
  static ParsedSessionDescription Parse(const RTCSessionDescriptionPlatform*);
  static ParsedSessionDescription Parse(const String& type, const String& sdp);

  // Moveable, but not copyable.
  ParsedSessionDescription(ParsedSessionDescription&& other) = default;
  ParsedSessionDescription& operator=(ParsedSessionDescription&& other) =
      default;
  ParsedSessionDescription(const ParsedSessionDescription&) = delete;
  ParsedSessionDescription operator=(const ParsedSessionDescription&) = delete;

  const webrtc::SessionDescriptionInterface* description() const {
    return description_.get();
  }
  const webrtc::SdpParseError& error() const { return error_; }
  const String& type() const { return type_; }
  const String& sdp() const { return sdp_; }

  std::unique_ptr<webrtc::SessionDescriptionInterface> release() {
    return std::unique_ptr<webrtc::SessionDescriptionInterface>(
        description_.release());
  }

 protected:
  // The constructor will not parse the SDP.
  // It is protected, not private, in order to allow it to be used to construct
  // mock objects.
  ParsedSessionDescription(const String& type, const String& sdp);
  // The mock object also needs access to the description.
  std::unique_ptr<webrtc::SessionDescriptionInterface> description_;

 private:
  // Does the actual parsing. Only called from the static Parse methods.
  void DoParse();

  String type_;
  String sdp_;

  webrtc::SdpParseError error_;
};

// Mockable wrapper for blink::RTCStatsResponseBase
class MODULES_EXPORT LocalRTCStatsResponse : public rtc::RefCountInterface {
 public:
  explicit LocalRTCStatsResponse(RTCStatsResponseBase* impl) : impl_(impl) {}

  virtual RTCStatsResponseBase* webKitStatsResponse() const;
  virtual void addStats(const RTCLegacyStats& stats);

 protected:
  ~LocalRTCStatsResponse() override {}
  // Constructor for creating mocks.
  LocalRTCStatsResponse() {}

 private:
  Persistent<RTCStatsResponseBase> impl_;
};

// Mockable wrapper for RTCStatsRequest
class MODULES_EXPORT LocalRTCStatsRequest : public rtc::RefCountInterface {
 public:
  explicit LocalRTCStatsRequest(RTCStatsRequest* impl);
  // Constructor for testing.
  LocalRTCStatsRequest();

  virtual bool hasSelector() const;
  virtual MediaStreamComponent* component() const;
  virtual void requestSucceeded(const LocalRTCStatsResponse* response);
  virtual scoped_refptr<LocalRTCStatsResponse> createResponse();

 protected:
  ~LocalRTCStatsRequest() override;

 private:
  CrossThreadPersistent<RTCStatsRequest> impl_;
};

// RTCPeerConnectionHandler is a delegate for the RTC PeerConnection API
// messages going between WebKit and native PeerConnection in libjingle. It's
// owned by WebKit.
// WebKit calls all of these methods on the main render thread.
// Callbacks to the webrtc::PeerConnectionObserver implementation also occur on
// the main render thread.
class MODULES_EXPORT RTCPeerConnectionHandler {
 public:
  enum class IceConnectionStateVersion {
    // Only applicable in Unified Plan when the JavaScript-exposed
    // iceConnectionState is calculated in blink. In this case, kLegacy is used
    // to report the webrtc::PeerConnectionInterface implementation which is not
    // visible in JavaScript, but still useful to track for debugging purposes.
    kLegacy,
    // The JavaScript-visible iceConnectionState. In Plan B, this is the same as
    // the webrtc::PeerConnectionInterface implementation.
    kDefault,
  };

  RTCPeerConnectionHandler(
      RTCPeerConnectionHandlerClient* client,
      blink::PeerConnectionDependencyFactory* dependency_factory,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      bool force_encoded_audio_insertable_streams,
      bool force_encoded_video_insertable_streams);

  RTCPeerConnectionHandler(const RTCPeerConnectionHandler&) = delete;
  RTCPeerConnectionHandler& operator=(const RTCPeerConnectionHandler&) = delete;

  virtual ~RTCPeerConnectionHandler();

  // Initialize method only used for unit test.
  bool InitializeForTest(
      const webrtc::PeerConnectionInterface::RTCConfiguration&
          server_configuration,
      const MediaConstraints& options,
      PeerConnectionTracker* peer_connection_tracker,
      ExceptionState& exception_state);

  // RTCPeerConnectionHandlerPlatform implementation
  virtual bool Initialize(
      const webrtc::PeerConnectionInterface::RTCConfiguration&
          server_configuration,
      const MediaConstraints& options,
      WebLocalFrame* web_frame,
      ExceptionState& exception_state);

  virtual void Close();
  virtual void CloseAndUnregister();

  virtual Vector<std::unique_ptr<RTCRtpTransceiverPlatform>> CreateOffer(
      RTCSessionDescriptionRequest* request,
      const MediaConstraints& options);
  virtual Vector<std::unique_ptr<RTCRtpTransceiverPlatform>> CreateOffer(
      RTCSessionDescriptionRequest* request,
      RTCOfferOptionsPlatform* options);

  virtual void CreateAnswer(blink::RTCSessionDescriptionRequest* request,
                            const MediaConstraints& options);
  virtual void CreateAnswer(blink::RTCSessionDescriptionRequest* request,
                            blink::RTCAnswerOptionsPlatform* options);

  virtual void SetLocalDescription(blink::RTCVoidRequest* request);
  // Set local and remote description.
  // The parsed_sdp argument must be passed in with std::move.
  virtual void SetLocalDescription(blink::RTCVoidRequest* request,
                                   ParsedSessionDescription parsed_sdp);
  virtual void SetRemoteDescription(blink::RTCVoidRequest* request,
                                    ParsedSessionDescription parsed_sdp);

  virtual const webrtc::PeerConnectionInterface::RTCConfiguration&
  GetConfiguration() const;
  virtual webrtc::RTCErrorType SetConfiguration(
      const webrtc::PeerConnectionInterface::RTCConfiguration& configuration);
  virtual void AddIceCandidate(blink::RTCVoidRequest* request,
                               RTCIceCandidatePlatform* candidate);
  virtual void RestartIce();

  virtual void GetStats(RTCStatsRequest* request);
  virtual void GetStats(
      RTCStatsReportCallback callback,
      const Vector<webrtc::NonStandardGroupId>& exposed_group_ids);
  virtual webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>>
  AddTransceiverWithTrack(MediaStreamComponent* component,
                          const webrtc::RtpTransceiverInit& init);
  virtual webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>>
  AddTransceiverWithKind(const String& kind,
                         const webrtc::RtpTransceiverInit& init);
  virtual webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>>
  AddTrack(MediaStreamComponent* component,
           const MediaStreamDescriptorVector& descriptors);
  virtual webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>>
  RemoveTrack(blink::RTCRtpSenderPlatform* web_sender);

  virtual scoped_refptr<webrtc::DataChannelInterface> CreateDataChannel(
      const String& label,
      const webrtc::DataChannelInit& init);
  virtual webrtc::PeerConnectionInterface* NativePeerConnection();
  virtual void RunSynchronousOnceClosureOnSignalingThread(
      CrossThreadOnceClosure closure,
      const char* trace_event_name);
  virtual void RunSynchronousOnceClosureOnSignalingThread(
      base::OnceClosure closure,
      const char* trace_event_name);

  virtual void TrackIceConnectionStateChange(
      RTCPeerConnectionHandler::IceConnectionStateVersion version,
      webrtc::PeerConnectionInterface::IceConnectionState state);

  // Delegate functions to allow for mocking of WebKit interfaces.
  // getStats takes ownership of request parameter.
  virtual void getStats(const scoped_refptr<LocalRTCStatsRequest>& request);

  // Asynchronously calls native_peer_connection_->getStats on the signaling
  // thread.
  void GetStandardStatsForTracker(
      scoped_refptr<webrtc::RTCStatsCollectorCallback> observer);
  void GetStats(webrtc::StatsObserver* observer,
                webrtc::PeerConnectionInterface::StatsOutputLevel level,
                rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> selector);

  // Tells the |client_| to close RTCPeerConnection.
  // Make it virtual for testing purpose.
  virtual void CloseClientPeerConnection();
  // Invoked when a new thermal state is received from the OS.
  // Virtual for testing purposes.
  virtual void OnThermalStateChange(
      mojom::blink::DeviceThermalState thermal_state);
  // Invoked when a new CPU speed limit is received from the OS.
  virtual void OnSpeedLimitChange(int32_t speed_limit);

  // Start recording an event log.
  void StartEventLog(int output_period_ms);
  // Stop recording an event log.
  void StopEventLog();

  // WebRTC event log fragments sent back from PeerConnection land here.
  void OnWebRtcEventLogWrite(const WTF::Vector<uint8_t>& output);

  // Virtual for testing purposes.
  virtual scoped_refptr<base::SingleThreadTaskRunner> signaling_thread() const;

  bool force_encoded_audio_insertable_streams() {
    return force_encoded_audio_insertable_streams_;
  }

  bool force_encoded_video_insertable_streams() {
    return force_encoded_video_insertable_streams_;
  }

 protected:
  // Constructor to be used for constructing mocks only.
  explicit RTCPeerConnectionHandler(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  webrtc::PeerConnectionInterface* native_peer_connection() {
    return native_peer_connection_.get();
  }

  class Observer;
  friend class Observer;
  class WebRtcSetDescriptionObserverImpl;
  friend class WebRtcSetDescriptionObserverImpl;
  class SetLocalDescriptionRequest;
  friend class SetLocalDescriptionRequest;

  void OnSessionDescriptionsUpdated(
      std::unique_ptr<webrtc::SessionDescriptionInterface>
          pending_local_description,
      std::unique_ptr<webrtc::SessionDescriptionInterface>
          current_local_description,
      std::unique_ptr<webrtc::SessionDescriptionInterface>
          pending_remote_description,
      std::unique_ptr<webrtc::SessionDescriptionInterface>
          current_remote_description);
  void TrackSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state);
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state);
  void OnStandardizedIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state);
  void OnConnectionChange(
      webrtc::PeerConnectionInterface::PeerConnectionState new_state);
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state);
  void OnNegotiationNeededEvent(uint32_t event_id);
  void OnReceiversModifiedPlanB(
      webrtc::PeerConnectionInterface::SignalingState signaling_state,
      Vector<blink::RtpReceiverState> receiver_state,
      Vector<uintptr_t> receiver_id);
  void OnModifySctpTransport(blink::WebRTCSctpTransportSnapshot state);
  void OnModifyTransceivers(
      webrtc::PeerConnectionInterface::SignalingState signaling_state,
      std::vector<blink::RtpTransceiverState> transceiver_states,
      bool is_remote_description);
  void OnDataChannel(scoped_refptr<webrtc::DataChannelInterface> channel);
  void OnIceCandidate(const String& sdp,
                      const String& sdp_mid,
                      int sdp_mline_index,
                      int component,
                      int address_family);
  void OnIceCandidateError(const String& address,
                           absl::optional<uint16_t> port,
                           const String& host_candidate,
                           const String& url,
                           int error_code,
                           const String& error_text);
  void OnInterestingUsage(int usage_pattern);

  // Protected for testing.
  ThermalUmaListener* thermal_uma_listener() const;
  SpeedLimitUmaListener* speed_limit_uma_listener() const;

 private:
  // Record info about the first SessionDescription from the local and
  // remote side to record UMA stats once both are set.
  struct FirstSessionDescription {
    explicit FirstSessionDescription(
        const webrtc::SessionDescriptionInterface* desc);

    bool audio = false;
    bool video = false;
    // If audio or video will use RTCP-mux (if there is no audio or
    // video, then false).
    bool rtcp_mux = false;
  };

  RTCSessionDescriptionPlatform*
  GetRTCSessionDescriptionPlatformOnSignalingThread(
      CrossThreadOnceFunction<const webrtc::SessionDescriptionInterface*()>
          description_cb,
      const char* log_text);

  // Report to UMA whether an IceConnectionState has occurred. It only records
  // the first occurrence of a given state.
  void ReportICEState(
      webrtc::PeerConnectionInterface::IceConnectionState new_state);

  // Reset UMA related members to the initial state. This is invoked at the
  // constructor as well as after Ice Restart.
  void ResetUMAStats();

  void ReportFirstSessionDescriptions(const FirstSessionDescription& local,
                                      const FirstSessionDescription& remote);

  void AddTransceiverWithTrackOnSignalingThread(
      webrtc::MediaStreamTrackInterface* webrtc_track,
      webrtc::RtpTransceiverInit init,
      blink::TransceiverStateSurfacer* transceiver_state_surfacer,
      webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>*
          error_or_transceiver);
  void AddTransceiverWithMediaTypeOnSignalingThread(
      cricket::MediaType media_type,
      webrtc::RtpTransceiverInit init,
      blink::TransceiverStateSurfacer* transceiver_state_surfacer,
      webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>*
          error_or_transceiver);
  void AddTrackOnSignalingThread(
      webrtc::MediaStreamTrackInterface* track,
      std::vector<std::string> stream_ids,
      blink::TransceiverStateSurfacer* transceiver_state_surfacer,
      webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpSenderInterface>>*
          error_or_sender);
  bool RemoveTrackPlanB(blink::RTCRtpSenderPlatform* web_sender);
  webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>>
  RemoveTrackUnifiedPlan(blink::RTCRtpSenderPlatform* web_sender);
  // Helper function to remove a track on the signaling thread.
  // Updates the entire transceiver state.
  // The result will be absl::nullopt if the operation is cancelled,
  // and no change to the state will be made.
  void RemoveTrackUnifiedPlanOnSignalingThread(
      webrtc::RtpSenderInterface* sender,
      blink::TransceiverStateSurfacer* transceiver_state_surfacer,
      absl::optional<webrtc::RTCError>* result);
  Vector<std::unique_ptr<RTCRtpTransceiverPlatform>> CreateOfferInternal(
      blink::RTCSessionDescriptionRequest* request,
      webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options);
  void CreateOfferOnSignalingThread(
      webrtc::CreateSessionDescriptionObserver* observer,
      webrtc::PeerConnectionInterface::RTCOfferAnswerOptions offer_options,
      blink::TransceiverStateSurfacer* transceiver_state_surfacer);
  std::vector<std::unique_ptr<blink::RTCRtpSenderImpl>>::iterator FindSender(
      uintptr_t id);
  std::vector<std::unique_ptr<blink::RTCRtpReceiverImpl>>::iterator
  FindReceiver(uintptr_t id);
  std::vector<std::unique_ptr<blink::RTCRtpTransceiverImpl>>::iterator
  FindTransceiver(uintptr_t id);
  // For full transceiver implementations, returns the index of
  // |rtp_transceivers_| that correspond to |platform_transceiver|.
  // For sender-only transceiver implementations, returns the index of
  // |rtp_senders_| that correspond to |platform_transceiver.Sender()|.
  // For receiver-only transceiver implementations, returns the index of
  // |rtp_receivers_| that correspond to |platform_transceiver.Receiver()|.
  // NOTREACHED()-crashes if no correspondent is found.
  size_t GetTransceiverIndex(
      const RTCRtpTransceiverPlatform& platform_transceiver);
  std::unique_ptr<blink::RTCRtpTransceiverImpl> CreateOrUpdateTransceiver(
      blink::RtpTransceiverState transceiver_state,
      blink::TransceiverStateUpdateMode update_mode);
  void MaybeCreateThermalUmaListner();

  // Initialize() is never expected to be called more than once, even if the
  // first call fails.
  bool initialize_called_ = false;

  // |client_| is a raw pointer to the blink object (blink::RTCPeerConnection)
  // that owns this object.
  // It is valid for the lifetime of this object, but is cleared when
  // CloseAndUnregister() is called, in order to make sure it doesn't
  // interfere with garbage collection of the owner object.
  RTCPeerConnectionHandlerClient* client_ = nullptr;
  // True if this PeerConnection has been closed.
  // After the PeerConnection has been closed, this object may no longer
  // forward callbacks to blink.
  bool is_closed_ = false;
  // True if CloseAndUnregister has been called.
  bool is_unregistered_ = false;

  // Transition from kHaveLocalOffer to kHaveRemoteOffer indicates implicit
  // rollback in which case we need to also make visiting of kStable observable.
  webrtc::PeerConnectionInterface::SignalingState previous_signaling_state_ =
      webrtc::PeerConnectionInterface::kStable;

  // Will be reset to nullptr when the handler is `CloseAndUnregister()`-ed, so
  // it doesn't prevent the factory from being garbage-collected.
  Persistent<PeerConnectionDependencyFactory> dependency_factory_;

  blink::WebLocalFrame* frame_ = nullptr;

  // Map and owners of track adapters. Every track that is in use by the peer
  // connection has an associated blink and webrtc layer representation of it.
  // The map keeps track of the relationship between
  // |MediaStreamComponent|s and |webrtc::MediaStreamTrackInterface|s.
  // Track adapters are created on the fly when a component (such as a stream)
  // needs to reference it, and automatically disposed when there are no longer
  // any components referencing it.
  scoped_refptr<blink::WebRtcMediaStreamTrackAdapterMap> track_adapter_map_;
  // In Plan B, senders and receivers are added or removed independently of one
  // another. In Unified Plan, senders and receivers are created in pairs as
  // transceivers. Transceivers may become inactive, but are never removed.
  // TODO(hbos): Implement transceiver behaviors. https://crbug.com/777617
  // Blink layer correspondents of |webrtc::RtpSenderInterface|.
  std::vector<std::unique_ptr<blink::RTCRtpSenderImpl>> rtp_senders_;
  // Blink layer correspondents of |webrtc::RtpReceiverInterface|.
  std::vector<std::unique_ptr<blink::RTCRtpReceiverImpl>> rtp_receivers_;
  // Blink layer correspondents of |webrtc::RtpTransceiverInterface|.
  std::vector<std::unique_ptr<blink::RTCRtpTransceiverImpl>> rtp_transceivers_;
  // A snapshot of transceiver ids taken before the last transition, used to
  // detect any removals during rollback.
  Vector<uintptr_t> previous_transceiver_ids_;

  WeakPersistent<PeerConnectionTracker> peer_connection_tracker_;

  MediaStreamTrackMetrics track_metrics_;

  // Counter for a UMA stat reported at destruction time.
  int num_data_channels_created_ = 0;

  // Counter for number of IPv4 and IPv6 local candidates.
  int num_local_candidates_ipv4_ = 0;
  int num_local_candidates_ipv6_ = 0;

  // To make sure the observers are released after native_peer_connection_,
  // they have to come first.
  CrossThreadPersistent<Observer> peer_connection_observer_;

  // |native_peer_connection_| is the libjingle native PeerConnection object.
  scoped_refptr<webrtc::PeerConnectionInterface> native_peer_connection_;

  // The last applied configuration. Used so that the constraints
  // used when constructing the PeerConnection carry over when
  // SetConfiguration is called.
  webrtc::PeerConnectionInterface::RTCConfiguration configuration_;
  bool force_encoded_audio_insertable_streams_ = false;
  bool force_encoded_video_insertable_streams_ = false;

  // Resources for Adaptation.
  // The Thermal Resource is lazily instantiated on platforms where thermal
  // signals are supported.
  scoped_refptr<ThermalResource> thermal_resource_;
  // ThermalUmaListener is only tracked on peer connection that add a track.
  std::unique_ptr<ThermalUmaListener> thermal_uma_listener_;
  mojom::blink::DeviceThermalState last_thermal_state_ =
      mojom::blink::DeviceThermalState::kUnknown;
  // Last received OS speed limit.
  int32_t last_speed_limit_ = mojom::blink::kSpeedLimitMax;
  // SpeedLimitUmaListener is only tracked on peer connection that add an audio
  // or video track.
  std::unique_ptr<SpeedLimitUmaListener> speed_limit_uma_listener_;

  // Record info about the first SessionDescription from the local and
  // remote side to record UMA stats once both are set.  We only check
  // for the first offer or answer.  "pranswer"s and "unknown"s (from
  // unit tests) are ignored.
  std::unique_ptr<FirstSessionDescription> first_local_description_;
  std::unique_ptr<FirstSessionDescription> first_remote_description_;

  base::TimeTicks ice_connection_checking_start_;

  // Track which ICE Connection state that this PeerConnection has gone through.
  bool ice_state_seen_[webrtc::PeerConnectionInterface::kIceConnectionMax] = {};

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<RTCPeerConnectionHandler> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_PEER_CONNECTION_HANDLER_H_
