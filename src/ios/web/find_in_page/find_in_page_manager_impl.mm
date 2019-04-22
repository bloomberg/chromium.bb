// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/find_in_page/find_in_page_manager_impl.h"

#import "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "base/values.h"
#import "ios/web/find_in_page/find_in_page_constants.h"
#import "ios/web/public/find_in_page/find_in_page_manager_delegate.h"
#import "ios/web/public/web_state/web_frame.h"
#include "ios/web/public/web_state/web_frame_util.h"
#import "ios/web/public/web_state/web_frames_manager.h"
#include "ios/web/public/web_task_traits.h"
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

void FindInPageManagerImpl::FindRequest::Reset(
    NSString* new_query,
    int new_pending_frame_call_count) {
  unique_id++;
  selected_frame_id = frame_order.end();
  selected_match_index_in_selected_frame = -1;
  query = [new_query copy];
  pending_frame_call_count = new_pending_frame_call_count;
  for (auto& pair : frame_match_count) {
    pair.second = 0;
  }
}

int FindInPageManagerImpl::FindRequest::GetTotalMatchCount() const {
  int matches = 0;
  for (auto pair : frame_match_count) {
    matches += pair.second;
  }
  return matches;
}

int FindInPageManagerImpl::FindRequest::GetCurrentSelectedMatchIndex() {
  if (selected_match_index_in_selected_frame == -1) {
    return -1;
  }
  // Count all matches in frames that come before frame with id
  // |selected_frame_id|.
  int total_match_index = selected_match_index_in_selected_frame;
  for (auto it = frame_order.begin(); it != selected_frame_id; ++it) {
    total_match_index += frame_match_count[*it];
  }
  return total_match_index;
}

bool FindInPageManagerImpl::FindRequest::GoToFirstMatch() {
  for (auto frame_id = frame_order.begin(); frame_id != frame_order.end();
       ++frame_id) {
    if (frame_match_count[*frame_id] > 0) {
      selected_frame_id = frame_id;
      selected_match_index_in_selected_frame = 0;
      return true;
    }
  }
  return false;
}

bool FindInPageManagerImpl::FindRequest::GoToNextMatch() {
  if (GetTotalMatchCount() == 0) {
    return false;
  }
  // No currently selected match, but there are matches. Move iterator to
  // beginning. This can happen if a frame containing the currently selected
  // match is removed from the page.
  if (selected_frame_id == frame_order.end()) {
    selected_frame_id = frame_order.begin();
  }

  bool next_match_is_in_selected_frame =
      selected_match_index_in_selected_frame + 1 <
      frame_match_count[*selected_frame_id];
  if (next_match_is_in_selected_frame) {
    selected_match_index_in_selected_frame++;
  } else {
    // Since the function returns early if there are no matches, an infinite
    // loop should not be a risk.
    do {
      if (selected_frame_id == --frame_order.end()) {
        selected_frame_id = frame_order.begin();
      } else {
        selected_frame_id++;
      }
    } while (frame_match_count[*selected_frame_id] == 0);
    // Should have found new frame.
    selected_match_index_in_selected_frame = 0;
  }
  return true;
}

bool FindInPageManagerImpl::FindRequest::GoToPreviousMatch() {
  if (GetTotalMatchCount() == 0) {
    return false;
  }
  // No currently selected match, but there are matches. Move iterator to
  // beginning. This can happen if a frame containing the currently selected
  // matchs is removed from the page.
  if (selected_frame_id == frame_order.end()) {
    selected_frame_id = frame_order.begin();
  }

  bool previous_match_is_in_selected_frame =
      selected_match_index_in_selected_frame - 1 >= 0;
  if (previous_match_is_in_selected_frame) {
    selected_match_index_in_selected_frame--;
  } else {
    // Since the function returns early if there are no matches, an infinite
    // loop should not be a risk.
    do {
      if (selected_frame_id == frame_order.begin()) {
        selected_frame_id = --frame_order.end();
      } else {
        selected_frame_id--;
      }
    } while (frame_match_count[*selected_frame_id] == 0);
    // Should have found new frame.
    selected_match_index_in_selected_frame =
        frame_match_count[*selected_frame_id] - 1;
  }
  return true;
}

void FindInPageManagerImpl::FindRequest::RemoveFrame(WebFrame* web_frame) {
  if (IsSelectedFrame(web_frame)) {
    // If currently selecting match in frame that will become unavailable,
    // there will no longer be a selected match. Reset to unselected match
    // state.
    selected_frame_id = frame_order.end();
    selected_match_index_in_selected_frame = -1;
  }
  frame_order.remove(web_frame->GetFrameId());
  frame_match_count.erase(web_frame->GetFrameId());
}

bool FindInPageManagerImpl::FindRequest::IsSelectedFrame(WebFrame* web_frame) {
  if (selected_frame_id == frame_order.end()) {
    return false;
  }
  return *selected_frame_id == web_frame->GetFrameId();
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
  last_find_request_.RemoveFrame(web_frame);

  if (delegate_ && last_find_request_.query) {
    delegate_->DidHighlightMatches(web_state_,
                                   last_find_request_.GetTotalMatchCount(),
                                   last_find_request_.query);
  }
}

