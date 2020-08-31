// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_HELPERS_PAGE_LIVE_STATE_DECORATOR_HELPER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_HELPERS_PAGE_LIVE_STATE_DECORATOR_HELPER_H_

#include "base/containers/flat_set.h"
#include "base/sequence_checker.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "components/performance_manager/public/performance_manager_main_thread_observer.h"

namespace performance_manager {

class PageLiveStateDecoratorHelper
    : public MediaStreamCaptureIndicator::Observer,
      public PerformanceManagerMainThreadObserver {
 public:
  PageLiveStateDecoratorHelper();
  ~PageLiveStateDecoratorHelper() override;
  PageLiveStateDecoratorHelper(const PageLiveStateDecoratorHelper& other) =
      delete;
  PageLiveStateDecoratorHelper& operator=(const PageLiveStateDecoratorHelper&) =
      delete;

  // MediaStreamCaptureIndicator::Observer implementation:
  void OnIsCapturingVideoChanged(content::WebContents* contents,
                                 bool is_capturing_video) override;
  void OnIsCapturingAudioChanged(content::WebContents* contents,
                                 bool is_capturing_audio) override;
  void OnIsBeingMirroredChanged(content::WebContents* contents,
                                bool is_being_mirrored) override;
  void OnIsCapturingDesktopChanged(content::WebContents* contents,
                                   bool is_capturing_desktop) override;

  // PerformanceManagerMainThreadObserver:
  void OnPageNodeCreatedForWebContents(
      content::WebContents* web_contents) override;

 private:
  class WebContentsObserver;

  // Linked list of WebContentsObservers created by this
  // PageLiveStateDecoratorHelper. Each WebContentsObservers removes itself from
  // the list and destroys itself when its associated WebContents is destroyed.
  // Additionally, all WebContentsObservers that are still in this list when the
  // destructor of PageLiveStateDecoratorHelper is invoked are destroyed.
  WebContentsObserver* first_web_contents_observer_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_HELPERS_PAGE_LIVE_STATE_DECORATOR_HELPER_H_
