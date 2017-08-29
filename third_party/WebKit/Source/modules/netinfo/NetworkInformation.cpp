// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/netinfo/NetworkInformation.h"

#include <algorithm>

#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/dom/events/Event.h"
#include "modules/EventTargetModules.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

namespace {

String ConnectionTypeToString(WebConnectionType type) {
  switch (type) {
    case kWebConnectionTypeCellular2G:
    case kWebConnectionTypeCellular3G:
    case kWebConnectionTypeCellular4G:
      return "cellular";
    case kWebConnectionTypeBluetooth:
      return "bluetooth";
    case kWebConnectionTypeEthernet:
      return "ethernet";
    case kWebConnectionTypeWifi:
      return "wifi";
    case kWebConnectionTypeWimax:
      return "wimax";
    case kWebConnectionTypeOther:
      return "other";
    case kWebConnectionTypeNone:
      return "none";
    case kWebConnectionTypeUnknown:
      return "unknown";
  }
  NOTREACHED();
  return "none";
}

String EffectiveConnectionTypeToString(WebEffectiveConnectionType type) {
  switch (type) {
    case WebEffectiveConnectionType::kTypeUnknown:
    case WebEffectiveConnectionType::kTypeOffline:
    case WebEffectiveConnectionType::kType4G:
      return "4g";
    case WebEffectiveConnectionType::kTypeSlow2G:
      return "slow-2g";
    case WebEffectiveConnectionType::kType2G:
      return "2g";
    case WebEffectiveConnectionType::kType3G:
      return "3g";
  }
  NOTREACHED();
  return "4g";
}

}  // namespace

NetworkInformation* NetworkInformation::Create(ExecutionContext* context) {
  return new NetworkInformation(context);
}

NetworkInformation::~NetworkInformation() {
  DCHECK(!observing_);
}

String NetworkInformation::type() const {
  // type_ is only updated when listening for events, so ask
  // networkStateNotifier if not listening (crbug.com/379841).
  if (!observing_)
    return ConnectionTypeToString(GetNetworkStateNotifier().ConnectionType());

  // If observing, return m_type which changes when the event fires, per spec.
  return ConnectionTypeToString(type_);
}

double NetworkInformation::downlinkMax() const {
  if (!observing_)
    return GetNetworkStateNotifier().MaxBandwidth();

  return downlink_max_mbps_;
}

String NetworkInformation::effectiveType() const {
  // effective_type_ is only updated when listening for events, so ask
  // networkStateNotifier if not listening (crbug.com/379841).
  if (!observing_) {
    return EffectiveConnectionTypeToString(
        GetNetworkStateNotifier().EffectiveType());
  }

  // If observing, return m_type which changes when the event fires, per spec.
  return EffectiveConnectionTypeToString(effective_type_);
}

unsigned long NetworkInformation::rtt() const {
  if (!observing_)
    return RoundRtt(GetNetworkStateNotifier().HttpRtt());

  return http_rtt_msec_;
}

double NetworkInformation::downlink() const {
  if (!observing_)
    return RoundMbps(GetNetworkStateNotifier().DownlinkThroughputMbps());

  return downlink_mbps_;
}

void NetworkInformation::ConnectionChange(
    WebConnectionType type,
    double downlink_max_mbps,
    WebEffectiveConnectionType effective_type,
    const Optional<TimeDelta>& http_rtt,
    const Optional<TimeDelta>& transport_rtt,
    const Optional<double>& downlink_mbps) {
  DCHECK(GetExecutionContext()->IsContextThread());

  unsigned long new_http_rtt_msec = RoundRtt(http_rtt);
  double new_downlink_mbps = RoundMbps(downlink_mbps);

  // This can happen if the observer removes and then adds itself again
  // during notification, or if |transport_rtt| was the only metric that
  // changed.
  if (type_ == type && downlink_max_mbps_ == downlink_max_mbps &&
      effective_type_ == effective_type &&
      http_rtt_msec_ == new_http_rtt_msec &&
      downlink_mbps_ == new_downlink_mbps) {
    return;
  }

  if (!RuntimeEnabledFeatures::NetInfoDownlinkMaxEnabled() &&
      effective_type_ == effective_type &&
      http_rtt_msec_ == new_http_rtt_msec &&
      downlink_mbps_ == new_downlink_mbps) {
    return;
  }

  type_ = type;
  downlink_max_mbps_ = downlink_max_mbps;
  effective_type_ = effective_type;
  http_rtt_msec_ = new_http_rtt_msec;
  downlink_mbps_ = new_downlink_mbps;
  DispatchEvent(Event::Create(EventTypeNames::typechange));

  if (RuntimeEnabledFeatures::NetInfoDownlinkMaxEnabled())
    DispatchEvent(Event::Create(EventTypeNames::change));
}

const AtomicString& NetworkInformation::InterfaceName() const {
  return EventTargetNames::NetworkInformation;
}

ExecutionContext* NetworkInformation::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

