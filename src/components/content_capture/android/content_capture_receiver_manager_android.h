// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CAPTURE_ANDROID_CONTENT_CAPTURE_RECEIVER_MANAGER_ANDROID_H_
#define COMPONENTS_CONTENT_CAPTURE_ANDROID_CONTENT_CAPTURE_RECEIVER_MANAGER_ANDROID_H_

#include <vector>

#include "base/android/scoped_java_ref.h"
#include "components/content_capture/browser/content_capture_receiver_manager.h"

namespace content_capture {

// The Android's implementation of ContentCaptureReceiverManager, it forwards
// the received message to Java.
class ContentCaptureReceiverManagerAndroid
    : public ContentCaptureReceiverManager {
 public:
  ~ContentCaptureReceiverManagerAndroid() override;
  static void Create(content::WebContents* web_contents,
                     const base::android::JavaRef<jobject>& jcaller);

  void DidCaptureContent(const ContentCaptureSession& parent_session,
                         const ContentCaptureData& data) override;
  void DidRemoveContent(const ContentCaptureSession& session,
                        const std::vector<int64_t>& data) override;
  void DidRemoveSession(const ContentCaptureSession& session) override;

 private:
  ContentCaptureReceiverManagerAndroid(
      content::WebContents* web_contents,
      const base::android::JavaRef<jobject>& jcaller);

  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

}  // namespace content_capture

#endif  // COMPONENTS_CONTENT_CAPTURE_ANDROID_CONTENT_CAPTURE_RECEIVER_MANAGER_ANDROID_H_
