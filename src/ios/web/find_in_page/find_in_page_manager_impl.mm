// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/find_in_page/find_in_page_manager_impl.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#include "base/values.h"
#import "ios/web/find_in_page/find_in_page_constants.h"
#import "ios/web/find_in_page/find_in_page_java_script_feature.h"
#import "ios/web/public/find_in_page/find_in_page_manager_delegate.h"
#import "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/web_state/web_state_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

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

void FindInPageManagerImpl::WebFrameDidBecomeAvailable(WebState* web_state,
                                                       WebFrame* web_frame) {
  const std::string frame_id = web_frame->GetFrameId();
  last_find_request_.AddFrame(web_frame);
}

void FindInPageManagerImpl::WebFrameWillBecomeUnavailable(WebState* web_state,
                                                          WebFrame* web_frame) {
  int match_count =
      last_find_request_.GetMatchCountForFrame(web_frame->GetFrameId());
  last_find_request_.RemoveFrame(web_frame->GetFrameId());

  // Only notify the delegate if the match count has changed.
  if (delegate_ && last_find_request_.GetRequestQuery() && match_count > 0) {
    delegate_->DidHighlightMatches(web_state_,
                                   last_find_request_.GetTotalMatchCount(),
                                   last_find_request_.GetRequestQuery());
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
  base::RecordAction(base::UserMetricsAction(kFindActionName));
  std::set<WebFrame*> all_frames =
      web_state_->GetWebFramesManager()->GetAllWebFrames();
  last_find_request_.Reset(query, all_frames.size());
  if (all_frames.size() == 0) {
    // No frames to search in.
    // Call asyncronously to match behavior if find was successful in frames.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&FindInPageManagerImpl::LastFindRequestCompleted,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  for (WebFrame* frame : all_frames) {
    bool result = FindInPageJavaScriptFeature::GetInstance()->Search(
        frame, base::SysNSStringToUTF8(query),
        base::BindOnce(&FindInPageManagerImpl::ProcessFindInPageResult,
                       weak_factory_.GetWeakPtr(), frame->GetFrameId(),
                       last_find_request_.GetRequestId()));

    if (!result) {
      // Calling JavaScript function failed or the frame does not support
      // messaging.
      last_find_request_.DidReceiveFindResponseFromOneFrame();
      if (last_find_request_.AreAllFindResponsesReturned()) {
        // Call asyncronously to match behavior if find was done in frames.
        GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE,
            base::BindOnce(&FindInPageManagerImpl::LastFindRequestCompleted,
                           weak_factory_.GetWeakPtr()));
      }
    }
  }
}

void FindInPageManagerImpl::StopFinding() {
  last_find_request_.Reset(/*new_query=*/nil,
                           /*new_pending_frame_call_count=*/0);

  for (WebFrame* frame : web_state_->GetWebFramesManager()->GetAllWebFrames()) {
    FindInPageJavaScriptFeature::GetInstance()->Stop(frame);
  }
  if (delegate_) {
    delegate_->DidHighlightMatches(web_state_,
                                   last_find_request_.GetTotalMatchCount(),
                                   last_find_request_.GetRequestQuery());
  }
}

bool FindInPageManagerImpl::CanSearchContent() {
  return web_state_->ContentIsHTML();
}

void FindInPageManagerImpl::ProcessFindInPageResult(
    const std::string& frame_id,
    const int unique_id,
    absl::optional<int> result_matches) {
  if (unique_id != last_find_request_.GetRequestId()) {
    // New find was started or current find was stopped.
    return;
  }
  if (!web_state_) {
    // WebState was destroyed before find finished.
    return;
  }

  WebFrame* frame = GetWebFrameWithId(web_state_, frame_id);
  if (!result_matches || !frame) {
    // The frame no longer exists or the function call timed out. In both cases,
    // result will be null.
    // Zero out count to ensure every frame is updated for every find.
    last_find_request_.SetMatchCountForFrame(0, frame_id);
  } else {
    // If response is equal to kFindInPagePending, find did not finish in the
    // JavaScript. Call pumpSearch to continue find.
    if (result_matches.value() == find_in_page::kFindInPagePending) {
      FindInPageJavaScriptFeature::GetInstance()->Pump(
          frame,
          base::BindOnce(&FindInPageManagerImpl::ProcessFindInPageResult,
                         weak_factory_.GetWeakPtr(), frame_id, unique_id));
      return;
    }

    last_find_request_.SetMatchCountForFrame(result_matches.value(), frame_id);
  }
  last_find_request_.DidReceiveFindResponseFromOneFrame();
  if (last_find_request_.AreAllFindResponsesReturned()) {
    LastFindRequestCompleted();
  }
}

void FindInPageManagerImpl::LastFindRequestCompleted() {
  if (delegate_) {
    delegate_->DidHighlightMatches(web_state_,
                                   last_find_request_.GetTotalMatchCount(),
                                   last_find_request_.GetRequestQuery());
  }
  int total_matches = last_find_request_.GetTotalMatchCount();
  if (total_matches == 0) {
    return;
  }

  if (last_find_request_.GoToFirstMatch()) {
    SelectCurrentMatch();
  }
}

void FindInPageManagerImpl::SelectDidFinish(const base::Value* result) {
  std::string match_context_string;
  if (result && result->is_dict()) {
    // Get updated match count.
    const base::Value* matches = result->FindKey(kSelectAndScrollResultMatches);
    if (matches && matches->is_double()) {
      int match_count = static_cast<int>(matches->GetDouble());
      if (match_count != last_find_request_.GetMatchCountForSelectedFrame()) {
        last_find_request_.SetMatchCountForSelectedFrame(match_count);
        if (delegate_) {
          delegate_->DidHighlightMatches(
              web_state_, last_find_request_.GetTotalMatchCount(),
              last_find_request_.GetRequestQuery());
        }
      }
    }
    // Get updated currently selected index.
    const base::Value* index = result->FindKey(kSelectAndScrollResultIndex);
    if (index && index->is_double()) {
      int current_index = static_cast<int>(index->GetDouble());
      last_find_request_.SetCurrentSelectedMatchFrameIndex(current_index);
    }
    // Get context string.
    const base::Value* context_string =
        result->FindKey(kSelectAndScrollResultContextString);
    if (context_string && context_string->is_string()) {
      match_context_string =
          static_cast<std::string>(context_string->GetString());
    }
  }
  if (delegate_) {
    delegate_->DidSelectMatch(
        web_state_, last_find_request_.GetCurrentSelectedMatchPageIndex(),
        base::SysUTF8ToNSString(match_context_string));
  }
}

void FindInPageManagerImpl::SelectNextMatch() {
  base::RecordAction(base::UserMetricsAction(kFindNextActionName));
  if (last_find_request_.GoToNextMatch()) {
    SelectCurrentMatch();
  }
}

void FindInPageManagerImpl::SelectPreviousMatch() {
  base::RecordAction(base::UserMetricsAction(kFindPreviousActionName));
  if (last_find_request_.GoToPreviousMatch()) {
    SelectCurrentMatch();
  }
}

void FindInPageManagerImpl::SelectCurrentMatch() {
  web::WebFrame* frame =
      GetWebFrameWithId(web_state_, last_find_request_.GetSelectedFrameId());
  if (frame) {
    FindInPageJavaScriptFeature::GetInstance()->SelectMatch(
        frame, last_find_request_.GetCurrentSelectedMatchFrameIndex(),
        base::BindOnce(&FindInPageManagerImpl::SelectDidFinish,
                       weak_factory_.GetWeakPtr()));
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(FindInPageManager)

}  // namespace web
