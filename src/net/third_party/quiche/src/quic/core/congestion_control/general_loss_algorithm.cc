// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/src/quic/core/congestion_control/general_loss_algorithm.h"

#include "net/third_party/quiche/src/quic/core/congestion_control/rtt_stats.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_bug_tracker.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flag_utils.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"

namespace quic {

namespace {

// The minimum delay before a packet will be considered lost,
// regardless of SRTT.  Half of the minimum TLP, since the loss algorithm only
// triggers when a nack has been receieved for the packet.
static const size_t kMinLossDelayMs = 5;

// Default fraction (1/8) of an RTT when doing IETF loss detection.
static const int kDefaultIetfLossDelayShift = 3;
// Default fraction (1/16) of an RTT when doing adaptive loss detection.
static const int kDefaultAdaptiveLossDelayShift = 4;

}  // namespace

GeneralLossAlgorithm::GeneralLossAlgorithm() : GeneralLossAlgorithm(kNack) {}

GeneralLossAlgorithm::GeneralLossAlgorithm(LossDetectionType loss_type)
    : loss_detection_timeout_(QuicTime::Zero()),
      reordering_threshold_(kNumberOfNacksBeforeRetransmission),
      use_adaptive_reordering_threshold_(false),
      use_adaptive_time_threshold_(false),
      least_in_flight_(1),
      packet_number_space_(NUM_PACKET_NUMBER_SPACES) {
  SetLossDetectionType(loss_type);
}

void GeneralLossAlgorithm::SetLossDetectionType(LossDetectionType loss_type) {
  loss_detection_timeout_ = QuicTime::Zero();
  loss_type_ = loss_type;
  if (loss_type == kAdaptiveTime) {
    reordering_shift_ = kDefaultAdaptiveLossDelayShift;
  } else if (loss_type == kIetfLossDetection) {
    reordering_shift_ = kDefaultIetfLossDelayShift;
  } else {
    reordering_shift_ = kDefaultLossDelayShift;
  }
  largest_previously_acked_.Clear();
}

LossDetectionType GeneralLossAlgorithm::GetLossDetectionType() const {
  return loss_type_;
}

// Uses nack counts to decide when packets are lost.
void GeneralLossAlgorithm::DetectLosses(
    const QuicUnackedPacketMap& unacked_packets,
    QuicTime time,
    const RttStats& rtt_stats,
    QuicPacketNumber largest_newly_acked,
    const AckedPacketVector& packets_acked,
    LostPacketVector* packets_lost) {
  loss_detection_timeout_ = QuicTime::Zero();
  if (!packets_acked.empty() &&
      packets_acked.front().packet_number == least_in_flight_) {
    if (packets_acked.back().packet_number == largest_newly_acked &&
        least_in_flight_ + packets_acked.size() - 1 == largest_newly_acked) {
      // Optimization for the case when no packet is missing. Please note,
      // packets_acked can include packets of different packet number space, so
      // do not use this optimization if largest_newly_acked is not the largest
      // packet in packets_acked.
      least_in_flight_ = largest_newly_acked + 1;
      largest_previously_acked_ = largest_newly_acked;
      return;
    }
    // There is hole in acked_packets, increment least_in_flight_ if possible.
    for (const auto& acked : packets_acked) {
      if (acked.packet_number != least_in_flight_) {
        break;
      }
      ++least_in_flight_;
    }
  }
  QuicTime::Delta max_rtt =
      std::max(rtt_stats.previous_srtt(), rtt_stats.latest_rtt());
  if (loss_type_ == kIetfLossDetection) {
    max_rtt = std::max(QuicTime::Delta::FromMilliseconds(1), max_rtt);
  }
  QuicTime::Delta loss_delay = max_rtt + (max_rtt >> reordering_shift_);
  if (loss_type_ != kIetfLossDetection) {
    loss_delay = std::max(QuicTime::Delta::FromMilliseconds(kMinLossDelayMs),
                          loss_delay);
  }
  QuicPacketNumber packet_number = unacked_packets.GetLeastUnacked();
  auto it = unacked_packets.begin();
  if (least_in_flight_.IsInitialized() && least_in_flight_ >= packet_number) {
    if (least_in_flight_ > unacked_packets.largest_sent_packet() + 1) {
      QUIC_BUG << "least_in_flight: " << least_in_flight_
               << " is greater than largest_sent_packet + 1: "
               << unacked_packets.largest_sent_packet() + 1;
    } else {
      it += (least_in_flight_ - packet_number);
      packet_number = least_in_flight_;
    }
  }
  // Clear least_in_flight_.
  least_in_flight_.Clear();
  DCHECK_EQ(packet_number_space_,
            unacked_packets.GetPacketNumberSpace(largest_newly_acked));
  for (; it != unacked_packets.end() && packet_number <= largest_newly_acked;
       ++it, ++packet_number) {
    if (unacked_packets.GetPacketNumberSpace(it->encryption_level) !=
        packet_number_space_) {
      // Skip packets of different packet number space.
      continue;
    }
    if (!it->in_flight) {
      continue;
    }

    if (loss_type_ == kNack || loss_type_ == kIetfLossDetection) {
      // Packet threshold loss detection.
      if (largest_newly_acked - packet_number >= reordering_threshold_) {
        packets_lost->push_back(LostPacket(packet_number, it->bytes_sent));
        continue;
      }
    } else if (loss_type_ == kLazyFack) {
      // Require two in order acks to invoke FACK, which avoids spuriously
      // retransmitting packets when one packet is reordered by a large amount.
      if (largest_previously_acked_.IsInitialized() &&
          largest_newly_acked > largest_previously_acked_ &&
          largest_previously_acked_ > packet_number &&
          largest_previously_acked_ - packet_number >=
              (kNumberOfNacksBeforeRetransmission - 1)) {
        packets_lost->push_back(LostPacket(packet_number, it->bytes_sent));
        continue;
      }
    }

    // Time threshold loss detection. Also implements early retransmit(RFC5827)
    // when time threshold is not used and the last packet gets acked.
    QuicPacketNumber largest_sent_retransmittable_packet =
        unacked_packets.GetLargestSentRetransmittableOfPacketNumberSpace(
            packet_number_space_);
    if (largest_sent_retransmittable_packet <= largest_newly_acked ||
        loss_type_ == kTime || loss_type_ == kAdaptiveTime ||
        loss_type_ == kIetfLossDetection) {
      QuicTime when_lost = it->sent_time + loss_delay;
      if (time < when_lost) {
        loss_detection_timeout_ = when_lost;
        if (!least_in_flight_.IsInitialized()) {
          // At this point, packet_number is in flight and not detected as lost.
          least_in_flight_ = packet_number;
        }
        break;
      }
      packets_lost->push_back(LostPacket(packet_number, it->bytes_sent));
      continue;
    }

    // NACK-based loss detection allows for a max reordering window of 1 RTT.
    if (loss_type_ != kIetfLossDetection &&
        it->sent_time + rtt_stats.smoothed_rtt() <
            unacked_packets.GetTransmissionInfo(largest_newly_acked)
                .sent_time) {
      packets_lost->push_back(LostPacket(packet_number, it->bytes_sent));
      continue;
    }
    if (!least_in_flight_.IsInitialized()) {
      // At this point, packet_number is in flight and not detected as lost.
      least_in_flight_ = packet_number;
    }
  }
  if (!least_in_flight_.IsInitialized()) {
    // There is no in flight packet.
    least_in_flight_ = largest_newly_acked + 1;
  }
  largest_previously_acked_ = largest_newly_acked;
}

QuicTime GeneralLossAlgorithm::GetLossTimeout() const {
  return loss_detection_timeout_;
}

void GeneralLossAlgorithm::SpuriousLossDetected(
    const QuicUnackedPacketMap& unacked_packets,
    const RttStats& rtt_stats,
    QuicTime ack_receive_time,
    QuicPacketNumber packet_number,
    QuicPacketNumber previous_largest_acked) {
  if ((loss_type_ == kAdaptiveTime || use_adaptive_time_threshold_) &&
      reordering_shift_ > 0) {
    // Increase reordering fraction such that the packet would not have been
    // declared lost.
    QuicTime::Delta time_needed =
        ack_receive_time -
        unacked_packets.GetTransmissionInfo(packet_number).sent_time;
    QuicTime::Delta max_rtt =
        std::max(rtt_stats.previous_srtt(), rtt_stats.latest_rtt());
    while (max_rtt + (max_rtt >> reordering_shift_) < time_needed &&
           reordering_shift_ > 0) {
      --reordering_shift_;
    }
  }

  if (use_adaptive_reordering_threshold_) {
    DCHECK_LT(packet_number, previous_largest_acked);
    // Increase reordering_threshold_ such that packet_number would not have
    // been declared lost.
    reordering_threshold_ = std::max(
        reordering_threshold_, previous_largest_acked - packet_number + 1);
  }
}

void GeneralLossAlgorithm::SetPacketNumberSpace(
    PacketNumberSpace packet_number_space) {
  if (packet_number_space_ < NUM_PACKET_NUMBER_SPACES) {
    QUIC_BUG << "Cannot switch packet_number_space";
    return;
  }

  packet_number_space_ = packet_number_space;
}

}  // namespace quic
