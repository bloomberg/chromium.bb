// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_IMPL_H_

#include <stddef.h>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"

namespace base {
class TimeDelta;
class TimeTicks;
}  // namespace base

namespace content {

class AttributionManager;

// Manages a receiver set of all ongoing `AttributionDataHost`s and forwards
// events to the AttributionManager which owns `this`. Because attributionsrc
// requests may continue until after we have detached a frame, all browser
// process data needed to validate sources/triggers is frozen and stored
// alongside each receiver.
class CONTENT_EXPORT AttributionDataHostManagerImpl
    : public AttributionDataHostManager,
      public blink::mojom::AttributionDataHost {
 public:
  explicit AttributionDataHostManagerImpl(
      AttributionManager* attribution_manager);
  AttributionDataHostManagerImpl(const AttributionDataHostManager& other) =
      delete;
  AttributionDataHostManagerImpl& operator=(
      const AttributionDataHostManagerImpl& other) = delete;
  AttributionDataHostManagerImpl(AttributionDataHostManagerImpl&& other) =
      delete;
  AttributionDataHostManagerImpl& operator=(
      AttributionDataHostManagerImpl&& other) = delete;
  ~AttributionDataHostManagerImpl() override;

  // AttributionDataHostManager:
  void RegisterDataHost(
      mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
      url::Origin context_origin) override;
  bool RegisterNavigationDataHost(
      mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
      const blink::AttributionSrcToken& attribution_src_token) override;
  void NotifyNavigationRedirectRegistation(
      const blink::AttributionSrcToken& attribution_src_token,
      const std::string& header_value,
      url::Origin reporting_origin,
      const url::Origin& source_origin) override;
  void NotifyNavigationForDataHost(
      const blink::AttributionSrcToken& attribution_src_token,
      const url::Origin& source_origin,
      const url::Origin& destination_origin) override;
  void NotifyNavigationFailure(
      const blink::AttributionSrcToken& attribution_src_token) override;

 private:
  // Represents frozen data from the browser process associated with each
  // receiver.
  struct FrozenContext;
  struct DelayedTrigger;
  struct NavigationDataHost;

  // Represents a set of attribution sources which registered in a top-level
  // navigation redirect chain, and associated info to process them.
  struct NavigationRedirectSourceRegistrations;

  // blink::mojom::AttributionDataHost:
  void SourceDataAvailable(
      blink::mojom::AttributionSourceDataPtr data) override;
  void TriggerDataAvailable(
      blink::mojom::AttributionTriggerDataPtr data) override;

  void OnReceiverDisconnected();
  void OnSourceEligibleDataHostFinished(base::TimeTicks register_time);

  void OnRedirectSourceParsed(
      const blink::AttributionSrcToken& attribution_src_token,
      url::Origin reporting_origin,
      data_decoder::DataDecoder::ValueOrError result);

  void SetTriggerTimer(base::TimeDelta delay);
  void ProcessDelayedTrigger();

  // Owns `this`.
  raw_ptr<AttributionManager> attribution_manager_;

  mojo::ReceiverSet<blink::mojom::AttributionDataHost, FrozenContext>
      receivers_;

  // Map which stores pending receivers for data hosts which are going to
  // register sources associated with a navigation. These are not added to
  // `receivers_` until the necessary browser process information is available
  // to validate the attribution sources which is after the navigation finishes.
  base::flat_map<blink::AttributionSrcToken, NavigationDataHost>
      navigation_data_host_map_;

  // Stores registrations received for redirects within a navigation with a
  // given token.
  base::flat_map<blink::AttributionSrcToken,
                 NavigationRedirectSourceRegistrations>
      redirect_registrations_;

  // The number of connected receivers that may register a source. Used to
  // determine whether to buffer triggers. Event receivers are counted here
  // until they register a trigger.
  size_t data_hosts_in_source_mode_ = 0;
  base::OneShotTimer trigger_timer_;
  base::circular_deque<DelayedTrigger> delayed_triggers_;

  base::WeakPtrFactory<AttributionDataHostManagerImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_IMPL_H_
