/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RTP_TRANSPORT_CONTROLLER_SEND_H_
#define CALL_RTP_TRANSPORT_CONTROLLER_SEND_H_

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/network_state_predictor.h"
#include "api/transport/network_control.h"
#include "call/rtp_bitrate_configurator.h"
#include "call/rtp_transport_controller_send_interface.h"
#include "call/rtp_video_sender.h"
#include "modules/congestion_controller/rtp/control_handler.h"
#include "modules/congestion_controller/rtp/transport_feedback_adapter.h"
#include "modules/pacing/packet_router.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/network_route.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/task_utils/repeating_task.h"

namespace webrtc {
class Clock;
class FrameEncryptorInterface;
class RtcEventLog;

// TODO(nisse): When we get the underlying transports here, we should
// have one object implementing RtpTransportControllerSendInterface
// per transport, sharing the same congestion controller.
class RtpTransportControllerSend final
    : public RtpTransportControllerSendInterface,
      public RtcpBandwidthObserver,
      public TransportFeedbackObserver {
 public:
  RtpTransportControllerSend(
      Clock* clock,
      RtcEventLog* event_log,
      NetworkStatePredictorFactoryInterface* predictor_factory,
      NetworkControllerFactoryInterface* controller_factory,
      const BitrateConstraints& bitrate_config,
      std::unique_ptr<ProcessThread> process_thread,
      TaskQueueFactory* task_queue_factory);
  ~RtpTransportControllerSend() override;

  RtpVideoSenderInterface* CreateRtpVideoSender(
      std::map<uint32_t, RtpState> suspended_ssrcs,
      const std::map<uint32_t, RtpPayloadState>&
          states,  // move states into RtpTransportControllerSend
      const RtpConfig& rtp_config,
      int rtcp_report_interval_ms,
      Transport* send_transport,
      const RtpSenderObservers& observers,
      RtcEventLog* event_log,
      std::unique_ptr<FecController> fec_controller,
      const RtpSenderFrameEncryptionConfig& frame_encryption_config) override;
  void DestroyRtpVideoSender(
      RtpVideoSenderInterface* rtp_video_sender) override;

  // Implements RtpTransportControllerSendInterface
  rtc::TaskQueue* GetWorkerQueue() override;
  PacketRouter* packet_router() override;

  TransportFeedbackObserver* transport_feedback_observer() override;
  RtpPacketSender* packet_sender() override;

  void SetAllocatedSendBitrateLimits(int min_send_bitrate_bps,
                                     int max_padding_bitrate_bps,
                                     int max_total_bitrate_bps) override;

  void SetPacingFactor(float pacing_factor) override;
  void SetQueueTimeLimit(int limit_ms) override;
  void RegisterPacketFeedbackObserver(
      PacketFeedbackObserver* observer) override;
  void DeRegisterPacketFeedbackObserver(
      PacketFeedbackObserver* observer) override;
  void RegisterTargetTransferRateObserver(
      TargetTransferRateObserver* observer) override;
  void OnNetworkRouteChanged(const std::string& transport_name,
                             const rtc::NetworkRoute& network_route) override;
  void OnNetworkAvailability(bool network_available) override;
  RtcpBandwidthObserver* GetBandwidthObserver() override;
  int64_t GetPacerQueuingDelayMs() const override;
  int64_t GetFirstPacketTimeMs() const override;
  void EnablePeriodicAlrProbing(bool enable) override;
  void OnSentPacket(const rtc::SentPacket& sent_packet) override;

  void SetSdpBitrateParameters(const BitrateConstraints& constraints) override;
  void SetClientBitratePreferences(const BitrateSettings& preferences) override;

  void OnTransportOverheadChanged(
      size_t transport_overhead_per_packet) override;

  // Implements RtcpBandwidthObserver interface
  void OnReceivedEstimatedBitrate(uint32_t bitrate) override;
  void OnReceivedRtcpReceiverReport(const ReportBlockList& report_blocks,
                                    int64_t rtt,
                                    int64_t now_ms) override;

  // Implements TransportFeedbackObserver interface
  void AddPacket(uint32_t ssrc,
                 uint16_t sequence_number,
                 size_t length,
                 const PacedPacketInfo& pacing_info) override;
  void OnTransportFeedback(const rtcp::TransportFeedback& feedback) override;

 private:
  void MaybeCreateControllers() RTC_RUN_ON(task_queue_);
  void UpdateInitialConstraints(TargetRateConstraints new_contraints)
      RTC_RUN_ON(task_queue_);

  void StartProcessPeriodicTasks() RTC_RUN_ON(task_queue_);
  void UpdateControllerWithTimeInterval() RTC_RUN_ON(task_queue_);

  void UpdateStreamsConfig() RTC_RUN_ON(task_queue_);
  void OnReceivedRtcpReceiverReportBlocks(const ReportBlockList& report_blocks,
                                          int64_t now_ms)
      RTC_RUN_ON(task_queue_);
  void PostUpdates(NetworkControlUpdate update) RTC_RUN_ON(task_queue_);
  void UpdateControlState() RTC_RUN_ON(task_queue_);

  Clock* const clock_;
  const FieldTrialBasedConfig trial_based_config_;
  PacketRouter packet_router_;
  std::vector<std::unique_ptr<RtpVideoSenderInterface>> video_rtp_senders_;
  PacedSender pacer_;
  RtpBitrateConfigurator bitrate_configurator_;
  std::map<std::string, rtc::NetworkRoute> network_routes_;
  const std::unique_ptr<ProcessThread> process_thread_;

  TargetTransferRateObserver* observer_ RTC_GUARDED_BY(task_queue_);

  // TODO(srte): Move all access to feedback adapter to task queue.
  TransportFeedbackAdapter transport_feedback_adapter_;

  NetworkControllerFactoryInterface* const controller_factory_override_
      RTC_PT_GUARDED_BY(task_queue_);
  const std::unique_ptr<NetworkControllerFactoryInterface>
      controller_factory_fallback_ RTC_PT_GUARDED_BY(task_queue_);

  std::unique_ptr<CongestionControlHandler> control_handler_
      RTC_GUARDED_BY(task_queue_) RTC_PT_GUARDED_BY(task_queue_);

  std::unique_ptr<NetworkControllerInterface> controller_
      RTC_GUARDED_BY(task_queue_) RTC_PT_GUARDED_BY(task_queue_);

  TimeDelta process_interval_ RTC_GUARDED_BY(task_queue_);

  std::map<uint32_t, RTCPReportBlock> last_report_blocks_
      RTC_GUARDED_BY(task_queue_);
  Timestamp last_report_block_time_ RTC_GUARDED_BY(task_queue_);

  NetworkControllerConfig initial_config_ RTC_GUARDED_BY(task_queue_);
  StreamsConfig streams_config_ RTC_GUARDED_BY(task_queue_);

  const bool reset_feedback_on_route_change_;
  const bool send_side_bwe_with_overhead_;
  const bool add_pacing_to_cwin_;
  // Transport overhead is written by OnNetworkRouteChanged and read by
  // AddPacket.
  // TODO(srte): Remove atomic when feedback adapter runs on task queue.
  std::atomic<size_t> transport_overhead_bytes_per_packet_;
  bool network_available_ RTC_GUARDED_BY(task_queue_);
  RepeatingTaskHandle pacer_queue_update_task_ RTC_GUARDED_BY(task_queue_);
  RepeatingTaskHandle controller_task_ RTC_GUARDED_BY(task_queue_);
  // Protects access to last_packet_feedback_vector_ in feedback adapter.
  // TODO(srte): Remove this checker when feedback adapter runs on task queue.
  rtc::RaceChecker worker_race_;

  RateLimiter retransmission_rate_limiter_;

  // TODO(perkj): |task_queue_| is supposed to replace |process_thread_|.
  // |task_queue_| is defined last to ensure all pending tasks are cancelled
  // and deleted before any other members.
  rtc::TaskQueue task_queue_;
  RTC_DISALLOW_COPY_AND_ASSIGN(RtpTransportControllerSend);
};

}  // namespace webrtc

#endif  // CALL_RTP_TRANSPORT_CONTROLLER_SEND_H_
