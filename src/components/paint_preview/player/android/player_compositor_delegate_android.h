// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_PLAYER_ANDROID_PLAYER_COMPOSITOR_DELEGATE_ANDROID_H_
#define COMPONENTS_PAINT_PREVIEW_PLAYER_ANDROID_PLAYER_COMPOSITOR_DELEGATE_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/paint_preview/player/player_compositor_delegate.h"

class SkBitmap;

namespace paint_preview {
class PaintPreviewBaseService;

class PlayerCompositorDelegateAndroid : public PlayerCompositorDelegate {
 public:
  PlayerCompositorDelegateAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobject,
      PaintPreviewBaseService* paint_preview_service,
      const base::android::JavaParamRef<jstring>& j_url_spec,
      const base::android::JavaParamRef<jstring>& j_directory_key);

  void OnCompositorReady(
      mojom::PaintPreviewCompositor::Status status,
      mojom::PaintPreviewBeginCompositeResponsePtr composite_response) override;

  // Called from Java when there is a request for a new bitmap. When the bitmap
  // is ready, it will be passed to j_bitmap_callback. In case of any failure,
  // j_error_callback will be called.
  void RequestBitmap(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_frame_guid,
      const base::android::JavaParamRef<jobject>& j_bitmap_callback,
      const base::android::JavaParamRef<jobject>& j_error_callback,
      jfloat j_scale_factor,
      jint j_clip_x,
      jint j_clip_y,
      jint j_clip_width,
      jint j_clip_height);

  // Called from Java on touch event on a frame.
  void OnClick(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& j_frame_guid,
               jint j_x,
               jint j_y);

  void Destroy(JNIEnv* env);

  static void CompositeResponseFramesToVectors(
      const base::flat_map<base::UnguessableToken, mojom::FrameDataPtr>& frames,
      std::vector<base::UnguessableToken>* all_guids,
      std::vector<int>* scroll_extents,
      std::vector<int>* subframe_count,
      std::vector<base::UnguessableToken>* subframe_ids,
      std::vector<int>* subframe_rects);

 private:
  ~PlayerCompositorDelegateAndroid() override;

  void OnBitmapCallback(
      const base::android::ScopedJavaGlobalRef<jobject>& j_bitmap_callback,
      const base::android::ScopedJavaGlobalRef<jobject>& j_error_callback,
      int request_id,
      mojom::PaintPreviewCompositor::Status status,
      const SkBitmap& sk_bitmap);

  // Points to corresponding the Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  int request_id_;

  base::WeakPtrFactory<PlayerCompositorDelegateAndroid> weak_factory_{this};

  PlayerCompositorDelegateAndroid(const PlayerCompositorDelegateAndroid&) =
      delete;
  PlayerCompositorDelegateAndroid& operator=(
      const PlayerCompositorDelegateAndroid&) = delete;
};  // namespace paint_preview

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_PLAYER_ANDROID_PLAYER_COMPOSITOR_DELEGATE_ANDROID_H_
