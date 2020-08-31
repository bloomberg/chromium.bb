// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_HOST_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_HOST_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "content/browser/conversions/conversion_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_receiver_set.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom.h"

namespace content {

class ConversionPageMetrics;
class RenderFrameHost;
class WebContents;

// Class responsible for listening to conversion events originating from blink,
// and verifying that they are valid. Owned by the WebContents. Lifetime is
// bound to lifetime of the WebContents.
class CONTENT_EXPORT ConversionHost : public WebContentsObserver,
                                      public blink::mojom::ConversionHost {
 public:
  static std::unique_ptr<ConversionHost> CreateForTesting(
      WebContents* web_contents,
      std::unique_ptr<ConversionManager::Provider> conversion_manager_provider);

  explicit ConversionHost(WebContents* web_contents);
  ConversionHost(const ConversionHost& other) = delete;
  ConversionHost& operator=(const ConversionHost& other) = delete;
  ~ConversionHost() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ConversionHostTest, ConversionInSubframe_BadMessage);
  FRIEND_TEST_ALL_PREFIXES(ConversionHostTest,
                           ConversionOnInsecurePage_BadMessage);
  FRIEND_TEST_ALL_PREFIXES(ConversionHostTest,
                           ConversionWithInsecureReportingOrigin_BadMessage);
  FRIEND_TEST_ALL_PREFIXES(ConversionHostTest, ValidConversion_NoBadMessage);
  FRIEND_TEST_ALL_PREFIXES(ConversionHostTest, PerPageConversionMetrics);
  FRIEND_TEST_ALL_PREFIXES(ConversionHostTest,
                           NoManager_NoPerPageConversionMetrics);

  ConversionHost(
      WebContents* web_contents,
      std::unique_ptr<ConversionManager::Provider> conversion_manager_provider);

  // blink::mojom::ConversionHost:
  void RegisterConversion(blink::mojom::ConversionPtr conversion) override;

  // WebContentsObserver:
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  // Sets the target frame on |receiver_|.
  void SetCurrentTargetFrameForTesting(RenderFrameHost* render_frame_host);

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
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_HOST_H_
