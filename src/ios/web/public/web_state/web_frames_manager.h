// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_WEB_FRAMES_MANAGER_H_
#define IOS_WEB_PUBLIC_WEB_STATE_WEB_FRAMES_MANAGER_H_

#include <set>
#include <string>

#include "base/macros.h"
#import "ios/web/public/web_state/web_state_user_data.h"

namespace web {

class WebFrame;

// Stores and provides access to all WebFrame objects associated with a
// particular WebState.
// NOTE: Code that store references to WebFrames must observe WebState, in
// particular |WebFrameDidBecomeUnavailable| event, and clear all reference
// when the frame becomes unavailable ad the pointer to the WebFrame becomes
// invalid.
// For example, a navigation will invalidate the WebFrame object for that frame.
class WebFramesManager : public web::WebStateUserData<WebFramesManager> {
 public:
  // Returns a list of all the web frames associated with WebState.
  // NOTE: Due to the asynchronous nature of renderer, this list may be
  // outdated.
  virtual std::set<WebFrame*> GetAllWebFrames() = 0;
  // Returns the web frame for the main frame associated with WebState or null
  // if unknown.
  // NOTE: Due to the asynchronous nature of JavaScript to native messsaging,
  // this object may be outdated.
  virtual WebFrame* GetMainWebFrame() = 0;
  // Returns the web frame with |frame_id|, if one exists.
  // NOTE: Due to the asynchronous nature of JavaScript to native messsaging,
  // this object may be outdated and the WebFrame returned by this method may
  // not back a real frame in the web page.
  virtual WebFrame* GetFrameWithId(const std::string& frame_id) = 0;

  ~WebFramesManager() override {}

 protected:
  WebFramesManager() {}

  DISALLOW_COPY_AND_ASSIGN(WebFramesManager);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_WEB_FRAMES_MANAGER_H_
