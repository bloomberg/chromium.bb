// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_OBSERVER_H_
#define CHROME_BROWSER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_OBSERVER_H_

#include "base/macros.h"
#include "chrome/common/subresource_redirect_service.mojom.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_receiver_set.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace subresource_redirect {

// Sends the public image URL hints to renderer.
class SubresourceRedirectObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SubresourceRedirectObserver>,
      public mojom::SubresourceRedirectService {
 public:
  static void MaybeCreateForWebContents(content::WebContents* web_contents);

  // Returns whether LiteMode https image compression was attempted on this
  // page.
  static bool IsHttpsImageCompressionApplied(
      content::WebContents* web_contents);

  ~SubresourceRedirectObserver() override;
  SubresourceRedirectObserver(const SubresourceRedirectObserver&) = delete;
  SubresourceRedirectObserver& operator=(const SubresourceRedirectObserver&) =
      delete;

 private:
  friend class content::WebContentsUserData<SubresourceRedirectObserver>;

  explicit SubresourceRedirectObserver(content::WebContents* web_contents);

  // content::WebContentsObserver.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // mojom::SubresourceRedirectService
  void NotifyCompressedImageFetchFailed(base::TimeDelta retry_after) override;

  // Invoked when the OptimizationGuideKeyedService has sufficient information
  // to make a decision for whether we can send resource loading image hints.
  // If |decision| is true, public image URLs contained in
  // |optimization_metadata| will be sent to the render frame host as specified
  // by |render_frame_host_routing_id| to later be compressed.
  void OnResourceLoadingImageHintsReceived(
      content::GlobalFrameRoutingId render_frame_host_routing_id,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& optimization_metadata);

  // Maintains whether https image compression was attempted for the last
  // navigation. Even though image compression was attempted, it doesn't mean at
  // least one image will get compressed, since that depends on a public image
  // present in this page. This is not an issue since most pages tend to have at
  // least one public image even though they are fully private.
  bool is_https_image_compression_applied_ = false;

  content::WebContentsFrameReceiverSet<mojom::SubresourceRedirectService>
      receivers_;

  base::WeakPtrFactory<SubresourceRedirectObserver> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace subresource_redirect

#endif  // CHROME_BROWSER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_OBSERVER_H_
