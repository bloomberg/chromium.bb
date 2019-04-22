// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_state/web_frames_manager_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "ios/web/public/web_state/web_frame.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Matcher for finding a WebFrame object with the given |frame_id|.
struct FrameIdMatcher {
  explicit FrameIdMatcher(const std::string& frame_id) : frame_id_(frame_id) {}

  // Returns true if the receiver was initialized with the same frame id as
  // |web_frame|.
  bool operator()(web::WebFrame* web_frame) {
    return web_frame->GetFrameId() == frame_id_;
  }

 private:
  // The frame id to match against.
  const std::string frame_id_;
};
}  // namespace

namespace web {

// static
void WebFramesManagerImpl::CreateForWebState(WebState* web_state) {
  DCHECK(web_state);
  if (!FromWebState(web_state))
    web_state->SetUserData(
        UserDataKey(), base::WrapUnique(new WebFramesManagerImpl(web_state)));
}

// static
WebFramesManagerImpl* WebFramesManagerImpl::FromWebState(WebState* web_state) {
  return static_cast<WebFramesManagerImpl*>(
      WebFramesManager::FromWebState(web_state));
}

WebFramesManagerImpl::~WebFramesManagerImpl() {
  RemoveAllWebFrames();
}

WebFramesManagerImpl::WebFramesManagerImpl(web::WebState* web_state)
    : web_state_(web_state) {}

void WebFramesManagerImpl::AddFrame(std::unique_ptr<WebFrame> frame) {
  DCHECK(frame);
  DCHECK(!frame->GetFrameId().empty());
  if (frame->IsMainFrame()) {
    DCHECK(!main_web_frame_);
    main_web_frame_ = frame.get();
  }
  DCHECK(web_frames_.count(frame->GetFrameId()) == 0);
  std::string frame_id = frame->GetFrameId();
  web_frames_[frame_id] = std::move(frame);
}

void WebFramesManagerImpl::RemoveFrameWithId(const std::string& frame_id) {
  DCHECK(!frame_id.empty());
  // If the removed frame is a main frame, it should be the current one.
  DCHECK(web_frames_.count(frame_id) == 0 ||
         !web_frames_[frame_id]->IsMainFrame() ||
         main_web_frame_ == web_frames_[frame_id].get());
  if (web_frames_.count(frame_id) == 0) {
    return;
  }
  if (main_web_frame_ && main_web_frame_->GetFrameId() == frame_id) {
    main_web_frame_ = nullptr;
  }
  // The web::WebFrame destructor can call some callbacks that will try to
  // access the frame via GetFrameWithId. This can lead to a reentrancy issue
  // on |web_frames_|.
  // To avoid this issue, keep the frame alive during the map operation and
  // destroy it after.
  auto keep_frame_alive = std::move(web_frames_[frame_id]);
  web_frames_.erase(frame_id);
}

void WebFramesManagerImpl::RemoveAllWebFrames() {
  while (web_frames_.size()) {
    RemoveFrameWithId(web_frames_.begin()->first);
  }
}

WebFrame* WebFramesManagerImpl::GetFrameWithId(const std::string& frame_id) {
  DCHECK(!frame_id.empty());
  auto web_frames_it = web_frames_.find(frame_id);
  return web_frames_it == web_frames_.end() ? nullptr
                                            : web_frames_it->second.get();
}

std::set<WebFrame*> WebFramesManagerImpl::GetAllWebFrames() {
  std::set<WebFrame*> frames;
  for (const auto& it : web_frames_) {
    frames.insert(it.second.get());
  }
  return frames;
}

WebFrame* WebFramesManagerImpl::GetMainWebFrame() {
  return main_web_frame_;
}

void WebFramesManagerImpl::RegisterExistingFrames() {
  web_state_->ExecuteJavaScript(
      base::UTF8ToUTF16("__gCrWeb.message.getExistingFrames();"));
}

WEB_STATE_USER_DATA_KEY_IMPL(WebFramesManager)

}  // namespace
