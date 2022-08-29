// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speculation_rules/prefetch/prefetch_container.h"

#include <memory>

#include "base/callback.h"
#include "base/time/time.h"
#include "content/browser/speculation_rules/prefetch/prefetch_cookie_listener.h"
#include "content/browser/speculation_rules/prefetch/prefetch_document_manager.h"
#include "content/browser/speculation_rules/prefetch/prefetch_network_context.h"
#include "content/browser/speculation_rules/prefetch/prefetch_service.h"
#include "content/browser/speculation_rules/prefetch/prefetch_status.h"
#include "content/browser/speculation_rules/prefetch/prefetch_type.h"
#include "content/browser/speculation_rules/prefetch/prefetched_mainframe_response_container.h"
#include "content/public/browser/global_routing_id.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

namespace content {

PrefetchContainer::PrefetchContainer(
    const GlobalRenderFrameHostId& referring_render_frame_host_id,
    const GURL& url,
    const PrefetchType& prefetch_type,
    base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager)
    : referring_render_frame_host_id_(referring_render_frame_host_id),
      url_(url),
      prefetch_type_(prefetch_type),
      prefetch_document_manager_(prefetch_document_manager) {}

PrefetchContainer::~PrefetchContainer() = default;

PrefetchStatus PrefetchContainer::GetPrefetchStatus() const {
  DCHECK(prefetch_status_);
  return prefetch_status_.value();
}

PrefetchNetworkContext* PrefetchContainer::GetOrCreateNetworkContext(
    PrefetchService* prefetch_service) {
  if (!network_context_) {
    network_context_ = std::make_unique<PrefetchNetworkContext>(
        prefetch_service, prefetch_type_);
  }
  return network_context_.get();
}

PrefetchDocumentManager* PrefetchContainer::GetPrefetchDocumentManager() const {
  return prefetch_document_manager_.get();
}

void PrefetchContainer::RegisterCookieListener(
    network::mojom::CookieManager* cookie_manager) {
  cookie_listener_ =
      PrefetchCookieListener::MakeAndRegister(url_, cookie_manager);
}

void PrefetchContainer::StopCookieListener() {
  if (cookie_listener_)
    cookie_listener_->StopListening();
}

bool PrefetchContainer::HaveDefaultContextCookiesChanged() const {
  if (cookie_listener_)
    return cookie_listener_->HaveCookiesChanged();
  return false;
}

bool PrefetchContainer::IsIsolatedCookieCopyInProgress() const {
  switch (cookie_copy_status_) {
    case CookieCopyStatus::kNotStarted:
    case CookieCopyStatus::kCompleted:
      return false;
    case CookieCopyStatus::kInProgress:
      return true;
  }
}

void PrefetchContainer::OnIsolatedCookieCopyStart() {
  DCHECK(!IsIsolatedCookieCopyInProgress());

  // We don't want the cookie listener for this URL to get the changes from the
  // copy.
  StopCookieListener();

  cookie_copy_status_ = CookieCopyStatus::kInProgress;
}

void PrefetchContainer::OnIsolatedCookieCopyComplete() {
  DCHECK(IsIsolatedCookieCopyInProgress());

  cookie_copy_status_ = CookieCopyStatus::kCompleted;

  if (on_cookie_copy_complete_callback_)
    std::move(on_cookie_copy_complete_callback_).Run();
}

void PrefetchContainer::SetOnCookieCopyCompleteCallback(
    base::OnceClosure callback) {
  DCHECK(IsIsolatedCookieCopyInProgress());
  on_cookie_copy_complete_callback_ = std::move(callback);
}

void PrefetchContainer::TakeURLLoader(
    std::unique_ptr<network::SimpleURLLoader> loader) {
  DCHECK(!loader_);
  loader_ = std::move(loader);
}

void PrefetchContainer::ResetURLLoader() {
  DCHECK(loader_);
  loader_.reset();
}

bool PrefetchContainer::HasValidPrefetchedResponse(
    base::TimeDelta cacheable_duration) const {
  return prefetched_response_ != nullptr &&
         prefetch_received_time_.has_value() &&
         base::TimeTicks::Now() <
             prefetch_received_time_.value() + cacheable_duration;
}

void PrefetchContainer::TakePrefetchedResponse(
    std::unique_ptr<PrefetchedMainframeResponseContainer> prefetched_response) {
  DCHECK(!prefetched_response_);
  DCHECK(!is_decoy_);
  prefetch_received_time_ = base::TimeTicks::Now();
  prefetched_response_ = std::move(prefetched_response);
}

std::unique_ptr<PrefetchedMainframeResponseContainer>
PrefetchContainer::ReleasePrefetchedResponse() {
  prefetch_received_time_.reset();
  return std::move(prefetched_response_);
}

}  // namespace content
