// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/netinfo/NetworkInformation.h"

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

}  // namespace

NetworkInformation* NetworkInformation::Create(ExecutionContext* context) {
  return new NetworkInformation(context);
}

NetworkInformation::~NetworkInformation() {
  DCHECK(!observing_);
}

String NetworkInformation::type() const {
  // m_type is only updated when listening for events, so ask
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

void NetworkInformation::ConnectionChange(WebConnectionType type,
                                          double downlink_max_mbps) {
  DCHECK(GetExecutionContext()->IsContextThread());

  // This can happen if the observer removes and then adds itself again
  // during notification.
  if (type_ == type && downlink_max_mbps_ == downlink_max_mbps)
    return;

  type_ = type;
  downlink_max_mbps_ = downlink_max_mbps;
  DispatchEvent(Event::Create(EventTypeNames::typechange));

  if (RuntimeEnabledFeatures::netInfoDownlinkMaxEnabled())
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
      observing_(false),
      context_stopped_(false) {}

DEFINE_TRACE(NetworkInformation) {
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
