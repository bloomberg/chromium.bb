// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/url_loader_throttles.h"

#include "base/feature_list.h"
#include "components/variations/net/omnibox_url_loader_throttle.h"
#include "components/variations/net/variations_url_loader_throttle.h"
#include "content/browser/client_hints/client_hints.h"
#include "content/browser/client_hints/critical_client_hints_throttle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/parsed_headers.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace content {

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
CreateContentBrowserURLLoaderThrottles(
    const network::ResourceRequest& request,
    BrowserContext* browser_context,
    const base::RepeatingCallback<WebContents*()>& wc_getter,
    NavigationUIData* navigation_ui_data,
    int frame_tree_node_id) {
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles =
      GetContentClient()->browser()->CreateURLLoaderThrottles(
          request, browser_context, wc_getter, navigation_ui_data,
          frame_tree_node_id);
  variations::OmniboxURLLoaderThrottle::AppendThrottleIfNeeded(&throttles);
  // TODO(crbug.com/1094303): Consider whether we want to use the WebContents to
  // determine the value for variations::Owner. Alternatively, this is the
  // browser side, and we might be fine with Owner::kUnknown.
  variations::VariationsURLLoaderThrottle::AppendThrottleIfNeeded(
      browser_context->GetVariationsClient(), &throttles);

  ClientHintsControllerDelegate* client_hint_delegate =
      browser_context->GetClientHintsControllerDelegate();
  // TODO(bokan): How to handle client hints in a fenced frame is still an open
  // question, see:
  // https://github.com/WICG/fenced-frame/blob/master/explainer/permission_document_policies.md#ua-client-hints-open-question
  if (base::FeatureList::IsEnabled(features::kCriticalClientHint) &&
      net::HttpUtil::IsMethodSafe(request.method) &&
      request.is_outermost_main_frame && client_hint_delegate) {
    throttles.push_back(std::make_unique<CriticalClientHintsThrottle>(
        browser_context, client_hint_delegate, frame_tree_node_id));
  }
  return throttles;
}

}  // namespace content
