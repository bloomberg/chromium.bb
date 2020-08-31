// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/src/quic/core/congestion_control/bbr2_probe_bw.h"

#include "net/third_party/quiche/src/quic/core/congestion_control/bbr2_misc.h"
#include "net/third_party/quiche/src/quic/core/congestion_control/bbr2_sender.h"
#include "net/third_party/quiche/src/quic/core/quic_bandwidth.h"
#include "net/third_party/quiche/src/quic/core/quic_time.h"
#include "net/third_party/quiche/src/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_logging.h"

namespace quic {

void Bbr2ProbeBwMode::Enter(QuicTime now,
                            const Bbr2CongestionEvent* /*congestion_event*/) {
  if (cycle_.phase == CyclePhase::PROBE_NOT_STARTED) {
    // First time entering PROBE_BW. Start a new probing cycle.
    EnterProbeDown(/*probed_too_high=*/false, /*stopped_risky_probe=*/false,
                   now);
  } else {
    // Transitioning from PROBE_RTT to PROBE_BW. Re-enter the last phase before
    // PROBE_RTT.
    DCHECK(cycle_.phase == CyclePhase::PROBE_CRUISE ||
           cycle_.phase == CyclePhase::PROBE_REFILL);
    cycle_.cycle_start_time = now;
    if (cycle_.phase == CyclePhase::PROBE_CRUISE) {
      EnterProbeCruise(now);
    } else if (cycle_.phase == CyclePhase::PROBE_REFILL) {
      EnterProbeRefill(cycle_.probe_up_rounds, now);
    }
  }
}

Bbr2Mode Bbr2ProbeBwMode::OnCongestionEvent(
    QuicByteCount prior_in_flight,
    QuicTime event_time,
    const AckedPacketVector& /*acked_packets*/,
    const LostPacketVector& /*lost_packets*/,
    const Bbr2CongestionEvent& congestion_event) {
  DCHECK_NE(cycle_.phase, CyclePhase::PROBE_NOT_STARTED);

  if (congestion_event.end_of_round_trip) {
    if (cycle_.cycle_start_time != event_time) {
      ++cycle_.rounds_since_probe;
    }
    if (cycle_.phase_start_time != event_time) {
      ++cycle_.rounds_in_phase;
    }
  }

  bool switch_to_probe_rtt = false;

  if (cycle_.phase == CyclePhase::PROBE_UP) {
    UpdateProbeUp(prior_in_flight, congestion_event);
  } else if (cycle_.phase == CyclePhase::PROBE_DOWN) {
    UpdateProbeDown(prior_in_flight, congestion_event);
    // Maybe transition to PROBE_RTT at the end of this cycle.
    if (cycle_.phase != CyclePhase::PROBE_DOWN &&
        model_->MaybeExpireMinRtt(congestion_event)) {
      switch_to_probe_rtt = true;
    }
  } else if (cycle_.phase == CyclePhase::PROBE_CRUISE) {
    UpdateProbeCruise(congestion_event);
  } else if (cycle_.phase == CyclePhase::PROBE_REFILL) {
    UpdateProbeRefill(congestion_event);
  }

  // Do not need to set the gains if switching to PROBE_RTT, they will be set
  // when Bbr2ProbeRttMode::Enter is called.
  if (!switch_to_probe_rtt) {
    model_->set_pacing_gain(PacingGainForPhase(cycle_.phase));
    model_->set_cwnd_gain(Params().probe_bw_cwnd_gain);
  }

  return switch_to_probe_rtt ? Bbr2Mode::PROBE_RTT : Bbr2Mode::PROBE_BW;
}

Limits<QuicByteCount> Bbr2ProbeBwMode::GetCwndLimits() const {
  if (!GetQuicReloadableFlag(quic_bbr2_avoid_too_low_probe_bw_cwnd)) {
    if (cycle_.phase == CyclePhase::PROBE_CRUISE) {
      return NoGreaterThan(
          std::min(model_->inflight_lo(), model_->inflight_hi_with_headroom()));
    }

    return NoGreaterThan(
        std::min(model_->inflight_lo(), model_->inflight_hi()));
  }

  QUIC_RELOADABLE_FLAG_COUNT(quic_bbr2_avoid_too_low_probe_bw_cwnd);

  QuicByteCount upper_limit =
      std::min(model_->inflight_lo(), cycle_.phase == CyclePhase::PROBE_CRUISE
                                          ? model_->inflight_hi_with_headroom()
                                          : model_->inflight_hi());

  if (Params().avoid_too_low_probe_bw_cwnd) {
    // Ensure upper_limit is at least BDP + AckHeight.
    QuicByteCount bdp_with_ack_height =
        model_->BDP(model_->MaxBandwidth()) + model_->MaxAckHeight();
    if (upper_limit < bdp_with_ack_height) {
      QUIC_DVLOG(3) << sender_ << " Rasing upper_limit from " << upper_limit
                    << " to " << bdp_with_ack_height;
      QUIC_CODE_COUNT(quic_bbr2_avoid_too_low_probe_bw_cwnd_in_effect);
      upper_limit = bdp_with_ack_height;
    }
  }

  return NoGreaterThan(upper_limit);
}

bool Bbr2ProbeBwMode::IsProbingForBandwidth() const {
  return cycle_.phase == CyclePhase::PROBE_REFILL ||
         cycle_.phase == CyclePhase::PROBE_UP;
}

Bbr2Mode Bbr2ProbeBwMode::OnExitQuiescence(QuicTime now,
                                           QuicTime quiescence_start_time) {
  QUIC_DVLOG(3) << sender_ << " Postponing min_rtt_timestamp("
                << model_->MinRttTimestamp() << ") by "
                << now - quiescence_start_time;
  model_->PostponeMinRttTimestamp(now - quiescence_start_time);
  return Bbr2Mode::PROBE_BW;
}

void Bbr2ProbeBwMode::UpdateProbeDown(
    QuicByteCount prior_in_flight,
    const Bbr2CongestionEvent& congestion_event) {
  DCHECK_EQ(cycle_.phase, CyclePhase::PROBE_DOWN);

  if (cycle_.rounds_in_phase == 1 && congestion_event.end_of_round_trip) {
    cycle_.is_sample_from_probing = false;

    if (!congestion_event.last_sample_is_app_limited) {
      QUIC_DVLOG(2)
          << sender_
          << " Advancing max bw filter after one round in PROBE_DOWN.";
      model_->AdvanceMaxBandwidthFilter();
      cycle_.has_advanced_max_bw = true;
    }

    if (last_cycle_stopped_risky_probe_ && !last_cycle_probed_too_high_) {
      EnterProbeRefill(/*probe_up_rounds=*/0, congestion_event.event_time);
      return;
    }
  }

  MaybeAdaptUpperBounds(congestion_event);

  if (IsTimeToProbeBandwidth(congestion_event)) {
    EnterProbeRefill(/*probe_up_rounds=*/0, congestion_event.event_time);
    return;
  }

  if (HasStayedLongEnoughInProbeDown(congestion_event)) {
    QUIC_DVLOG(3) << sender_ << " Proportional time based PROBE_DOWN exit";
    EnterProbeCruise(congestion_event.event_time);
    return;
  }

  const QuicByteCount inflight_with_headroom =
      model_->inflight_hi_with_headroom();
  QUIC_DVLOG(3)
      << sender_
      << " Checking if have enough inflight headroom. prior_in_flight:"
      << prior_in_flight
      << ", inflight_with_headroom:" << inflight_with_headroom;
  if (prior_in_flight > inflight_with_headroom) {
    // Stay in PROBE_DOWN.
    return;
  }

  // Transition to PROBE_CRUISE iff we've drained to target.
  QuicByteCount bdp = model_->BDP(model_->MaxBandwidth());
  QUIC_DVLOG(3) << sender_ << " Checking if drained to target. prior_in_flight:"
                << prior_in_flight << ", bdp:" << bdp;
  if (prior_in_flight < bdp) {
    EnterProbeCruise(congestion_event.event_time);
  }
}

Bbr2ProbeBwMode::AdaptUpperBoundsResult Bbr2ProbeBwMode::MaybeAdaptUpperBounds(
    const Bbr2CongestionEvent& congestion_event) {
  const SendTimeState& send_state = congestion_event.last_packet_send_state;
  if (!send_state.is_valid) {
    QUIC_DVLOG(3) << sender_ << " " << cycle_.phase
                  << ": NOT_ADAPTED_INVALID_SAMPLE";
    return NOT_ADAPTED_INVALID_SAMPLE;
  }

  const bool has_enough_loss_events =
      model_->loss_events_in_round() >= Params().probe_bw_full_loss_count;

  if (has_enough_loss_events && model_->IsInflightTooHigh(congestion_event)) {
    if (cycle_.is_sample_from_probing) {
      cycle_.is_sample_from_probing = false;

      if (!send_state.is_app_limited) {
        QuicByteCount inflight_at_send = BytesInFlight(send_state);

        QuicByteCount inflight_target =
            sender_->GetTargetBytesInflight() * (1.0 - Params().beta);
        if (inflight_at_send >= inflight_target) {
          // The new code does not change behavior.
          QUIC_CODE_COUNT(quic_bbr2_cut_inflight_hi_gradually_noop);
        } else {
          // The new code actually cuts inflight_hi slower than before.
          QUIC_CODE_COUNT(quic_bbr2_cut_inflight_hi_gradually_in_effect);
        }
        model_->set_inflight_hi(std::max(inflight_at_send, inflight_target));
      }

      QUIC_DVLOG(3) << sender_ << " " << cycle_.phase
                    << ": ADAPTED_PROBED_TOO_HIGH";
      return ADAPTED_PROBED_TOO_HIGH;
    }
    return ADAPTED_OK;
  }

  if (model_->inflight_hi() == model_->inflight_hi_default()) {
    QUIC_DVLOG(3) << sender_ << " " << cycle_.phase
                  << ": NOT_ADAPTED_INFLIGHT_HIGH_NOT_SET";
    return NOT_ADAPTED_INFLIGHT_HIGH_NOT_SET;
  }

  const QuicByteCount inflight_at_send = BytesInFlight(send_state);

  // Raise the upper bound for inflight.
  if (inflight_at_send > model_->inflight_hi()) {
    QUIC_DVLOG(3)
        << sender_ << " " << cycle_.phase
        << ": Adapting inflight_hi from inflight_at_send. inflight_at_send:"
        << inflight_at_send << ", old inflight_hi:" << model_->inflight_hi();
    model_->set_inflight_hi(inflight_at_send);
  }

  return ADAPTED_OK;
}

bool Bbr2ProbeBwMode::IsTimeToProbeBandwidth(
    const Bbr2CongestionEvent& congestion_event) const {
  if (HasCycleLasted(cycle_.probe_wait_time, congestion_event)) {
    return true;
  }

  if (IsTimeToProbeForRenoCoexistence(1.0, congestion_event)) {
    ++sender_->connection_stats_->bbr_num_short_cycles_for_reno_coexistence;
    return true;
  }
  return false;
}

// QUIC only. Used to prevent a Bbr2 flow from staying in PROBE_DOWN for too
// long, as seen in some multi-sender simulator tests.
bool Bbr2ProbeBwMode::HasStayedLongEnoughInProbeDown(
    const Bbr2CongestionEvent& congestion_event) const {
  // Stay in PROBE_DOWN for at most the time of a min rtt, as it is done in
  // BBRv1.
  // TODO(wub): Consider exit after a full round instead, which typically
  // indicates most(if not all) packets sent during PROBE_UP have been acked.
  return HasPhaseLasted(model_->MinRtt(), congestion_event);
}

bool Bbr2ProbeBwMode::HasCycleLasted(
    QuicTime::Delta duration,
    const Bbr2CongestionEvent& congestion_event) const {
  bool result =
      (congestion_event.event_time - cycle_.cycle_start_time) > duration;
  QUIC_DVLOG(3) << sender_ << " " << cycle_.phase
                << ": HasCycleLasted=" << result << ". elapsed:"
                << (congestion_event.event_time - cycle_.cycle_start_time)
                << ", duration:" << duration;
  return result;
}

bool Bbr2ProbeBwMode::HasPhaseLasted(
    QuicTime::Delta duration,
    const Bbr2CongestionEvent& congestion_event) const {
  bool result =
      (congestion_event.event_time - cycle_.phase_start_time) > duration;
  QUIC_DVLOG(3) << sender_ << " " << cycle_.phase
                << ": HasPhaseLasted=" << result << ". elapsed:"
                << (congestion_event.event_time - cycle_.phase_start_time)
                << ", duration:" << duration;
  return result;
}

bool Bbr2ProbeBwMode::IsTimeToProbeForRenoCoexistence(
    double probe_wait_fraction,
    const Bbr2CongestionEvent& /*congestion_event*/) const {
  uint64_t rounds = Params().probe_bw_probe_max_rounds;
  if (Params().probe_bw_probe_reno_gain > 0.0) {
    QuicByteCount target_bytes_inflight = sender_->GetTargetBytesInflight();
    uint64_t reno_rounds = Params().probe_bw_probe_reno_gain *
                           target_bytes_inflight / kDefaultTCPMSS;
    rounds = std::min(rounds, reno_rounds);
  }
  bool result = cycle_.rounds_since_probe >= (rounds * probe_wait_fraction);
  QUIC_DVLOG(3) << sender_ << " " << cycle_.phase
                << ": IsTimeToProbeForRenoCoexistence=" << result
                << ". rounds_since_probe:" << cycle_.rounds_since_probe
                << ", rounds:" << rounds
                << ", probe_wait_fraction:" << probe_wait_fraction;
  return result;
}

void Bbr2ProbeBwMode::RaiseInflightHighSlope() {
  DCHECK_EQ(cycle_.phase, CyclePhase::PROBE_UP);
  uint64_t growth_this_round = 1 << cycle_.probe_up_rounds;
  // The number 30 below means |growth_this_round| is capped at 1G and the lower
  // bound of |probe_up_bytes| is (practically) 1 mss, at this speed inflight_hi
  // grows by approximately 1 packet per packet acked.
  cycle_.probe_up_rounds = std::min<uint64_t>(cycle_.probe_up_rounds + 1, 30);
  uint64_t probe_up_bytes = sender_->GetCongestionWindow() / growth_this_round;
  cycle_.probe_up_bytes =
      std::max<QuicByteCount>(probe_up_bytes, kDefaultTCPMSS);
  QUIC_DVLOG(3) << sender_ << " Rasing inflight_hi slope. probe_up_rounds:"
                << cycle_.probe_up_rounds
                << ", probe_up_bytes:" << cycle_.probe_up_bytes;
}

void Bbr2ProbeBwMode::ProbeInflightHighUpward(
    const Bbr2CongestionEvent& congestion_event) {
  DCHECK_EQ(cycle_.phase, CyclePhase::PROBE_UP);
  if (!model_->IsCongestionWindowLimited(congestion_event)) {
    QUIC_DVLOG(3) << sender_
                  << " Raising inflight_hi early return: Not cwnd limited.";
    // Not fully utilizing cwnd, so can't safely grow.
    return;
  }

  if (congestion_event.prior_cwnd < model_->inflight_hi()) {
    QUIC_DVLOG(3)
        << sender_
        << " Raising inflight_hi early return: inflight_hi not fully used.";
    // Not fully using inflight_hi, so don't grow it.
    return;
  }

  // Increase inflight_hi by the number of probe_up_bytes within probe_up_acked.
  cycle_.probe_up_acked += congestion_event.bytes_acked;
  if (cycle_.probe_up_acked >= cycle_.probe_up_bytes) {
    uint64_t delta = cycle_.probe_up_acked / cycle_.probe_up_bytes;
    cycle_.probe_up_acked -= delta * cycle_.probe_up_bytes;
    QuicByteCount new_inflight_hi =
        model_->inflight_hi() + delta * kDefaultTCPMSS;
    if (new_inflight_hi > model_->inflight_hi()) {
      QUIC_DVLOG(3) << sender_ << " Raising inflight_hi from "
                    << model_->inflight_hi() << " to " << new_inflight_hi
                    << ". probe_up_bytes:" << cycle_.probe_up_bytes
                    << ", delta:" << delta
                    << ", (new)probe_up_acked:" << cycle_.probe_up_acked;

      model_->set_inflight_hi(new_inflight_hi);
    } else {
      QUIC_BUG << "Not growing inflight_hi due to wrap around. Old value:"
               << model_->inflight_hi() << ", new value:" << new_inflight_hi;
    }
  }

  if (congestion_event.end_of_round_trip) {
    RaiseInflightHighSlope();
  }
}

void Bbr2ProbeBwMode::UpdateProbeCruise(
    const Bbr2CongestionEvent& congestion_event) {
  DCHECK_EQ(cycle_.phase, CyclePhase::PROBE_CRUISE);
  MaybeAdaptUpperBounds(congestion_event);
  DCHECK(!cycle_.is_sample_from_probing);

  if (IsTimeToProbeBandwidth(congestion_event)) {
    EnterProbeRefill(/*probe_up_rounds=*/0, congestion_event.event_time);
    return;
  }
}

void Bbr2ProbeBwMode::UpdateProbeRefill(
    const Bbr2CongestionEvent& congestion_event) {
  DCHECK_EQ(cycle_.phase, CyclePhase::PROBE_REFILL);
  MaybeAdaptUpperBounds(congestion_event);
  DCHECK(!cycle_.is_sample_from_probing);

  if (cycle_.rounds_in_phase > 0 && congestion_event.end_of_round_trip) {
    EnterProbeUp(congestion_event.event_time);
    return;
  }
}

void Bbr2ProbeBwMode::UpdateProbeUp(
    QuicByteCount prior_in_flight,
    const Bbr2CongestionEvent& congestion_event) {
  DCHECK_EQ(cycle_.phase, CyclePhase::PROBE_UP);
  if (MaybeAdaptUpperBounds(congestion_event) == ADAPTED_PROBED_TOO_HIGH) {
    EnterProbeDown(/*probed_too_high=*/true, /*stopped_risky_probe=*/false,
                   congestion_event.event_time);
    return;
  }

  // TODO(wub): Consider exit PROBE_UP after a certain number(e.g. 64) of RTTs.

  ProbeInflightHighUpward(congestion_event);

  bool is_risky = false;
  bool is_queuing = false;
  if (last_cycle_probed_too_high_ && prior_in_flight >= model_->inflight_hi()) {
    is_risky = true;
    QUIC_DVLOG(3) << sender_
                  << " Probe is too risky. last_cycle_probed_too_high_:"
                  << last_cycle_probed_too_high_
                  << ", prior_in_flight:" << prior_in_flight
                  << ", inflight_hi:" << model_->inflight_hi();
    // TCP uses min_rtt instead of a full round:
    //   HasPhaseLasted(model_->MinRtt(), congestion_event)
  } else if (cycle_.rounds_in_phase > 0) {
    const QuicByteCount bdp = model_->BDP(model_->MaxBandwidth());
    QuicByteCount queuing_threshold_extra_bytes = 2 * kDefaultTCPMSS;
    if (Params().add_ack_height_to_queueing_threshold) {
      QUIC_RELOADABLE_FLAG_COUNT(
          quic_bbr2_add_ack_height_to_queueing_threshold);
      queuing_threshold_extra_bytes += model_->MaxAckHeight();
    }
    QuicByteCount queuing_threshold =
        (Params().probe_bw_probe_inflight_gain * bdp) +
        queuing_threshold_extra_bytes;
    is_queuing = prior_in_flight >= queuing_threshold;
    QUIC_DVLOG(3) << sender_
                  << " Checking if building up a queue. prior_in_flight:"
                  << prior_in_flight << ", threshold:" << queuing_threshold
                  << ", is_queuing:" << is_queuing
                  << ", max_bw:" << model_->MaxBandwidth()
                  << ", min_rtt:" << model_->MinRtt();
  }

  if (is_risky || is_queuing) {
    EnterProbeDown(/*probed_too_high=*/false, /*stopped_risky_probe=*/is_risky,
                   congestion_event.event_time);
  }
}

void Bbr2ProbeBwMode::EnterProbeDown(bool probed_too_high,
                                     bool stopped_risky_probe,
                                     QuicTime now) {
  QUIC_DVLOG(2) << sender_ << " Phase change: " << cycle_.phase << " ==> "
                << CyclePhase::PROBE_DOWN << " after "
                << now - cycle_.phase_start_time << ", or "
                << cycle_.rounds_in_phase
                << " rounds. probed_too_high:" << probed_too_high
                << ", stopped_risky_probe:" << stopped_risky_probe << "  @ "
                << now;
  last_cycle_probed_too_high_ = probed_too_high;
  last_cycle_stopped_risky_probe_ = stopped_risky_probe;

  cycle_.cycle_start_time = now;
  cycle_.phase = CyclePhase::PROBE_DOWN;
  cycle_.rounds_in_phase = 0;
  cycle_.phase_start_time = now;
  ++sender_->connection_stats_->bbr_num_cycles;

  // Pick probe wait time.
  cycle_.rounds_since_probe =
      sender_->RandomUint64(Params().probe_bw_max_probe_rand_rounds);
  cycle_.probe_wait_time =
      Params().probe_bw_probe_base_duration +
      QuicTime::Delta::FromMicroseconds(sender_->RandomUint64(
          Params().probe_bw_probe_max_rand_duration.ToMicroseconds()));

  cycle_.probe_up_bytes = std::numeric_limits<QuicByteCount>::max();
  cycle_.has_advanced_max_bw = false;
  model_->RestartRound();
}

void Bbr2ProbeBwMode::EnterProbeCruise(QuicTime now) {
  if (cycle_.phase == CyclePhase::PROBE_DOWN) {
    ExitProbeDown();
  }
  QUIC_DVLOG(2) << sender_ << " Phase change: " << cycle_.phase << " ==> "
                << CyclePhase::PROBE_CRUISE << " after "
                << now - cycle_.phase_start_time << ", or "
                << cycle_.rounds_in_phase << " rounds.  @ " << now;

  model_->cap_inflight_lo(model_->inflight_hi());
  cycle_.phase = CyclePhase::PROBE_CRUISE;
  cycle_.rounds_in_phase = 0;
  cycle_.phase_start_time = now;
  cycle_.is_sample_from_probing = false;
}

void Bbr2ProbeBwMode::EnterProbeRefill(uint64_t probe_up_rounds, QuicTime now) {
  if (cycle_.phase == CyclePhase::PROBE_DOWN) {
    ExitProbeDown();
  }
  QUIC_DVLOG(2) << sender_ << " Phase change: " << cycle_.phase << " ==> "
                << CyclePhase::PROBE_REFILL << " after "
                << now - cycle_.phase_start_time << ", or "
                << cycle_.rounds_in_phase
                << " rounds. probe_up_rounds:" << probe_up_rounds << "  @ "
                << now;
  cycle_.phase = CyclePhase::PROBE_REFILL;
  cycle_.rounds_in_phase = 0;
  cycle_.phase_start_time = now;
  cycle_.is_sample_from_probing = false;
  last_cycle_stopped_risky_probe_ = false;

  model_->clear_bandwidth_lo();
  model_->clear_inflight_lo();
  cycle_.probe_up_rounds = probe_up_rounds;
  cycle_.probe_up_acked = 0;
  model_->RestartRound();
}

void Bbr2ProbeBwMode::EnterProbeUp(QuicTime now) {
  DCHECK_EQ(cycle_.phase, CyclePhase::PROBE_REFILL);
  QUIC_DVLOG(2) << sender_ << " Phase change: " << cycle_.phase << " ==> "
                << CyclePhase::PROBE_UP << " after "
                << now - cycle_.phase_start_time << ", or "
                << cycle_.rounds_in_phase << " rounds.  @ " << now;
  cycle_.phase = CyclePhase::PROBE_UP;
  cycle_.rounds_in_phase = 0;
  cycle_.phase_start_time = now;
  cycle_.is_sample_from_probing = true;
  RaiseInflightHighSlope();

  model_->RestartRound();
}

void Bbr2ProbeBwMode::ExitProbeDown() {
  DCHECK_EQ(cycle_.phase, CyclePhase::PROBE_DOWN);
  if (!cycle_.has_advanced_max_bw) {
    QUIC_DVLOG(2) << sender_ << " Advancing max bw filter at end of cycle.";
    model_->AdvanceMaxBandwidthFilter();
    cycle_.has_advanced_max_bw = true;
  }
}

// static
const char* Bbr2ProbeBwMode::CyclePhaseToString(CyclePhase phase) {
  switch (phase) {
    case CyclePhase::PROBE_NOT_STARTED:
      return "PROBE_NOT_STARTED";
    case CyclePhase::PROBE_UP:
      return "PROBE_UP";
    case CyclePhase::PROBE_DOWN:
      return "PROBE_DOWN";
    case CyclePhase::PROBE_CRUISE:
      return "PROBE_CRUISE";
    case CyclePhase::PROBE_REFILL:
      return "PROBE_REFILL";
    default:
      break;
  }
  return "<Invalid CyclePhase>";
}

std::ostream& operator<<(std::ostream& os,
                         const Bbr2ProbeBwMode::CyclePhase phase) {
  return os << Bbr2ProbeBwMode::CyclePhaseToString(phase);
}

Bbr2ProbeBwMode::DebugState Bbr2ProbeBwMode::ExportDebugState() const {
  DebugState s;
  s.phase = cycle_.phase;
  s.cycle_start_time = cycle_.cycle_start_time;
  s.phase_start_time = cycle_.phase_start_time;
  return s;
}

std::ostream& operator<<(std::ostream& os,
                         const Bbr2ProbeBwMode::DebugState& state) {
  os << "[PROBE_BW] phase: " << state.phase << "\n";
  os << "[PROBE_BW] cycle_start_time: " << state.cycle_start_time << "\n";
  os << "[PROBE_BW] phase_start_time: " << state.phase_start_time << "\n";
  return os;
}

const Bbr2Params& Bbr2ProbeBwMode::Params() const {
  return sender_->Params();
}

float Bbr2ProbeBwMode::PacingGainForPhase(
    Bbr2ProbeBwMode::CyclePhase phase) const {
  if (phase == Bbr2ProbeBwMode::CyclePhase::PROBE_UP) {
    return Params().probe_bw_probe_up_pacing_gain;
  }
  if (phase == Bbr2ProbeBwMode::CyclePhase::PROBE_DOWN) {
    return Params().probe_bw_probe_down_pacing_gain;
  }
  return Params().probe_bw_default_pacing_gain;
}

}  // namespace quic
