// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/src/quic/core/quic_sent_packet_manager.h"

#include <algorithm>
#include <string>

#include "net/third_party/quiche/src/quic/core/congestion_control/general_loss_algorithm.h"
#include "net/third_party/quiche/src/quic/core/congestion_control/pacing_sender.h"
#include "net/third_party/quiche/src/quic/core/crypto/crypto_protocol.h"
#include "net/third_party/quiche/src/quic/core/proto/cached_network_parameters_proto.h"
#include "net/third_party/quiche/src/quic/core/quic_connection_stats.h"
#include "net/third_party/quiche/src/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_bug_tracker.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flag_utils.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_logging.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_map_util.h"

namespace quic {

namespace {
static const int64_t kDefaultRetransmissionTimeMs = 500;
static const int64_t kMaxRetransmissionTimeMs = 60000;
// Maximum number of exponential backoffs used for RTO timeouts.
static const size_t kMaxRetransmissions = 10;
// Maximum number of packets retransmitted upon an RTO.
static const size_t kMaxRetransmissionsOnTimeout = 2;
// The path degrading delay is the sum of this number of consecutive RTO delays.
const size_t kNumRetransmissionDelaysForPathDegradingDelay = 2;

// Ensure the handshake timer isnt't faster than 10ms.
// This limits the tenth retransmitted packet to 10s after the initial CHLO.
static const int64_t kMinHandshakeTimeoutMs = 10;

// Sends up to two tail loss probes before firing an RTO,
// per draft RFC draft-dukkipati-tcpm-tcp-loss-probe.
static const size_t kDefaultMaxTailLossProbes = 2;

// Returns true of retransmissions of the specified type should retransmit
// the frames directly (as opposed to resulting in a loss notification).
inline bool ShouldForceRetransmission(TransmissionType transmission_type) {
  return transmission_type == HANDSHAKE_RETRANSMISSION ||
         transmission_type == TLP_RETRANSMISSION ||
         transmission_type == PROBING_RETRANSMISSION ||
         transmission_type == RTO_RETRANSMISSION ||
         transmission_type == PTO_RETRANSMISSION;
}

// If pacing rate is accurate, > 2 burst token is not likely to help first ACK
// to arrive earlier, and overly large burst token could cause incast packet
// losses.
static const uint32_t kConservativeUnpacedBurst = 2;

}  // namespace

#define ENDPOINT                                                         \
  (unacked_packets_.perspective() == Perspective::IS_SERVER ? "Server: " \
                                                            : "Client: ")

QuicSentPacketManager::QuicSentPacketManager(
    Perspective perspective,
    const QuicClock* clock,
    QuicRandom* random,
    QuicConnectionStats* stats,
    CongestionControlType congestion_control_type,
    LossDetectionType loss_type)
    : unacked_packets_(perspective),
      clock_(clock),
      random_(random),
      stats_(stats),
      debug_delegate_(nullptr),
      network_change_visitor_(nullptr),
      initial_congestion_window_(kInitialCongestionWindow),
      loss_algorithm_(GetInitialLossAlgorithm()),
      uber_loss_algorithm_(loss_type),
      consecutive_rto_count_(0),
      consecutive_tlp_count_(0),
      consecutive_crypto_retransmission_count_(0),
      pending_timer_transmission_count_(0),
      max_tail_loss_probes_(kDefaultMaxTailLossProbes),
      max_rto_packets_(kMaxRetransmissionsOnTimeout),
      enable_half_rtt_tail_loss_probe_(false),
      using_pacing_(false),
      use_new_rto_(false),
      conservative_handshake_retransmits_(false),
      min_tlp_timeout_(
          QuicTime::Delta::FromMilliseconds(kMinTailLossProbeTimeoutMs)),
      min_rto_timeout_(
          QuicTime::Delta::FromMilliseconds(kMinRetransmissionTimeMs)),
      largest_mtu_acked_(0),
      handshake_state_(HANDSHAKE_START),
      peer_max_ack_delay_(
          QuicTime::Delta::FromMilliseconds(kDefaultDelayedAckTimeMs)),
      rtt_updated_(false),
      acked_packets_iter_(last_ack_frame_.packets.rbegin()),
      pto_enabled_(false),
      max_probe_packets_per_pto_(2),
      consecutive_pto_count_(0),
      handshake_mode_disabled_(false),
      skip_packet_number_for_pto_(false),
      always_include_max_ack_delay_for_pto_timeout_(true),
      pto_exponential_backoff_start_point_(0),
      pto_rttvar_multiplier_(4),
      neuter_handshake_packets_once_(
          GetQuicReloadableFlag(quic_neuter_handshake_packets_once2)) {
  SetSendAlgorithm(congestion_control_type);
}

LossDetectionInterface* QuicSentPacketManager::GetInitialLossAlgorithm() {
  return &uber_loss_algorithm_;
}

QuicSentPacketManager::~QuicSentPacketManager() {}

void QuicSentPacketManager::SetFromConfig(const QuicConfig& config) {
  const Perspective perspective = unacked_packets_.perspective();
  if (config.HasReceivedInitialRoundTripTimeUs() &&
      config.ReceivedInitialRoundTripTimeUs() > 0) {
    if (!config.HasClientSentConnectionOption(kNRTT, perspective)) {
      SetInitialRtt(QuicTime::Delta::FromMicroseconds(
          config.ReceivedInitialRoundTripTimeUs()));
    }
  } else if (config.HasInitialRoundTripTimeUsToSend() &&
             config.GetInitialRoundTripTimeUsToSend() > 0) {
    SetInitialRtt(QuicTime::Delta::FromMicroseconds(
        config.GetInitialRoundTripTimeUsToSend()));
  }
  if (config.HasReceivedMaxAckDelayMs()) {
    peer_max_ack_delay_ =
        QuicTime::Delta::FromMilliseconds(config.ReceivedMaxAckDelayMs());
  }
  if (config.HasClientSentConnectionOption(kMAD0, perspective)) {
    rtt_stats_.set_ignore_max_ack_delay(true);
  }
  if (config.HasClientSentConnectionOption(kMAD1, perspective)) {
    rtt_stats_.set_initial_max_ack_delay(peer_max_ack_delay_);
  }
  if (config.HasClientSentConnectionOption(kMAD2, perspective)) {
    // Set the minimum to the alarm granularity.
    min_tlp_timeout_ = QuicTime::Delta::FromMilliseconds(1);
  }
  if (config.HasClientSentConnectionOption(kMAD3, perspective)) {
    // Set the minimum to the alarm granularity.
    min_rto_timeout_ = QuicTime::Delta::FromMilliseconds(1);
  }

  if (GetQuicReloadableFlag(quic_enable_pto)) {
    if (config.HasClientSentConnectionOption(k2PTO, perspective)) {
      pto_enabled_ = true;
      QUIC_RELOADABLE_FLAG_COUNT_N(quic_enable_pto, 2, 8);
    }
    if (config.HasClientSentConnectionOption(k1PTO, perspective)) {
      pto_enabled_ = true;
      max_probe_packets_per_pto_ = 1;
      QUIC_RELOADABLE_FLAG_COUNT_N(quic_enable_pto, 1, 8);
    }
  }

  if (GetQuicReloadableFlag(quic_skip_packet_number_for_pto) &&
      config.HasClientSentConnectionOption(kPTOS, perspective)) {
    QUIC_RELOADABLE_FLAG_COUNT(quic_skip_packet_number_for_pto);
    if (!pto_enabled_) {
      QUIC_PEER_BUG
          << "PTO is not enabled when receiving PTOS connection option.";
      pto_enabled_ = true;
      max_probe_packets_per_pto_ = 1;
    }
    skip_packet_number_for_pto_ = true;
  }

  if (pto_enabled_) {
    if (config.HasClientSentConnectionOption(kPTOA, perspective)) {
      QUIC_RELOADABLE_FLAG_COUNT_N(quic_enable_pto, 5, 8);
      always_include_max_ack_delay_for_pto_timeout_ = false;
    }
    if (config.HasClientSentConnectionOption(kPEB1, perspective)) {
      QUIC_RELOADABLE_FLAG_COUNT_N(quic_enable_pto, 6, 8);
      StartExponentialBackoffAfterNthPto(1);
    }
    if (config.HasClientSentConnectionOption(kPEB2, perspective)) {
      QUIC_RELOADABLE_FLAG_COUNT_N(quic_enable_pto, 7, 8);
      StartExponentialBackoffAfterNthPto(2);
    }
    if (config.HasClientSentConnectionOption(kPVS1, perspective)) {
      QUIC_RELOADABLE_FLAG_COUNT_N(quic_enable_pto, 8, 8);
      pto_rttvar_multiplier_ = 2;
    }
  }

  // Configure congestion control.
  if (config.HasClientRequestedIndependentOption(kTBBR, perspective)) {
    SetSendAlgorithm(kBBR);
  }
  if (GetQuicReloadableFlag(quic_allow_client_enabled_bbr_v2) &&
      config.HasClientRequestedIndependentOption(kB2ON, perspective)) {
    QUIC_RELOADABLE_FLAG_COUNT(quic_allow_client_enabled_bbr_v2);
    SetSendAlgorithm(kBBRv2);
  }

  if (config.HasClientRequestedIndependentOption(kRENO, perspective)) {
    SetSendAlgorithm(kRenoBytes);
  } else if (config.HasClientRequestedIndependentOption(kBYTE, perspective) ||
             (GetQuicReloadableFlag(quic_default_to_bbr) &&
              config.HasClientRequestedIndependentOption(kQBIC, perspective))) {
    SetSendAlgorithm(kCubicBytes);
  } else if (GetQuicReloadableFlag(quic_enable_pcc3) &&
             config.HasClientRequestedIndependentOption(kTPCC, perspective)) {
    SetSendAlgorithm(kPCC);
  }

  // Initial window.
  if (GetQuicReloadableFlag(quic_unified_iw_options)) {
    if (config.HasClientRequestedIndependentOption(kIW03, perspective)) {
      initial_congestion_window_ = 3;
      send_algorithm_->SetInitialCongestionWindowInPackets(3);
    }
    if (config.HasClientRequestedIndependentOption(kIW10, perspective)) {
      initial_congestion_window_ = 10;
      send_algorithm_->SetInitialCongestionWindowInPackets(10);
    }
    if (config.HasClientRequestedIndependentOption(kIW20, perspective)) {
      initial_congestion_window_ = 20;
      send_algorithm_->SetInitialCongestionWindowInPackets(20);
    }
    if (config.HasClientRequestedIndependentOption(kIW50, perspective)) {
      initial_congestion_window_ = 50;
      send_algorithm_->SetInitialCongestionWindowInPackets(50);
    }
  }

  using_pacing_ = !GetQuicFlag(FLAGS_quic_disable_pacing_for_perf_tests);

  if (config.HasClientSentConnectionOption(kNTLP, perspective)) {
    max_tail_loss_probes_ = 0;
  }
  if (config.HasClientSentConnectionOption(k1TLP, perspective)) {
    max_tail_loss_probes_ = 1;
  }
  if (config.HasClientSentConnectionOption(k1RTO, perspective)) {
    max_rto_packets_ = 1;
  }
  if (config.HasClientSentConnectionOption(kTLPR, perspective)) {
    enable_half_rtt_tail_loss_probe_ = true;
  }
  if (config.HasClientSentConnectionOption(kNRTO, perspective)) {
    use_new_rto_ = true;
  }
  // Configure loss detection.
  if (config.HasClientRequestedIndependentOption(kTIME, perspective)) {
    uber_loss_algorithm_.SetLossDetectionType(kTime);
  }
  if (config.HasClientRequestedIndependentOption(kATIM, perspective)) {
    uber_loss_algorithm_.SetLossDetectionType(kAdaptiveTime);
  }
  if (config.HasClientRequestedIndependentOption(kLFAK, perspective)) {
    uber_loss_algorithm_.SetLossDetectionType(kLazyFack);
  }
  if (GetQuicReloadableFlag(quic_enable_ietf_loss_detection)) {
    if (config.HasClientRequestedIndependentOption(kILD0, perspective)) {
      QUIC_RELOADABLE_FLAG_COUNT_N(quic_enable_ietf_loss_detection, 1, 5);
      uber_loss_algorithm_.SetLossDetectionType(kIetfLossDetection);
    }
    if (config.HasClientRequestedIndependentOption(kILD1, perspective)) {
      QUIC_RELOADABLE_FLAG_COUNT_N(quic_enable_ietf_loss_detection, 2, 5);
      uber_loss_algorithm_.SetLossDetectionType(kIetfLossDetection);
      uber_loss_algorithm_.SetReorderingShift(kDefaultLossDelayShift);
    }
    if (config.HasClientRequestedIndependentOption(kILD2, perspective)) {
      QUIC_RELOADABLE_FLAG_COUNT_N(quic_enable_ietf_loss_detection, 3, 5);
      uber_loss_algorithm_.SetLossDetectionType(kIetfLossDetection);
      uber_loss_algorithm_.EnableAdaptiveReorderingThreshold();
    }
    if (config.HasClientRequestedIndependentOption(kILD3, perspective)) {
      QUIC_RELOADABLE_FLAG_COUNT_N(quic_enable_ietf_loss_detection, 4, 5);
      uber_loss_algorithm_.SetLossDetectionType(kIetfLossDetection);
      uber_loss_algorithm_.SetReorderingShift(kDefaultLossDelayShift);
      uber_loss_algorithm_.EnableAdaptiveReorderingThreshold();
    }
    if (config.HasClientRequestedIndependentOption(kILD4, perspective)) {
      QUIC_RELOADABLE_FLAG_COUNT_N(quic_enable_ietf_loss_detection, 5, 5);
      uber_loss_algorithm_.SetLossDetectionType(kIetfLossDetection);
      uber_loss_algorithm_.SetReorderingShift(kDefaultLossDelayShift);
      uber_loss_algorithm_.EnableAdaptiveReorderingThreshold();
      uber_loss_algorithm_.EnableAdaptiveTimeThreshold();
    }
  }
  if (config.HasClientSentConnectionOption(kCONH, perspective)) {
    conservative_handshake_retransmits_ = true;
  }
  send_algorithm_->SetFromConfig(config, perspective);

  if (network_change_visitor_ != nullptr) {
    network_change_visitor_->OnCongestionChange();
  }
}

void QuicSentPacketManager::ResumeConnectionState(
    const CachedNetworkParameters& cached_network_params,
    bool max_bandwidth_resumption) {
  QuicBandwidth bandwidth = QuicBandwidth::FromBytesPerSecond(
      max_bandwidth_resumption
          ? cached_network_params.max_bandwidth_estimate_bytes_per_second()
          : cached_network_params.bandwidth_estimate_bytes_per_second());
  QuicTime::Delta rtt =
      QuicTime::Delta::FromMilliseconds(cached_network_params.min_rtt_ms());
  // This calls the old AdjustNetworkParameters interface, and fills certain
  // fields in SendAlgorithmInterface::NetworkParams
  // (e.g., quic_bbr_fix_pacing_rate) using GFE flags.
  AdjustNetworkParameters(SendAlgorithmInterface::NetworkParams(
      bandwidth, rtt, /*allow_cwnd_to_decrease = */ false));
}

void QuicSentPacketManager::AdjustNetworkParameters(
    const SendAlgorithmInterface::NetworkParams& params) {
  const QuicBandwidth& bandwidth = params.bandwidth;
  const QuicTime::Delta& rtt = params.rtt;
  if (!rtt.IsZero()) {
    SetInitialRtt(rtt);
  }
  const QuicByteCount old_cwnd = send_algorithm_->GetCongestionWindow();
  if (GetQuicReloadableFlag(quic_conservative_bursts) && using_pacing_ &&
      !bandwidth.IsZero()) {
    QUIC_RELOADABLE_FLAG_COUNT(quic_conservative_bursts);
    pacing_sender_.SetBurstTokens(kConservativeUnpacedBurst);
  }
  send_algorithm_->AdjustNetworkParameters(params);
  if (debug_delegate_ != nullptr) {
    debug_delegate_->OnAdjustNetworkParameters(
        bandwidth, rtt.IsZero() ? rtt_stats_.SmoothedOrInitialRtt() : rtt,
        old_cwnd, send_algorithm_->GetCongestionWindow());
  }
}

void QuicSentPacketManager::SetHandshakeConfirmed() {
  if (!neuter_handshake_packets_once_ ||
      handshake_state_ < HANDSHAKE_COMPLETE) {
    if (neuter_handshake_packets_once_) {
      QUIC_RELOADABLE_FLAG_COUNT_N(quic_neuter_handshake_packets_once2, 1, 3);
    }
    handshake_state_ = HANDSHAKE_COMPLETE;
    NeuterHandshakePackets();
  }
}

void QuicSentPacketManager::PostProcessNewlyAckedPackets(
    QuicPacketNumber ack_packet_number,
    const QuicAckFrame& ack_frame,
    QuicTime ack_receive_time,
    bool rtt_updated,
    QuicByteCount prior_bytes_in_flight) {
  unacked_packets_.NotifyAggregatedStreamFrameAcked(
      last_ack_frame_.ack_delay_time);
  InvokeLossDetection(ack_receive_time);
  // Ignore losses in RTO mode.
  if (consecutive_rto_count_ > 0 && !use_new_rto_) {
    packets_lost_.clear();
  }
  MaybeInvokeCongestionEvent(rtt_updated, prior_bytes_in_flight,
                             ack_receive_time);
  unacked_packets_.RemoveObsoletePackets();

  sustained_bandwidth_recorder_.RecordEstimate(
      send_algorithm_->InRecovery(), send_algorithm_->InSlowStart(),
      send_algorithm_->BandwidthEstimate(), ack_receive_time, clock_->WallNow(),
      rtt_stats_.smoothed_rtt());

  // Anytime we are making forward progress and have a new RTT estimate, reset
  // the backoff counters.
  if (rtt_updated) {
    if (consecutive_rto_count_ > 0) {
      // If the ack acknowledges data sent prior to the RTO,
      // the RTO was spurious.
      if (LargestAcked(ack_frame) < first_rto_transmission_) {
        // Replace SRTT with latest_rtt and increase the variance to prevent
        // a spurious RTO from happening again.
        rtt_stats_.ExpireSmoothedMetrics();
      } else {
        if (!use_new_rto_) {
          send_algorithm_->OnRetransmissionTimeout(true);
        }
      }
    }
    // Reset all retransmit counters any time a new packet is acked.
    consecutive_rto_count_ = 0;
    consecutive_tlp_count_ = 0;
    consecutive_pto_count_ = 0;
    consecutive_crypto_retransmission_count_ = 0;
  }

  if (debug_delegate_ != nullptr) {
    debug_delegate_->OnIncomingAck(ack_packet_number, ack_frame,
                                   ack_receive_time, LargestAcked(ack_frame),
                                   rtt_updated, GetLeastUnacked());
  }
  // Remove packets below least unacked from all_packets_acked_ and
  // last_ack_frame_.
  last_ack_frame_.packets.RemoveUpTo(unacked_packets_.GetLeastUnacked());
  last_ack_frame_.received_packet_times.clear();
}

void QuicSentPacketManager::MaybeInvokeCongestionEvent(
    bool rtt_updated,
    QuicByteCount prior_in_flight,
    QuicTime event_time) {
  if (!rtt_updated && packets_acked_.empty() && packets_lost_.empty()) {
    return;
  }
  if (using_pacing_) {
    pacing_sender_.OnCongestionEvent(rtt_updated, prior_in_flight, event_time,
                                     packets_acked_, packets_lost_);
  } else {
    send_algorithm_->OnCongestionEvent(rtt_updated, prior_in_flight, event_time,
                                       packets_acked_, packets_lost_);
  }
  packets_acked_.clear();
  packets_lost_.clear();
  if (network_change_visitor_ != nullptr) {
    network_change_visitor_->OnCongestionChange();
  }
}

void QuicSentPacketManager::RetransmitUnackedPackets(
    TransmissionType retransmission_type) {
  DCHECK(retransmission_type == ALL_UNACKED_RETRANSMISSION ||
         retransmission_type == ALL_INITIAL_RETRANSMISSION);
  QuicPacketNumber packet_number = unacked_packets_.GetLeastUnacked();
  for (QuicUnackedPacketMap::iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it, ++packet_number) {
    if ((retransmission_type == ALL_UNACKED_RETRANSMISSION ||
         it->encryption_level == ENCRYPTION_ZERO_RTT)) {
      if (it->in_flight) {
        // Remove 0-RTT packets and packets of the wrong version from flight,
        // because neither can be processed by the peer.
        unacked_packets_.RemoveFromInFlight(&*it);
      }
      if (unacked_packets_.HasRetransmittableFrames(*it)) {
        MarkForRetransmission(packet_number, retransmission_type);
      }
    }
  }
  if (retransmission_type == ALL_UNACKED_RETRANSMISSION &&
      unacked_packets_.bytes_in_flight() > 0) {
    QUIC_BUG << "RetransmitUnackedPackets should remove all packets from flight"
             << ", bytes_in_flight:" << unacked_packets_.bytes_in_flight();
  }
}

void QuicSentPacketManager::NeuterUnencryptedPackets() {
  QuicPacketNumber packet_number = unacked_packets_.GetLeastUnacked();
  for (QuicUnackedPacketMap::iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it, ++packet_number) {
    if (!it->retransmittable_frames.empty() &&
        it->encryption_level == ENCRYPTION_INITIAL) {
      // Once the connection swithes to forward secure, no unencrypted packets
      // will be sent. The data has been abandoned in the cryto stream. Remove
      // it from in flight.
      unacked_packets_.RemoveFromInFlight(packet_number);
      if (neuter_handshake_packets_once_) {
        QUIC_RELOADABLE_FLAG_COUNT_N(quic_neuter_handshake_packets_once2, 2, 3);
        it->state = NEUTERED;
        DCHECK(!unacked_packets_.HasRetransmittableFrames(*it));
      }
    }
  }
}

void QuicSentPacketManager::NeuterHandshakePackets() {
  QuicPacketNumber packet_number = unacked_packets_.GetLeastUnacked();
  for (QuicUnackedPacketMap::iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it, ++packet_number) {
    if (!it->retransmittable_frames.empty() &&
        unacked_packets_.GetPacketNumberSpace(it->encryption_level) ==
            HANDSHAKE_DATA) {
      unacked_packets_.RemoveFromInFlight(packet_number);
      if (neuter_handshake_packets_once_) {
        // Notify session that the data has been delivered (but do not notify
        // send algorithm).
        QUIC_RELOADABLE_FLAG_COUNT_N(quic_neuter_handshake_packets_once2, 3, 3);
        it->state = NEUTERED;
        unacked_packets_.NotifyFramesAcked(*it, QuicTime::Delta::Zero(),
                                           QuicTime::Zero());
      }
    }
  }
}

bool QuicSentPacketManager::ShouldAddMaxAckDelay() const {
  DCHECK(pto_enabled_);
  if (always_include_max_ack_delay_for_pto_timeout_) {
    return true;
  }
  if (!unacked_packets_
           .GetLargestSentRetransmittableOfPacketNumberSpace(APPLICATION_DATA)
           .IsInitialized() ||
      unacked_packets_.GetLargestSentRetransmittableOfPacketNumberSpace(
          APPLICATION_DATA) <
          FirstSendingPacketNumber() + kMinReceivedBeforeAckDecimation - 1) {
    // Peer is doing TCP style acking. Expect an immediate ACK if more than 1
    // packet are outstanding.
    if (unacked_packets_.packets_in_flight() >=
        kDefaultRetransmittablePacketsBeforeAck) {
      return false;
    }
  } else if (unacked_packets_.packets_in_flight() >=
             kMaxRetransmittablePacketsBeforeAck) {
    // Peer is doing ack decimation. Expect an immediate ACK if >= 10
    // packets are outstanding.
    return false;
  }
  if (skip_packet_number_for_pto_ && consecutive_pto_count_ > 0) {
    // An immediate ACK is expected when doing PTOS. Please note, this will miss
    // cases when PTO fires and turns out to be spurious.
    return false;
  }
  return true;
}

void QuicSentPacketManager::MarkForRetransmission(
    QuicPacketNumber packet_number,
    TransmissionType transmission_type) {
  QuicTransmissionInfo* transmission_info =
      unacked_packets_.GetMutableTransmissionInfo(packet_number);
  // A previous RTO retransmission may cause connection close; packets without
  // retransmittable frames can be marked for loss retransmissions.
  QUIC_BUG_IF(transmission_type != LOSS_RETRANSMISSION &&
              transmission_type != RTO_RETRANSMISSION &&
              !unacked_packets_.HasRetransmittableFrames(*transmission_info))
      << "transmission_type: " << TransmissionTypeToString(transmission_type);
  // Handshake packets should never be sent as probing retransmissions.
  DCHECK(!transmission_info->has_crypto_handshake ||
         transmission_type != PROBING_RETRANSMISSION);

  HandleRetransmission(transmission_type, transmission_info);

  // Update packet state according to transmission type.
  transmission_info->state =
      QuicUtils::RetransmissionTypeToPacketState(transmission_type);
}

void QuicSentPacketManager::HandleRetransmission(
    TransmissionType transmission_type,
    QuicTransmissionInfo* transmission_info) {
  if (ShouldForceRetransmission(transmission_type)) {
    // TODO(fayang): Consider to make RTO and PROBING retransmission
    // strategies be configurable by applications. Today, TLP, RTO and PROBING
    // retransmissions are handled similarly, i.e., always retranmist the
    // oldest outstanding data. This is not ideal in general because different
    // applications may want different strategies. For example, some
    // applications may want to use higher priority stream data for bandwidth
    // probing, and some applications want to consider RTO is an indication of
    // loss, etc.
    unacked_packets_.RetransmitFrames(*transmission_info, transmission_type);
    return;
  }

  unacked_packets_.NotifyFramesLost(*transmission_info, transmission_type);
  if (transmission_info->retransmittable_frames.empty()) {
    return;
  }

  if (transmission_type == LOSS_RETRANSMISSION) {
    // Record the first packet sent after loss, which allows to wait 1
    // more RTT before giving up on this lost packet.
    transmission_info->retransmission =
        unacked_packets_.largest_sent_packet() + 1;
  } else {
    // Clear the recorded first packet sent after loss when version or
    // encryption changes.
    transmission_info->retransmission.Clear();
  }
}

void QuicSentPacketManager::RecordOneSpuriousRetransmission(
    const QuicTransmissionInfo& info) {
  stats_->bytes_spuriously_retransmitted += info.bytes_sent;
  ++stats_->packets_spuriously_retransmitted;
  if (debug_delegate_ != nullptr) {
    debug_delegate_->OnSpuriousPacketRetransmission(info.transmission_type,
                                                    info.bytes_sent);
  }
}

void QuicSentPacketManager::MarkPacketHandled(QuicPacketNumber packet_number,
                                              QuicTransmissionInfo* info,
                                              QuicTime ack_receive_time,
                                              QuicTime::Delta ack_delay_time,
                                              QuicTime receive_timestamp) {
  // Try to aggregate acked stream frames if acked packet is not a
  // retransmission.
  if (info->transmission_type == NOT_RETRANSMISSION) {
    unacked_packets_.MaybeAggregateAckedStreamFrame(*info, ack_delay_time,
                                                    receive_timestamp);
  } else {
    unacked_packets_.NotifyAggregatedStreamFrameAcked(ack_delay_time);
    const bool new_data_acked = unacked_packets_.NotifyFramesAcked(
        *info, ack_delay_time, receive_timestamp);
    if (!new_data_acked && info->transmission_type != NOT_RETRANSMISSION) {
      // Record as a spurious retransmission if this packet is a
      // retransmission and no new data gets acked.
      QUIC_DVLOG(1) << "Detect spurious retransmitted packet " << packet_number
                    << " transmission type: "
                    << TransmissionTypeToString(info->transmission_type);
      RecordOneSpuriousRetransmission(*info);
    }
  }
  if (info->state == LOST) {
    // Record as a spurious loss as a packet previously declared lost gets
    // acked.
    const PacketNumberSpace packet_number_space =
        unacked_packets_.GetPacketNumberSpace(info->encryption_level);
    const QuicPacketNumber previous_largest_acked =
        supports_multiple_packet_number_spaces()
            ? unacked_packets_.GetLargestAckedOfPacketNumberSpace(
                  packet_number_space)
            : unacked_packets_.largest_acked();
    QUIC_DVLOG(1) << "Packet " << packet_number
                  << " was detected lost spuriously, "
                     "previous_largest_acked: "
                  << previous_largest_acked;
    loss_algorithm_->SpuriousLossDetected(unacked_packets_, rtt_stats_,
                                          ack_receive_time, packet_number,
                                          previous_largest_acked);
  }

  if (network_change_visitor_ != nullptr &&
      info->bytes_sent > largest_mtu_acked_) {
    largest_mtu_acked_ = info->bytes_sent;
    network_change_visitor_->OnPathMtuIncreased(largest_mtu_acked_);
  }
  unacked_packets_.RemoveFromInFlight(info);
  unacked_packets_.RemoveRetransmittability(info);
  info->state = ACKED;
}

bool QuicSentPacketManager::OnPacketSent(
    SerializedPacket* serialized_packet,
    QuicTime sent_time,
    TransmissionType transmission_type,
    HasRetransmittableData has_retransmittable_data) {
  QuicPacketNumber packet_number = serialized_packet->packet_number;
  DCHECK_LE(FirstSendingPacketNumber(), packet_number);
  DCHECK(!unacked_packets_.IsUnacked(packet_number));
  QUIC_BUG_IF(serialized_packet->encrypted_length == 0)
      << "Cannot send empty packets.";
  if (pending_timer_transmission_count_ > 0) {
    --pending_timer_transmission_count_;
  }

  bool in_flight = has_retransmittable_data == HAS_RETRANSMITTABLE_DATA;
  if (using_pacing_) {
    pacing_sender_.OnPacketSent(
        sent_time, unacked_packets_.bytes_in_flight(), packet_number,
        serialized_packet->encrypted_length, has_retransmittable_data);
  } else {
    send_algorithm_->OnPacketSent(
        sent_time, unacked_packets_.bytes_in_flight(), packet_number,
        serialized_packet->encrypted_length, has_retransmittable_data);
  }

  unacked_packets_.AddSentPacket(serialized_packet, transmission_type,
                                 sent_time, in_flight);
  // Reset the retransmission timer anytime a pending packet is sent.
  return in_flight;
}

QuicSentPacketManager::RetransmissionTimeoutMode
QuicSentPacketManager::OnRetransmissionTimeout() {
  DCHECK(unacked_packets_.HasInFlightPackets() ||
         (handshake_mode_disabled_ && handshake_state_ < HANDSHAKE_COMPLETE));
  DCHECK_EQ(0u, pending_timer_transmission_count_);
  // Handshake retransmission, timer based loss detection, TLP, and RTO are
  // implemented with a single alarm. The handshake alarm is set when the
  // handshake has not completed, the loss alarm is set when the loss detection
  // algorithm says to, and the TLP and  RTO alarms are set after that.
  // The TLP alarm is always set to run for under an RTO.
  switch (GetRetransmissionMode()) {
    case HANDSHAKE_MODE:
      DCHECK(!handshake_mode_disabled_);
      ++stats_->crypto_retransmit_count;
      RetransmitCryptoPackets();
      return HANDSHAKE_MODE;
    case LOSS_MODE: {
      ++stats_->loss_timeout_count;
      QuicByteCount prior_in_flight = unacked_packets_.bytes_in_flight();
      const QuicTime now = clock_->Now();
      InvokeLossDetection(now);
      MaybeInvokeCongestionEvent(false, prior_in_flight, now);
      return LOSS_MODE;
    }
    case TLP_MODE:
      ++stats_->tlp_count;
      ++consecutive_tlp_count_;
      pending_timer_transmission_count_ = 1;
      // TLPs prefer sending new data instead of retransmitting data, so
      // give the connection a chance to write before completing the TLP.
      return TLP_MODE;
    case RTO_MODE:
      ++stats_->rto_count;
      RetransmitRtoPackets();
      return RTO_MODE;
    case PTO_MODE:
      QUIC_DVLOG(1) << ENDPOINT << "PTO mode";
      ++stats_->pto_count;
      ++consecutive_pto_count_;
      pending_timer_transmission_count_ = max_probe_packets_per_pto_;
      return PTO_MODE;
  }
}

void QuicSentPacketManager::RetransmitCryptoPackets() {
  DCHECK_EQ(HANDSHAKE_MODE, GetRetransmissionMode());
  ++consecutive_crypto_retransmission_count_;
  bool packet_retransmitted = false;
  QuicPacketNumber packet_number = unacked_packets_.GetLeastUnacked();
  std::vector<QuicPacketNumber> crypto_retransmissions;
  for (QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it, ++packet_number) {
    // Only retransmit frames which are in flight, and therefore have been sent.
    if (!it->in_flight || it->state != OUTSTANDING ||
        !it->has_crypto_handshake ||
        !unacked_packets_.HasRetransmittableFrames(*it)) {
      continue;
    }
    packet_retransmitted = true;
    crypto_retransmissions.push_back(packet_number);
    ++pending_timer_transmission_count_;
  }
  DCHECK(packet_retransmitted) << "No crypto packets found to retransmit.";
  for (QuicPacketNumber retransmission : crypto_retransmissions) {
    MarkForRetransmission(retransmission, HANDSHAKE_RETRANSMISSION);
  }
}

bool QuicSentPacketManager::MaybeRetransmitTailLossProbe() {
  DCHECK(!pto_enabled_);
  if (pending_timer_transmission_count_ == 0) {
    return false;
  }
  if (!MaybeRetransmitOldestPacket(TLP_RETRANSMISSION)) {
    return false;
  }
  return true;
}

bool QuicSentPacketManager::MaybeRetransmitOldestPacket(TransmissionType type) {
  QuicPacketNumber packet_number = unacked_packets_.GetLeastUnacked();
  for (QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it, ++packet_number) {
    // Only retransmit frames which are in flight, and therefore have been sent.
    if (!it->in_flight || it->state != OUTSTANDING ||
        !unacked_packets_.HasRetransmittableFrames(*it)) {
      continue;
    }
    MarkForRetransmission(packet_number, type);
    return true;
  }
  QUIC_DVLOG(1)
      << "No retransmittable packets, so RetransmitOldestPacket failed.";
  return false;
}

void QuicSentPacketManager::RetransmitRtoPackets() {
  DCHECK(!pto_enabled_);
  QUIC_BUG_IF(pending_timer_transmission_count_ > 0)
      << "Retransmissions already queued:" << pending_timer_transmission_count_;
  // Mark two packets for retransmission.
  QuicPacketNumber packet_number = unacked_packets_.GetLeastUnacked();
  std::vector<QuicPacketNumber> retransmissions;
  for (QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it, ++packet_number) {
    if (it->state == OUTSTANDING &&
        unacked_packets_.HasRetransmittableFrames(*it) &&
        pending_timer_transmission_count_ < max_rto_packets_) {
      DCHECK(!neuter_handshake_packets_once_ || it->in_flight);
      retransmissions.push_back(packet_number);
      ++pending_timer_transmission_count_;
    }
  }
  if (pending_timer_transmission_count_ > 0) {
    if (consecutive_rto_count_ == 0) {
      first_rto_transmission_ = unacked_packets_.largest_sent_packet() + 1;
    }
    ++consecutive_rto_count_;
  }
  for (QuicPacketNumber retransmission : retransmissions) {
    MarkForRetransmission(retransmission, RTO_RETRANSMISSION);
  }
  if (retransmissions.empty()) {
    QUIC_BUG_IF(pending_timer_transmission_count_ != 0);
    // No packets to be RTO retransmitted, raise up a credit to allow
    // connection to send.
    QUIC_CODE_COUNT(no_packets_to_be_rto_retransmitted);
    pending_timer_transmission_count_ = 1;
  }
}

void QuicSentPacketManager::MaybeSendProbePackets() {
  if (pending_timer_transmission_count_ == 0) {
    return;
  }
  QuicPacketNumber packet_number = unacked_packets_.GetLeastUnacked();
  std::vector<QuicPacketNumber> probing_packets;
  for (QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it, ++packet_number) {
    if (it->state == OUTSTANDING &&
        unacked_packets_.HasRetransmittableFrames(*it)) {
      DCHECK(!neuter_handshake_packets_once_ || it->in_flight);
      probing_packets.push_back(packet_number);
      if (probing_packets.size() == pending_timer_transmission_count_) {
        break;
      }
    }
  }

  for (QuicPacketNumber retransmission : probing_packets) {
    QUIC_DVLOG(1) << ENDPOINT << "Marking " << retransmission
                  << " for probing retransmission";
    MarkForRetransmission(retransmission, PTO_RETRANSMISSION);
  }
  // It is possible that there is not enough outstanding data for probing.
}

void QuicSentPacketManager::AdjustPendingTimerTransmissions() {
  if (pending_timer_transmission_count_ < max_probe_packets_per_pto_) {
    // There are packets sent already, clear credit.
    pending_timer_transmission_count_ = 0;
    return;
  }
  // No packet gets sent, leave 1 credit to allow data to be write eventually.
  pending_timer_transmission_count_ = 1;
}

void QuicSentPacketManager::EnableIetfPtoAndLossDetection() {
  pto_enabled_ = true;
  handshake_mode_disabled_ = true;
  uber_loss_algorithm_.SetLossDetectionType(kIetfLossDetection);
}

void QuicSentPacketManager::StartExponentialBackoffAfterNthPto(
    size_t exponential_backoff_start_point) {
  pto_exponential_backoff_start_point_ = exponential_backoff_start_point;
}

QuicSentPacketManager::RetransmissionTimeoutMode
QuicSentPacketManager::GetRetransmissionMode() const {
  DCHECK(unacked_packets_.HasInFlightPackets() ||
         (handshake_mode_disabled_ && handshake_state_ < HANDSHAKE_COMPLETE));
  if (!handshake_mode_disabled_ && handshake_state_ < HANDSHAKE_COMPLETE &&
      unacked_packets_.HasPendingCryptoPackets()) {
    return HANDSHAKE_MODE;
  }
  if (loss_algorithm_->GetLossTimeout() != QuicTime::Zero()) {
    return LOSS_MODE;
  }
  if (pto_enabled_) {
    return PTO_MODE;
  }
  if (consecutive_tlp_count_ < max_tail_loss_probes_) {
    if (unacked_packets_.HasUnackedRetransmittableFrames()) {
      return TLP_MODE;
    }
  }
  return RTO_MODE;
}

void QuicSentPacketManager::InvokeLossDetection(QuicTime time) {
  if (!packets_acked_.empty()) {
    DCHECK_LE(packets_acked_.front().packet_number,
              packets_acked_.back().packet_number);
    largest_newly_acked_ = packets_acked_.back().packet_number;
  }
  loss_algorithm_->DetectLosses(unacked_packets_, time, rtt_stats_,
                                largest_newly_acked_, packets_acked_,
                                &packets_lost_);
  for (const LostPacket& packet : packets_lost_) {
    ++stats_->packets_lost;
    if (debug_delegate_ != nullptr) {
      debug_delegate_->OnPacketLoss(packet.packet_number, LOSS_RETRANSMISSION,
                                    time);
    }
    unacked_packets_.RemoveFromInFlight(packet.packet_number);
    MarkForRetransmission(packet.packet_number, LOSS_RETRANSMISSION);
  }
}

bool QuicSentPacketManager::MaybeUpdateRTT(QuicPacketNumber largest_acked,
                                           QuicTime::Delta ack_delay_time,
                                           QuicTime ack_receive_time) {
  // We rely on ack_delay_time to compute an RTT estimate, so we
  // only update rtt when the largest observed gets acked.
  if (!unacked_packets_.IsUnacked(largest_acked)) {
    return false;
  }
  // We calculate the RTT based on the highest ACKed packet number, the lower
  // packet numbers will include the ACK aggregation delay.
  const QuicTransmissionInfo& transmission_info =
      unacked_packets_.GetTransmissionInfo(largest_acked);
  // Ensure the packet has a valid sent time.
  if (transmission_info.sent_time == QuicTime::Zero()) {
    QUIC_BUG << "Acked packet has zero sent time, largest_acked:"
             << largest_acked;
    return false;
  }
  if (transmission_info.sent_time > ack_receive_time) {
    QUIC_CODE_COUNT(quic_receive_acked_before_sending);
  }

  QuicTime::Delta send_delta = ack_receive_time - transmission_info.sent_time;
  rtt_stats_.UpdateRtt(send_delta, ack_delay_time, ack_receive_time);

  return true;
}

QuicTime::Delta QuicSentPacketManager::TimeUntilSend(QuicTime now) const {
  // The TLP logic is entirely contained within QuicSentPacketManager, so the
  // send algorithm does not need to be consulted.
  if (pending_timer_transmission_count_ > 0) {
    return QuicTime::Delta::Zero();
  }

  if (using_pacing_) {
    return pacing_sender_.TimeUntilSend(now,
                                        unacked_packets_.bytes_in_flight());
  }

  return send_algorithm_->CanSend(unacked_packets_.bytes_in_flight())
             ? QuicTime::Delta::Zero()
             : QuicTime::Delta::Infinite();
}

const QuicTime QuicSentPacketManager::GetRetransmissionTime() const {
  if (!unacked_packets_.HasInFlightPackets() &&
      (!handshake_mode_disabled_ || handshake_state_ >= HANDSHAKE_COMPLETE ||
       unacked_packets_.perspective() == Perspective::IS_SERVER)) {
    // Do not set the timer if there is nothing in flight. However, to avoid
    // handshake deadlock due to anti-amplification limit, client needs to set
    // PTO timer when the handshake is not confirmed even there is nothing in
    // flight.
    return QuicTime::Zero();
  }
  if (pending_timer_transmission_count_ > 0) {
    // Do not set the timer if there is any credit left.
    return QuicTime::Zero();
  }
  switch (GetRetransmissionMode()) {
    case HANDSHAKE_MODE:
      return unacked_packets_.GetLastCryptoPacketSentTime() +
             GetCryptoRetransmissionDelay();
    case LOSS_MODE:
      return loss_algorithm_->GetLossTimeout();
    case TLP_MODE: {
      DCHECK(!pto_enabled_);
      // TODO(ianswett): When CWND is available, it would be preferable to
      // set the timer based on the earliest retransmittable packet.
      // Base the updated timer on the send time of the last packet.
      const QuicTime sent_time =
          unacked_packets_.GetLastInFlightPacketSentTime();
      const QuicTime tlp_time = sent_time + GetTailLossProbeDelay();
      // Ensure the TLP timer never gets set to a time in the past.
      return std::max(clock_->ApproximateNow(), tlp_time);
    }
    case RTO_MODE: {
      DCHECK(!pto_enabled_);
      // The RTO is based on the first outstanding packet.
      const QuicTime sent_time =
          unacked_packets_.GetLastInFlightPacketSentTime();
      QuicTime rto_time = sent_time + GetRetransmissionDelay();
      // Wait for TLP packets to be acked before an RTO fires.
      QuicTime tlp_time = sent_time + GetTailLossProbeDelay();
      return std::max(tlp_time, rto_time);
    }
    case PTO_MODE: {
      // Ensure PTO never gets set to a time in the past.
      return std::max(clock_->ApproximateNow(),
                      unacked_packets_.GetLastInFlightPacketSentTime() +
                          GetProbeTimeoutDelay());
    }
  }
  DCHECK(false);
  return QuicTime::Zero();
}

const QuicTime::Delta QuicSentPacketManager::GetPathDegradingDelay() const {
  QuicTime::Delta delay = QuicTime::Delta::Zero();
  for (size_t i = 0; i < max_tail_loss_probes_; ++i) {
    delay = delay + GetTailLossProbeDelay(i);
  }
  for (size_t i = 0; i < kNumRetransmissionDelaysForPathDegradingDelay; ++i) {
    delay = delay + GetRetransmissionDelay(i);
  }
  return delay;
}

const QuicTime::Delta QuicSentPacketManager::GetCryptoRetransmissionDelay()
    const {
  // This is equivalent to the TailLossProbeDelay, but slightly more aggressive
  // because crypto handshake messages don't incur a delayed ack time.
  QuicTime::Delta srtt = rtt_stats_.SmoothedOrInitialRtt();
  int64_t delay_ms;
  if (conservative_handshake_retransmits_) {
    // Using the delayed ack time directly could cause conservative handshake
    // retransmissions to actually be more aggressive than the default.
    delay_ms = std::max(peer_max_ack_delay_.ToMilliseconds(),
                        static_cast<int64_t>(2 * srtt.ToMilliseconds()));
  } else {
    delay_ms = std::max(kMinHandshakeTimeoutMs,
                        static_cast<int64_t>(1.5 * srtt.ToMilliseconds()));
  }
  return QuicTime::Delta::FromMilliseconds(
      delay_ms << consecutive_crypto_retransmission_count_);
}

const QuicTime::Delta QuicSentPacketManager::GetTailLossProbeDelay(
    size_t consecutive_tlp_count) const {
  QuicTime::Delta srtt = rtt_stats_.SmoothedOrInitialRtt();
  if (enable_half_rtt_tail_loss_probe_ && consecutive_tlp_count == 0u) {
    if (unacked_packets().HasUnackedStreamData()) {
      // Enable TLPR if there are pending data packets.
      return std::max(min_tlp_timeout_, srtt * 0.5);
    }
  }
  if (!unacked_packets_.HasMultipleInFlightPackets()) {
    // This expression really should be using the delayed ack time, but in TCP
    // MinRTO was traditionally set to 2x the delayed ack timer and this
    // expression assumed QUIC did the same.
    return std::max(2 * srtt, 1.5 * srtt + (min_rto_timeout_ * 0.5));
  }
  return std::max(min_tlp_timeout_, 2 * srtt);
}

const QuicTime::Delta QuicSentPacketManager::GetRetransmissionDelay(
    size_t consecutive_rto_count) const {
  QuicTime::Delta retransmission_delay = QuicTime::Delta::Zero();
  if (rtt_stats_.smoothed_rtt().IsZero()) {
    // We are in the initial state, use default timeout values.
    retransmission_delay =
        QuicTime::Delta::FromMilliseconds(kDefaultRetransmissionTimeMs);
  } else {
    retransmission_delay =
        rtt_stats_.smoothed_rtt() + 4 * rtt_stats_.mean_deviation();
    if (retransmission_delay < min_rto_timeout_) {
      retransmission_delay = min_rto_timeout_;
    }
  }

  // Calculate exponential back off.
  retransmission_delay =
      retransmission_delay *
      (1 << std::min<size_t>(consecutive_rto_count, kMaxRetransmissions));

  if (retransmission_delay.ToMilliseconds() > kMaxRetransmissionTimeMs) {
    return QuicTime::Delta::FromMilliseconds(kMaxRetransmissionTimeMs);
  }
  return retransmission_delay;
}

const QuicTime::Delta QuicSentPacketManager::GetProbeTimeoutDelay() const {
  DCHECK(pto_enabled_);
  if (rtt_stats_.smoothed_rtt().IsZero()) {
    if (rtt_stats_.initial_rtt().IsZero()) {
      return QuicTime::Delta::FromSeconds(1);
    }
    return 2 * rtt_stats_.initial_rtt();
  }
  const QuicTime::Delta pto_delay =
      rtt_stats_.smoothed_rtt() +
      std::max(pto_rttvar_multiplier_ * rtt_stats_.mean_deviation(),
               QuicTime::Delta::FromMilliseconds(1)) +
      (ShouldAddMaxAckDelay() ? peer_max_ack_delay_ : QuicTime::Delta::Zero());
  return pto_delay * (1 << (consecutive_pto_count_ -
                            std::min(consecutive_pto_count_,
                                     pto_exponential_backoff_start_point_)));
}

QuicTime::Delta QuicSentPacketManager::GetSlowStartDuration() const {
  if (send_algorithm_->GetCongestionControlType() == kBBR ||
      send_algorithm_->GetCongestionControlType() == kBBRv2) {
    return stats_->slowstart_duration.GetTotalElapsedTime(
        clock_->ApproximateNow());
  }
  return QuicTime::Delta::Infinite();
}

std::string QuicSentPacketManager::GetDebugState() const {
  return send_algorithm_->GetDebugState();
}

void QuicSentPacketManager::SetSendAlgorithm(
    CongestionControlType congestion_control_type) {
  SetSendAlgorithm(SendAlgorithmInterface::Create(
      clock_, &rtt_stats_, &unacked_packets_, congestion_control_type, random_,
      stats_, initial_congestion_window_));
}

void QuicSentPacketManager::SetSendAlgorithm(
    SendAlgorithmInterface* send_algorithm) {
  send_algorithm_.reset(send_algorithm);
  pacing_sender_.set_sender(send_algorithm);
}

void QuicSentPacketManager::OnConnectionMigration(AddressChangeType type) {
  if (type == PORT_CHANGE || type == IPV4_SUBNET_CHANGE) {
    // Rtt and cwnd do not need to be reset when the peer address change is
    // considered to be caused by NATs.
    return;
  }
  consecutive_rto_count_ = 0;
  consecutive_tlp_count_ = 0;
  consecutive_pto_count_ = 0;
  rtt_stats_.OnConnectionMigration();
  send_algorithm_->OnConnectionMigration();
}

void QuicSentPacketManager::OnAckFrameStart(QuicPacketNumber largest_acked,
                                            QuicTime::Delta ack_delay_time,
                                            QuicTime ack_receive_time) {
  DCHECK(packets_acked_.empty());
  DCHECK_LE(largest_acked, unacked_packets_.largest_sent_packet());
  rtt_updated_ =
      MaybeUpdateRTT(largest_acked, ack_delay_time, ack_receive_time);
  last_ack_frame_.ack_delay_time = ack_delay_time;
  acked_packets_iter_ = last_ack_frame_.packets.rbegin();
}

void QuicSentPacketManager::OnAckRange(QuicPacketNumber start,
                                       QuicPacketNumber end) {
  if (!last_ack_frame_.largest_acked.IsInitialized() ||
      end > last_ack_frame_.largest_acked + 1) {
    // Largest acked increases.
    unacked_packets_.IncreaseLargestAcked(end - 1);
    last_ack_frame_.largest_acked = end - 1;
  }
  // Drop ack ranges which ack packets below least_unacked.
  QuicPacketNumber least_unacked = unacked_packets_.GetLeastUnacked();
  if (least_unacked.IsInitialized() && end <= least_unacked) {
    return;
  }
  start = std::max(start, least_unacked);
  do {
    QuicPacketNumber newly_acked_start = start;
    if (acked_packets_iter_ != last_ack_frame_.packets.rend()) {
      newly_acked_start = std::max(start, acked_packets_iter_->max());
    }
    for (QuicPacketNumber acked = end - 1; acked >= newly_acked_start;
         --acked) {
      // Check if end is above the current range. If so add newly acked packets
      // in descending order.
      packets_acked_.push_back(AckedPacket(acked, 0, QuicTime::Zero()));
      if (acked == FirstSendingPacketNumber()) {
        break;
      }
    }
    if (acked_packets_iter_ == last_ack_frame_.packets.rend() ||
        start > acked_packets_iter_->min()) {
      // Finish adding all newly acked packets.
      return;
    }
    end = std::min(end, acked_packets_iter_->min());
    ++acked_packets_iter_;
  } while (start < end);
}

void QuicSentPacketManager::OnAckTimestamp(QuicPacketNumber packet_number,
                                           QuicTime timestamp) {
  last_ack_frame_.received_packet_times.push_back({packet_number, timestamp});
  for (AckedPacket& packet : packets_acked_) {
    if (packet.packet_number == packet_number) {
      packet.receive_timestamp = timestamp;
      return;
    }
  }
}

AckResult QuicSentPacketManager::OnAckFrameEnd(
    QuicTime ack_receive_time,
    QuicPacketNumber ack_packet_number,
    EncryptionLevel ack_decrypted_level) {
  QuicByteCount prior_bytes_in_flight = unacked_packets_.bytes_in_flight();
  // Reverse packets_acked_ so that it is in ascending order.
  std::reverse(packets_acked_.begin(), packets_acked_.end());
  for (AckedPacket& acked_packet : packets_acked_) {
    QuicTransmissionInfo* info =
        unacked_packets_.GetMutableTransmissionInfo(acked_packet.packet_number);
    if (!QuicUtils::IsAckable(info->state)) {
      if (info->state == ACKED) {
        QUIC_BUG << "Trying to ack an already acked packet: "
                 << acked_packet.packet_number
                 << ", last_ack_frame_: " << last_ack_frame_
                 << ", least_unacked: " << unacked_packets_.GetLeastUnacked()
                 << ", packets_acked_: " << packets_acked_;
      } else {
        QUIC_PEER_BUG << "Received "
                      << EncryptionLevelToString(ack_decrypted_level)
                      << " ack for unackable packet: "
                      << acked_packet.packet_number << " with state: "
                      << QuicUtils::SentPacketStateToString(info->state);
        if (supports_multiple_packet_number_spaces()) {
          if (info->state == NEVER_SENT) {
            return UNSENT_PACKETS_ACKED;
          }
          return UNACKABLE_PACKETS_ACKED;
        }
      }
      continue;
    }
    QUIC_DVLOG(1) << ENDPOINT << "Got an "
                  << EncryptionLevelToString(ack_decrypted_level)
                  << " ack for packet " << acked_packet.packet_number
                  << " , state: "
                  << QuicUtils::SentPacketStateToString(info->state);
    const PacketNumberSpace packet_number_space =
        unacked_packets_.GetPacketNumberSpace(info->encryption_level);
    if (supports_multiple_packet_number_spaces() &&
        QuicUtils::GetPacketNumberSpace(ack_decrypted_level) !=
            packet_number_space) {
      return PACKETS_ACKED_IN_WRONG_PACKET_NUMBER_SPACE;
    }
    last_ack_frame_.packets.Add(acked_packet.packet_number);
    if (info->encryption_level == ENCRYPTION_FORWARD_SECURE) {
      handshake_state_ = HANDSHAKE_CONFIRMED;
    }
    largest_packet_peer_knows_is_acked_.UpdateMax(info->largest_acked);
    if (supports_multiple_packet_number_spaces()) {
      largest_packets_peer_knows_is_acked_[packet_number_space].UpdateMax(
          info->largest_acked);
    }
    // If data is associated with the most recent transmission of this
    // packet, then inform the caller.
    if (info->in_flight) {
      acked_packet.bytes_acked = info->bytes_sent;
    } else {
      // Unackable packets are skipped earlier.
      largest_newly_acked_ = acked_packet.packet_number;
    }
    unacked_packets_.MaybeUpdateLargestAckedOfPacketNumberSpace(
        packet_number_space, acked_packet.packet_number);
    MarkPacketHandled(acked_packet.packet_number, info, ack_receive_time,
                      last_ack_frame_.ack_delay_time,
                      acked_packet.receive_timestamp);
  }
  const bool acked_new_packet = !packets_acked_.empty();
  PostProcessNewlyAckedPackets(ack_packet_number, last_ack_frame_,
                               ack_receive_time, rtt_updated_,
                               prior_bytes_in_flight);

  return acked_new_packet ? PACKETS_NEWLY_ACKED : NO_PACKETS_NEWLY_ACKED;
}

void QuicSentPacketManager::SetDebugDelegate(DebugDelegate* debug_delegate) {
  debug_delegate_ = debug_delegate;
}

void QuicSentPacketManager::OnApplicationLimited() {
  if (using_pacing_) {
    pacing_sender_.OnApplicationLimited();
  }
  send_algorithm_->OnApplicationLimited(unacked_packets_.bytes_in_flight());
  if (debug_delegate_ != nullptr) {
    debug_delegate_->OnApplicationLimited();
  }
}

QuicTime QuicSentPacketManager::GetNextReleaseTime() const {
  return using_pacing_ ? pacing_sender_.ideal_next_packet_send_time()
                       : QuicTime::Zero();
}

void QuicSentPacketManager::SetInitialRtt(QuicTime::Delta rtt) {
  const QuicTime::Delta min_rtt =
      QuicTime::Delta::FromMicroseconds(kMinInitialRoundTripTimeUs);
  const QuicTime::Delta max_rtt =
      QuicTime::Delta::FromMicroseconds(kMaxInitialRoundTripTimeUs);
  rtt_stats_.set_initial_rtt(std::max(min_rtt, std::min(max_rtt, rtt)));
}

void QuicSentPacketManager::EnableMultiplePacketNumberSpacesSupport() {
  unacked_packets_.EnableMultiplePacketNumberSpacesSupport();
}

QuicPacketNumber QuicSentPacketManager::GetLargestAckedPacket(
    EncryptionLevel decrypted_packet_level) const {
  DCHECK(supports_multiple_packet_number_spaces());
  return unacked_packets_.GetLargestAckedOfPacketNumberSpace(
      QuicUtils::GetPacketNumberSpace(decrypted_packet_level));
}

QuicPacketNumber QuicSentPacketManager::GetLargestSentPacket(
    EncryptionLevel decrypted_packet_level) const {
  DCHECK(supports_multiple_packet_number_spaces());
  return unacked_packets_.GetLargestSentPacketOfPacketNumberSpace(
      decrypted_packet_level);
}

QuicPacketNumber QuicSentPacketManager::GetLargestPacketPeerKnowsIsAcked(
    EncryptionLevel decrypted_packet_level) const {
  DCHECK(supports_multiple_packet_number_spaces());
  return largest_packets_peer_knows_is_acked_[QuicUtils::GetPacketNumberSpace(
      decrypted_packet_level)];
}

#undef ENDPOINT  // undef for jumbo builds
}  // namespace quic
