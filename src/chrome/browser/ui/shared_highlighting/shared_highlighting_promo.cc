// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/shared_highlighting/shared_highlighting_promo.h"

#include <memory>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/user_education/feature_promo_controller.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace {

void OnGetExistingSelectorsComplete(
    base::WeakPtr<FeaturePromoController> feature_promo_controller,
    const std::vector<std::string>& selectors) {
  if (feature_promo_controller && selectors.size() > 0) {
    feature_promo_controller->MaybeShowPromo(
        feature_engagement::kIPHDesktopSharedHighlightingFeature);
  }
}

}  // namespace

SharedHighlightingPromo::SharedHighlightingPromo(
    content::WebContents* web_contents,
    Browser* browser)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<SharedHighlightingPromo>(*web_contents),
      browser_(browser) {}

SharedHighlightingPromo::~SharedHighlightingPromo() = default;

void SharedHighlightingPromo::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!base::FeatureList::IsEnabled(
          feature_engagement::kIPHDesktopSharedHighlightingFeature)) {
    return;
  }

  if (HasTextFragment(validated_url.spec()))
    CheckExistingSelectors(render_frame_host);
}

void SharedHighlightingPromo::CheckExistingSelectors(
    content::RenderFrameHost* render_frame_host) {
  if (!remote_.is_bound()) {
    render_frame_host->GetMainFrame()->GetRemoteInterfaces()->GetInterface(
        remote_.BindNewPipeAndPassReceiver());
  }

  // Make sure that all the relevant things still exist - and then still use a
  // weak pointer to ensure we don't tear down the browser and its promo
  // controller before the callback returns.
  if (browser_ && browser_->window() &&
      browser_->window()->GetFeaturePromoController()) {
    remote_->GetExistingSelectors(base::BindOnce(
        &OnGetExistingSelectorsComplete,
        browser_->window()->GetFeaturePromoController()->GetAsWeakPtr()));
  }
}

bool SharedHighlightingPromo::HasTextFragment(std::string url) {
  if (url.empty())
    return false;

  return url.find(":~:text=") != std::string::npos;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SharedHighlightingPromo);