void NetworkInformation::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::AddedEventListener(event_type,
                                                registered_listener);
  StartObserving();
}

void NetworkInformation::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::RemovedEventListener(event_type,
                                                  registered_listener);
  if (!HasEventListeners())
    StopObserving();
}

void NetworkInformation::RemoveAllEventListeners() {
  EventTargetWithInlineData::RemoveAllEventListeners();
  DCHECK(!HasEventListeners());
  StopObserving();
}

bool NetworkInformation::HasPendingActivity() const {
  DCHECK(context_stopped_ || observing_ == HasEventListeners());

  // Prevent collection of this object when there are active listeners.
  return observing_;
}

void NetworkInformation::ContextDestroyed(ExecutionContext*) {
  context_stopped_ = true;
  StopObserving();
}

void NetworkInformation::StartObserving() {
  if (!observing_ && !context_stopped_) {
    type_ = GetNetworkStateNotifier().ConnectionType();
    GetNetworkStateNotifier().AddConnectionObserver(
        this,
        TaskRunnerHelper::Get(TaskType::kNetworking, GetExecutionContext()));
    observing_ = true;
  }
}

void NetworkInformation::StopObserving() {
  if (observing_) {
    GetNetworkStateNotifier().RemoveConnectionObserver(
        this,
        TaskRunnerHelper::Get(TaskType::kNetworking, GetExecutionContext()));
    observing_ = false;
  }
}

NetworkInformation::NetworkInformation(ExecutionContext* context)
    : ContextLifecycleObserver(context),
      type_(GetNetworkStateNotifier().ConnectionType()),
      downlink_max_mbps_(GetNetworkStateNotifier().MaxBandwidth()),
      effective_type_(GetNetworkStateNotifier().EffectiveType()),
      http_rtt_msec_(RoundRtt(GetNetworkStateNotifier().HttpRtt())),
      downlink_mbps_(
          RoundMbps(GetNetworkStateNotifier().DownlinkThroughputMbps())),
      observing_(false),
      context_stopped_(false) {
  DCHECK_LE(1u, GetNetworkStateNotifier().RandomizationSalt());
  DCHECK_GE(20u, GetNetworkStateNotifier().RandomizationSalt());
}

DEFINE_TRACE(NetworkInformation) {
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

double NetworkInformation::GetRandomMultiplier() const {
  if (!GetExecutionContext())
    return 0.0;

  // The random number should be a function of the hostname to reduce
  // cross-origin fingerprinting. The random number should also be a function
  // of randomized salt which is known only to the device. This prevents
  // origin from removing noise from the estimates.
  unsigned hash = StringHash::GetHash(GetExecutionContext()->Url().Host()) +
                  GetNetworkStateNotifier().RandomizationSalt();
  double random_multiplier = 0.9 + static_cast<double>((hash % 21)) * 0.01;
  DCHECK_LE(0.90, random_multiplier);
  DCHECK_GE(1.10, random_multiplier);
  return random_multiplier;
}

unsigned long NetworkInformation::RoundRtt(
    const Optional<TimeDelta>& rtt) const {
  // Limit the size of the buckets and the maximum reported value to reduce
  // fingerprinting.
  static const size_t kBucketSize = 50;
  static const double kMaxRttMsec = 3.0 * 1000;

  if (!rtt.has_value() || !GetExecutionContext()) {
    // RTT is unavailable. So, return the fastest value.
    return 0;
  }

  double rtt_msec = static_cast<double>(rtt.value().InMilliseconds());
  rtt_msec *= GetRandomMultiplier();
  rtt_msec = std::min(rtt_msec, kMaxRttMsec);

  DCHECK_LE(0, rtt_msec);
  DCHECK_GE(kMaxRttMsec, rtt_msec);

  // Round down to the nearest kBucketSize msec value.
  return std::round(rtt_msec / kBucketSize) * kBucketSize;
}

double NetworkInformation::RoundMbps(
    const Optional<double>& downlink_mbps) const {
  // Limit the size of the buckets and the maximum reported value to reduce
  // fingerprinting.
  static const size_t kBucketSize = 50;
  static const double kMaxDownlinkKbps = 10.0 * 1000;

  double downlink_kbps = 0;
  if (!downlink_mbps.has_value()) {
    // Throughput is unavailable. So, return the fastest value.
    downlink_kbps = kMaxDownlinkKbps;
  } else {
    downlink_kbps = downlink_mbps.value() * 1000;
  }
  downlink_kbps *= GetRandomMultiplier();

  downlink_kbps = std::min(downlink_kbps, kMaxDownlinkKbps);

  DCHECK_LE(0, downlink_kbps);
  DCHECK_GE(kMaxDownlinkKbps, downlink_kbps);
  // Round down to the nearest kBucketSize kbps value.
  double downlink_kbps_rounded =
      std::round(downlink_kbps / kBucketSize) * kBucketSize;

  // Convert from Kbps to Mbps.
  return downlink_kbps_rounded / 1000;
}

}  // namespace blink
