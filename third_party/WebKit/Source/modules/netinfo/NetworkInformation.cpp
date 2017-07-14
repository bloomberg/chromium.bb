// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/netinfo/NetworkInformation.h"

#include <limits>

#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/events/Event.h"
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

// Rounds |rtt_msec| to the nearest 25 milliseconds as per the NetInfo spec.
unsigned long RoundRtt(const Optional<TimeDelta>& rtt) {
  if (!rtt.has_value()) {
    // RTT is unavailable. So, return the fastest value.
    return 0;
  }

  int rtt_msec = rtt.value().InMilliseconds();
  if (rtt.value().InMilliseconds() > std::numeric_limits<int>::max())
    rtt_msec = std::numeric_limits<int>::max();

  DCHECK_LE(0, rtt_msec);
  return std::round(static_cast<double>(rtt_msec) / 25) * 25;
}

// Rounds |downlink_mbps| to the nearest 25 kbps as per the NetInfo spec. The
// returned value is in Mbps.
double RoundMbps(const Optional<double>& downlink_mbps) {
  double downlink_kbps = 0;
  if (!downlink_mbps.has_value()) {
    // Throughput is unavailable. So, return the fastest value.
    downlink_kbps = (std::numeric_limits<double>::max());
  } else {
    downlink_kbps = downlink_mbps.value() * 1000;
  }

  DCHECK_LE(0, downlink_kbps);
  double downlink_kbps_rounded = std::round(downlink_kbps / 25) * 25;
  return downlink_kbps_rounded / 1000;
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
      context_stopped_(false) {}

DEFINE_TRACE(NetworkInformation) {
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
