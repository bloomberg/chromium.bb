// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_WEB_FRAMES_MANAGER_IMPL_H_
#define IOS_WEB_JS_MESSAGING_WEB_FRAMES_MANAGER_IMPL_H_

#import "ios/web/public/js_messaging/web_frames_manager.h"

#include <map>
#include "base/memory/weak_ptr.h"

namespace web {
class WebFrame;

class WebFramesManagerImpl : public WebFramesManager {
 public:
  explicit WebFramesManagerImpl();

  WebFramesManagerImpl(const WebFramesManagerImpl&) = delete;
  WebFramesManagerImpl& operator=(const WebFramesManagerImpl&) = delete;

  ~WebFramesManagerImpl() override;

  // Adds |frame| to the list of web frames. A frame with the same frame ID must
  // not already be registered). If |frame| is a main frame, the frame manager
  // must not already have a main frame.
  void AddFrame(std::unique_ptr<WebFrame> frame);
  // Removes the web frame with |frame_id|, if one exists, from the list of
  // associated web frames. If the frame manager does not contain a frame with
  // |frame_id|, operation is a no-op.
  void RemoveFrameWithId(const std::string& frame_id);

  // WebFramesManager overrides.
  std::set<WebFrame*> GetAllWebFrames() override;
  WebFrame* GetMainWebFrame() override;
  WebFrame* GetFrameWithId(const std::string& frame_id) override;

 private:
  // List of pointers to all web frames.
  std::map<std::string, std::unique_ptr<WebFrame>> web_frames_;

  // Reference to the current main web frame.
  WebFrame* main_web_frame_ = nullptr;

  base::WeakPtrFactory<WebFramesManagerImpl> weak_factory_;
};

}  // namespace web

#endif  // IOS_WEB_JS_MESSAGING_WEB_FRAMES_MANAGER_IMPL_H_
