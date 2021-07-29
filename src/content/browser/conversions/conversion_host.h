// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_HOST_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_HOST_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "content/browser/conversions/conversion_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_receiver_set.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom.h"

namespace content {

class ConversionPageMetrics;
class WebContents;

// Class responsible for listening to conversion events originating from blink,
// and verifying that they are valid. Owned by the WebContents. Lifetime is
// bound to lifetime of the WebContents.
class CONTENT_EXPORT ConversionHost
    : public WebContentsObserver,
      public WebContentsUserData<ConversionHost>,
      public blink::mojom::ConversionHost {
 public:
  explicit ConversionHost(WebContents* web_contents);
  ConversionHost(const ConversionHost& other) = delete;
  ConversionHost& operator=(const ConversionHost& other) = delete;
  ~ConversionHost() override;

  static absl::optional<blink::Impression> ParseImpressionFromApp(
      const std::string& attribution_source_event_id,
      const std::string& attribution_destination,
      const std::string& attribution_report_to,
      int64_t attribution_expiry) WARN_UNUSED_RESULT;

  static blink::mojom::ImpressionPtr MojoImpressionFromImpression(
      const blink::Impression& impression) WARN_UNUSED_RESULT;

 private:
  friend class ConversionHostTestPeer;
  friend class WebContentsUserData<ConversionHost>;

  // Private constructor exposed to `ConversionHostTestPeer` for testing.
  ConversionHost(
      WebContents* web_contents,
      std::unique_ptr<ConversionManager::Provider> conversion_manager_provider);

  // blink::mojom::ConversionHost:
  void RegisterConversion(blink::mojom::ConversionPtr conversion) override;
  void RegisterImpression(const blink::Impression& impression) override;

  // WebContentsObserver:
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  // Notifies an impression for a navigation.
  void NotifyImpressionNavigationInitiatedByPage();

  // Stores the impression if conversion measurement is allowed for the
  // impression origin and reporting origin and the impressionorigin, reporting
  // origin, and conversion destination are potentially trustworthy.
  void VerifyAndStoreImpression(StorableImpression::SourceType source_type,
                                const url::Origin& impression_origin,
                                const blink::Impression& impression,
                                ConversionManager& conversion_manager);

  // Map which stores the top-frame origin an impression occurred on for all
  // navigations with an associated impression, keyed by navigation ID.
  // Initiator origins are stored at navigation start time to have the best
  // chance of catching the initiating frame before it has a chance to go away.
  // Storing the origins at navigation start also prevents cases where a frame
  // initiates a navigation for itself, causing the frame to be correct but not
  // representing the frame state at the time the navigation was initiated. They
  // are stored until DidFinishNavigation, when they can be matched up with an
  // impression.
  //
  // A flat_map is used as the number of ongoing impression navigations is
  // expected to be very small in a given WebContents.
  using NavigationImpressionOriginMap = base::flat_map<int64_t, url::Origin>;
  NavigationImpressionOriginMap navigation_impression_origins_;

  // Gives access to a ConversionManager implementation to forward impressions
  // and conversion registrations to.
  std::unique_ptr<ConversionManager::Provider> conversion_manager_provider_;

  // Logs metrics per top-level page load. Created for every top level
  // navigation that commits, as long as there is a ConversionManager.
  // Excludes the initial about:blank document.
  std::unique_ptr<ConversionPageMetrics> conversion_page_metrics_;

  WebContentsFrameReceiverSet<blink::mojom::ConversionHost> receiver_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_HOST_H_
