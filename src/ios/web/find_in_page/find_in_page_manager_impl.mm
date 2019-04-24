// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/find_in_page/find_in_page_manager_impl.h"

#import "base/strings/sys_string_conversions.h"
#include "base/values.h"
#import "ios/web/find_in_page/find_in_page_constants.h"
#import "ios/web/public/find_in_page/find_in_page_manager_delegate.h"
#import "ios/web/public/web_state/web_frame.h"
#include "ios/web/public/web_state/web_frame_util.h"
#import "ios/web/public/web_state/web_frames_manager.h"
#import "ios/web/web_state/web_state_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// Timeout for the find within JavaScript in milliseconds.
const double kFindInPageFindTimeout = 100.0;

// Value returned when |kFindInPageSearch| call times out.
const int kFindInPagePending = -1;

// The timeout for JavaScript function calls in milliseconds. Important that
// this is longer than |kFindInPageFindTimeout| to allow for incomplete find to
// restart again. If this timeout hits, then something went wrong with the find
// and find in page should not continue.
const double kJavaScriptFunctionCallTimeout = 200.0;

// static
FindInPageManagerImpl::FindInPageManagerImpl(WebState* web_state)
    : web_state_(web_state), weak_factory_(this) {
  web_state_->AddObserver(this);
}

void FindInPageManagerImpl::CreateForWebState(WebState* web_state) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(UserDataKey(),
                           std::make_unique<FindInPageManagerImpl>(web_state));
  }
}

FindInPageManagerImpl::~FindInPageManagerImpl() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

FindInPageManagerDelegate* FindInPageManagerImpl::GetDelegate() {
  return delegate_;
}
void FindInPageManagerImpl::SetDelegate(FindInPageManagerDelegate* delegate) {
  delegate_ = delegate;
}

FindInPageManagerImpl::FindRequest::FindRequest() {}

FindInPageManagerImpl::FindRequest::~FindRequest() {}

int FindInPageManagerImpl::FindRequest::GetTotalMatchCount() const {
  int matches = 0;
  for (auto pair : frame_match_count) {
    matches += pair.second;
  }
  return matches;
}

void FindInPageManagerImpl::WebFrameDidBecomeAvailable(WebState* web_state,
                                                       WebFrame* web_frame) {
  const std::string frame_id = web_frame->GetFrameId();
  last_find_request_.frame_match_count[frame_id] = 0;
  if (web_frame->IsMainFrame()) {
    // Main frame matches should show up first.
    last_find_request_.frame_order.push_front(frame_id);
  } else {
    // The order of iframes is not important.
    last_find_request_.frame_order.push_back(frame_id);
  }
}

void FindInPageManagerImpl::WebFrameWillBecomeUnavailable(WebState* web_state,
                                                          WebFrame* web_frame) {
  last_find_request_.frame_order.remove(web_frame->GetFrameId());
  last_find_request_.frame_match_count.erase(web_frame->GetFrameId());
}

void FindInPageManagerImpl::WebStateDestroyed(WebState* web_state) {
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

void FindInPageManagerImpl::Find(NSString* query, FindInPageOptions options) {
  switch (options) {
    case FindInPageOptions::FindInPageSearch: {
      DCHECK(query);

      std::set<WebFrame*> all_frames = GetAllWebFrames(web_state_);
      last_find_request_.pending_frame_call_count = all_frames.size();

      std::vector<base::Value> params;
      params.push_back(base::Value(base::SysNSStringToUTF8(query)));
      params.push_back(base::Value(kFindInPageFindTimeout));
      int unique_id = ++last_find_request_.unique_id;
      for (WebFrame* frame : all_frames) {
        frame->CallJavaScriptFunction(
            kFindInPageSearch, params,
            base::BindOnce(&FindInPageManagerImpl::ProcessFindInPageResult,
                           weak_factory_.GetWeakPtr(),
                           base::SysNSStringToUTF8(query), frame->GetFrameId(),
                           unique_id),
            base::TimeDelta::FromSeconds(kJavaScriptFunctionCallTimeout));
      }
      break;
    }
    case FindInPageOptions::FindInPageNext:
    case FindInPageOptions::FindInPagePrevious:
      break;
  }
}

void FindInPageManagerImpl::StopFinding() {}

void FindInPageManagerImpl::ProcessFindInPageResult(const std::string& query,
                                                    const std::string& frame_id,
                                                    const int unique_id,
                                                    const base::Value* result) {
  if (unique_id != last_find_request_.unique_id) {
    // New find was started.
    return;
  }
  if (!web_state_) {
    // WebState was destroyed before find finished.
    return;
  }

  last_find_request_.pending_frame_call_count--;
  WebFrame* frame = GetWebFrameWithId(web_state_, frame_id);
  if (!result || !frame) {
    // The frame no longer exists or the function call timed out. In both cases,
    // result will be null.
    // Zero out count to ensure every frame is updated for every find.
    last_find_request_.frame_match_count[frame_id] = 0;
  } else {
    int match_count = 0;
    if (result->is_double()) {
      // Valid match number returned. If not, match count will be 0 in order to
      // zero-out count from previous find.
      match_count = static_cast<int>(result->GetDouble());
    }
    // If response is equal to kFindInPagePending, find did not finish in the
    // JavaScript. Call pumpSearch to continue find.
    if (match_count == kFindInPagePending) {
      std::vector<base::Value> params;
      params.push_back(base::Value(kFindInPageFindTimeout));
      frame->CallJavaScriptFunction(
          kFindInPagePump, params,
          base::BindOnce(&FindInPageManagerImpl::ProcessFindInPageResult,
                         weak_factory_.GetWeakPtr(), query, frame_id,
                         unique_id),
          base::TimeDelta::FromSeconds(kJavaScriptFunctionCallTimeout));
      return;
    }

    last_find_request_.frame_match_count[frame_id] = match_count;
  }
  if (last_find_request_.pending_frame_call_count == 0) {
    int total_match_count = last_find_request_.GetTotalMatchCount();
    delegate_->DidCountMatches(web_state_, total_match_count, query);
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(FindInPageManager)

}  // namespace web
