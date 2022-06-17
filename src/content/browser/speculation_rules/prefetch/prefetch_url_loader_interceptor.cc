// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speculation_rules/prefetch/prefetch_url_loader_interceptor.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "content/browser/browser_context_impl.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/loader/single_request_url_loader_factory.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/speculation_rules/prefetch/prefetch_container.h"
#include "content/browser/speculation_rules/prefetch/prefetch_features.h"
#include "content/browser/speculation_rules/prefetch/prefetch_from_string_url_loader.h"
#include "content/browser/speculation_rules/prefetch/prefetch_params.h"
#include "content/browser/speculation_rules/prefetch/prefetch_service.h"
#include "content/browser/speculation_rules/prefetch/prefetched_mainframe_response_container.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace content {
namespace {

BrowserContext* BrowserContextFromFrameTreeNodeId(int frame_tree_node_id) {
  WebContents* web_content =
      WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_content)
    return nullptr;
  return web_content->GetBrowserContext();
}

PrefetchService* PrefetchServiceFromFrameTreeNodeId(int frame_tree_node_id) {
  BrowserContext* browser_context =
      BrowserContextFromFrameTreeNodeId(frame_tree_node_id);
  if (!browser_context)
    return nullptr;
  return BrowserContextImpl::From(browser_context)->GetPrefetchService();
}

void RecordCookieWaitTime(base::TimeDelta wait_time) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", wait_time,
      base::TimeDelta(), base::Seconds(5), 50);
}

}  // namespace

// static
std::unique_ptr<PrefetchURLLoaderInterceptor>
PrefetchURLLoaderInterceptor::MaybeCreateInterceptor(int frame_tree_node_id) {
  if (!base::FeatureList::IsEnabled(features::kPrefetchUseContentRefactor))
    return nullptr;

  return std::make_unique<PrefetchURLLoaderInterceptor>(frame_tree_node_id);
}

PrefetchURLLoaderInterceptor::PrefetchURLLoaderInterceptor(
    int frame_tree_node_id)
    : frame_tree_node_id_(frame_tree_node_id) {}

PrefetchURLLoaderInterceptor::~PrefetchURLLoaderInterceptor() = default;

void PrefetchURLLoaderInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tenative_resource_request,
    BrowserContext* browser_context,
    NavigationLoaderInterceptor::LoaderCallback callback,
    NavigationLoaderInterceptor::FallbackCallback fallback_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!loader_callback_);
  loader_callback_ = std::move(callback);
  url_ = tenative_resource_request.url;

  base::WeakPtr<PrefetchContainer> prefetch_container = GetPrefetch(url_);
  if (!prefetch_container ||
      !prefetch_container->HasValidPrefetchedResponse(
          PrefetchCacheableDuration()) ||
      prefetch_container->HaveDefaultContextCookiesChanged()) {
    DoNotInterceptNavigation();
    return;
  }

  // TODO(crbug.com/1299059): Check if we need to probe the origin

  EnsureCookiesCopiedAndInterceptPrefetchedNavigation(tenative_resource_request,
                                                      prefetch_container);
}

base::WeakPtr<PrefetchContainer> PrefetchURLLoaderInterceptor::GetPrefetch(
    const GURL& url) const {
  PrefetchService* prefetch_service =
      PrefetchServiceFromFrameTreeNodeId(frame_tree_node_id_);
  if (!prefetch_service)
    return nullptr;

  return prefetch_service->GetPrefetchToServe(url);
}

void PrefetchURLLoaderInterceptor::
    EnsureCookiesCopiedAndInterceptPrefetchedNavigation(
        const network::ResourceRequest& tenative_resource_request,
        base::WeakPtr<PrefetchContainer> prefetch_container) {
  if (prefetch_container &&
      prefetch_container->IsIsolatedCookieCopyInProgress()) {
    cookie_copy_start_time_ = base::TimeTicks::Now();
    prefetch_container->SetOnCookieCopyCompleteCallback(base::BindOnce(
        &PrefetchURLLoaderInterceptor::InterceptPrefetchedNavigation,
        weak_factory_.GetWeakPtr(), tenative_resource_request,
        prefetch_container));
    return;
  }

  RecordCookieWaitTime(base::TimeDelta());

  InterceptPrefetchedNavigation(tenative_resource_request, prefetch_container);
}

void PrefetchURLLoaderInterceptor::InterceptPrefetchedNavigation(
    const network::ResourceRequest& tenative_resource_request,
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  if (cookie_copy_start_time_) {
    base::TimeDelta wait_time =
        base::TimeTicks::Now() - cookie_copy_start_time_.value();
    DCHECK_GT(wait_time, base::TimeDelta());
    RecordCookieWaitTime(wait_time);
  }

  if (!prefetch_container) {
    DoNotInterceptNavigation();
    return;
  }

  prefetch_container->SetPrefetchStatus(PrefetchStatus::kPrefetchUsedNoProbe);

  std::unique_ptr<PrefetchFromStringURLLoader> url_loader =
      std::make_unique<PrefetchFromStringURLLoader>(
          prefetch_container->ReleasePrefetchedResponse(),
          tenative_resource_request);

  std::move(loader_callback_)
      .Run(base::MakeRefCounted<SingleRequestURLLoaderFactory>(
          url_loader->ServingResponseHandler()));

  // url_loader manages its own lifetime once bound to the mojo pipes.
  url_loader.release();
}

void PrefetchURLLoaderInterceptor::DoNotInterceptNavigation() {
  std::move(loader_callback_).Run({});
}

}  // namespace content