void FindInPageManagerImpl::WebStateDestroyed(WebState* web_state) {
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

void FindInPageManagerImpl::Find(NSString* query, FindInPageOptions options) {
  DCHECK(CanSearchContent());

  switch (options) {
    case FindInPageOptions::FindInPageSearch:
      DCHECK(query);
      StartSearch(query);
      break;
    case FindInPageOptions::FindInPageNext:
      SelectNextMatch();
      break;
    case FindInPageOptions::FindInPagePrevious:
      SelectPreviousMatch();
      break;
  }
}

void FindInPageManagerImpl::StartSearch(NSString* query) {
  std::set<WebFrame*> all_frames = GetAllWebFrames(web_state_);
  last_find_request_.Reset(query, all_frames.size());
  if (all_frames.size() == 0) {
    // No frames to search in.
    // Call asyncronously to match behavior if find was successful in frames.
    base::PostTaskWithTraits(
        FROM_HERE, {WebThread::UI},
        base::BindOnce(&FindInPageManagerImpl::LastFindRequestCompleted,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  std::vector<base::Value> params;
  params.push_back(base::Value(base::SysNSStringToUTF8(query)));
  params.push_back(base::Value(kFindInPageFindTimeout));
  for (WebFrame* frame : all_frames) {
    bool result = frame->CallJavaScriptFunction(
        kFindInPageSearch, params,
        base::BindOnce(&FindInPageManagerImpl::ProcessFindInPageResult,
                       weak_factory_.GetWeakPtr(), frame->GetFrameId(),
                       last_find_request_.unique_id),
        base::TimeDelta::FromSeconds(kJavaScriptFunctionCallTimeout));
    if (!result) {
      // Calling JavaScript function failed or the frame does not support
      // messaging.
      if (--last_find_request_.pending_frame_call_count == 0) {
        // Call asyncronously to match behavior if find was done in frames.
        base::PostTaskWithTraits(
            FROM_HERE, {WebThread::UI},
            base::BindOnce(&FindInPageManagerImpl::LastFindRequestCompleted,
                           weak_factory_.GetWeakPtr()));
      }
    }
  }
}

void FindInPageManagerImpl::StopFinding() {
  last_find_request_.Reset(/*new_query=*/nil,
                           /*new_pending_frame_call_count=*/0);

  std::vector<base::Value> params;
  for (WebFrame* frame : GetAllWebFrames(web_state_)) {
    frame->CallJavaScriptFunction(kFindInPageStop, params);
  }
  if (delegate_) {
    delegate_->DidHighlightMatches(web_state_,
                                   last_find_request_.GetTotalMatchCount(),
                                   last_find_request_.query);
  }
}

bool FindInPageManagerImpl::CanSearchContent() {
  return web_state_->ContentIsHTML();
}

void FindInPageManagerImpl::ProcessFindInPageResult(const std::string& frame_id,
                                                    const int unique_id,
                                                    const base::Value* result) {
  if (unique_id != last_find_request_.unique_id) {
    // New find was started or current find was stopped.
    return;
  }
  if (!web_state_) {
    // WebState was destroyed before find finished.
    return;
  }

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
                         weak_factory_.GetWeakPtr(), frame_id, unique_id),
          base::TimeDelta::FromSeconds(kJavaScriptFunctionCallTimeout));
      return;
    }

    last_find_request_.frame_match_count[frame_id] = match_count;
  }
  if (--last_find_request_.pending_frame_call_count == 0) {
    LastFindRequestCompleted();
  }
}

void FindInPageManagerImpl::LastFindRequestCompleted() {
  if (delegate_) {
    delegate_->DidHighlightMatches(web_state_,
                                   last_find_request_.GetTotalMatchCount(),
                                   last_find_request_.query);
  }
  int total_matches = last_find_request_.GetTotalMatchCount();
  if (total_matches == 0) {
    return;
  }

  if (last_find_request_.GoToFirstMatch()) {
    SelectCurrentMatch();
  }
}

void FindInPageManagerImpl::NotifyDelegateDidSelectMatch(
    const base::Value* result) {
  if (delegate_) {
    delegate_->DidSelectMatch(
        web_state_, last_find_request_.GetCurrentSelectedMatchIndex());
  }
}

void FindInPageManagerImpl::SelectNextMatch() {
  if (last_find_request_.GoToNextMatch()) {
    SelectCurrentMatch();
  }
}

void FindInPageManagerImpl::SelectPreviousMatch() {
  if (last_find_request_.GoToPreviousMatch()) {
    SelectCurrentMatch();
  }
}

void FindInPageManagerImpl::SelectCurrentMatch() {
  web::WebFrame* frame =
      GetWebFrameWithId(web_state_, *last_find_request_.selected_frame_id);
  if (frame) {
    std::vector<base::Value> params;
    params.push_back(
        base::Value(last_find_request_.selected_match_index_in_selected_frame));
    frame->CallJavaScriptFunction(
        kFindInPageSelectAndScrollToMatch, params,
        base::BindOnce(&FindInPageManagerImpl::NotifyDelegateDidSelectMatch,
                       weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(kJavaScriptFunctionCallTimeout));
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(FindInPageManager)

}  // namespace web
