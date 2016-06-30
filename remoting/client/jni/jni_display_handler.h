// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_JNI_DISPLAY_HANDLER_H_
#define REMOTING_CLIENT_JNI_JNI_DISPLAY_HANDLER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/client/jni/display_updater_factory.h"

namespace remoting {

namespace protocol {
class CursorShapeInfo;
class VideoRenderer;
}  // namespace protocol

class ChromotingJniRuntime;

// Handles display operations. Must be called and deleted on the display thread
// unless otherwise noted.
class JniDisplayHandler : public DisplayUpdaterFactory {
 public:
  explicit JniDisplayHandler(ChromotingJniRuntime* runtime);

  // Must be deleted on the display thread.
  ~JniDisplayHandler() override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaDisplay();

  void UpdateCursorShape(const protocol::CursorShapeInfo& cursor_shape);

  // DisplayUpdaterFactory overrides (functions can be called on any thread).
  std::unique_ptr<protocol::CursorShapeStub> CreateCursorShapeStub() override;
  std::unique_ptr<protocol::VideoRenderer> CreateVideoRenderer() override;

  // Creates a new Bitmap object to store a video frame. Can be called on any
  // thread.
  static base::android::ScopedJavaLocalRef<jobject> NewBitmap(int width,
                                                              int height);

  // Updates video frame bitmap. |bitmap| must be an instance of
  // android.graphics.Bitmap.
  void UpdateFrameBitmap(base::android::ScopedJavaGlobalRef<jobject> bitmap);

  // Draws the latest image buffer onto the canvas.
  void RedrawCanvas();

  static bool RegisterJni(JNIEnv* env);

  // Schedule redraw. Can be called on any thread.
  void ScheduleRedraw(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& caller);

 private:
  ChromotingJniRuntime* runtime_;

  base::android::ScopedJavaGlobalRef<jobject> java_display_;
  base::WeakPtrFactory<JniDisplayHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(JniDisplayHandler);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_JNI_JNI_DISPLAY_HANDLER_H_
