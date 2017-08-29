// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NetworkInformation_h
#define NetworkInformation_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/events/EventTarget.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/network/NetworkStateNotifier.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/Time.h"
#include "public/platform/WebConnectionType.h"
#include "public/platform/WebEffectiveConnectionType.h"

namespace blink {

class ExecutionContext;

class NetworkInformation final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<NetworkInformation>,
      public ContextLifecycleObserver,
      public NetworkStateNotifier::NetworkStateObserver {
  USING_GARBAGE_COLLECTED_MIXIN(NetworkInformation);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static NetworkInformation* Create(ExecutionContext*);
  ~NetworkInformation() override;

  String type() const;
  double downlinkMax() const;
  String effectiveType() const;
  unsigned long rtt() const;
  double downlink() const;

  // NetworkStateObserver overrides.
  void ConnectionChange(WebConnectionType,
                        double downlink_max_mbps,
                        WebEffectiveConnectionType effective_type,
                        const Optional<TimeDelta>& http_rtt,
                        const Optional<TimeDelta>& transport_rtt,
                        const Optional<double>& downlink_mbps) override;

  // EventTarget overrides.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;
  void RemoveAllEventListeners() override;

  // ScriptWrappable
  bool HasPendingActivity() const final;

  // ContextLifecycleObserver overrides.
  void ContextDestroyed(ExecutionContext*) override;

  DECLARE_VIRTUAL_TRACE();

  DEFINE_ATTRIBUTE_EVENT_LISTENER(change);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(typechange);  // Deprecated

 protected:
  // EventTarget overrides.
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) final;
  void RemovedEventListener(const AtomicString& event_type,
                            const RegisteredEventListener&) final;

 private:
  explicit NetworkInformation(ExecutionContext*);
  void StartObserving();
  void StopObserving();

  // A random number by which the RTT and downlink estimates are multiplied
  // with. Adding this noise reduces the chances of cross-origin fingerprinting.
  double GetRandomMultiplier() const;

  // Rounds |rtt| as per the NetInfo spec and to improve privacy.
  unsigned long RoundRtt(const Optional<TimeDelta>& rtt) const;

  // Rounds |downlink_mbps| as per the NetInfo spec and to improve privacy. The
  // returned value is in Mbps.
  double RoundMbps(const Optional<double>& downlink_mbps) const;

  // Touched only on context thread.
  WebConnectionType type_;

  // Touched only on context thread.
  double downlink_max_mbps_;

  // Current effective connection type, which is the connection type whose
  // typical performance is most similar to the measured performance of the
  // network in use.
  WebEffectiveConnectionType effective_type_;

  // HTTP RTT estimate. Rounded off to the nearest 25 msec. Touched only on
  // context thread.
  unsigned long http_rtt_msec_;

  // Downlink throughput estimate. Rounded off to the nearest 25 kbps. Touched
  // only on context thread.
  double downlink_mbps_;

  // Whether this object is listening for events from NetworkStateNotifier.
  bool observing_;

  // Whether ContextLifecycleObserver::contextDestroyed has been called.
  bool context_stopped_;
};

}  // namespace blink

#endif  // NetworkInformation_h
