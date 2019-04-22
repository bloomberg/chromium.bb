// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/overlay_request_queue_impl.h"

#include <utility>

#include "ios/chrome/browser/overlays/overlay_request.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WEB_STATE_USER_DATA_KEY_IMPL(OverlayRequestQueue)

// static
void OverlayRequestQueueImpl::CreateForWebState(web::WebState* web_state) {
  DCHECK(web_state);
  if (!FromWebState(web_state))
    web_state->SetUserData(UserDataKey(),
                           base::WrapUnique(new OverlayRequestQueueImpl()));
}

// static
OverlayRequestQueueImpl* OverlayRequestQueueImpl::FromWebState(
    web::WebState* web_state) {
  return static_cast<OverlayRequestQueueImpl*>(
      OverlayRequestQueue::FromWebState(web_state));
}

OverlayRequestQueueImpl::OverlayRequestQueueImpl() = default;
OverlayRequestQueueImpl::~OverlayRequestQueueImpl() = default;

void OverlayRequestQueueImpl::AddRequest(
    std::unique_ptr<OverlayRequest> request) {
  requests_.push_back(std::move(request));
  for (auto& observer : observers_) {
    observer.OnRequestAdded(this, requests_.back().get());
  }
}

void OverlayRequestQueueImpl::PopRequest() {
  DCHECK(!requests_.empty());
  std::unique_ptr<OverlayRequest> popped_request = std::move(requests_.front());
  requests_.pop_front();
  for (auto& observer : observers_) {
    observer.OnRequestRemoved(this, popped_request.get());
  }
}

OverlayRequest* OverlayRequestQueueImpl::front_request() const {
  return requests_.empty() ? nullptr : requests_.front().get();
}
