// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/tab_content_manager.h"

#include <android/bitmap.h>
#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "cc/layers/layer.h"
#include "chrome/android/chrome_jni_headers/TabContentManager_jni.h"
#include "chrome/browser/android/compositor/layer/thumbnail_layer.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/thumbnail/cc/thumbnail.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/image_operations.h"
#include "ui/android/resources/ui_resource_provider.h"
#include "ui/android/view_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace {

using TabReadbackCallback = base::OnceCallback<void(float, const SkBitmap&)>;

}  // namespace

namespace android {

class TabContentManager::TabReadbackRequest {
 public:
  TabReadbackRequest(content::RenderWidgetHostView* rwhv,
                     float thumbnail_scale,
                     bool crop_to_match_aspect_ratio,
                     TabReadbackCallback end_callback)
      : thumbnail_scale_(thumbnail_scale),
        end_callback_(std::move(end_callback)),
        drop_after_readback_(false) {
    DCHECK(rwhv);
    auto result_callback =
        base::BindOnce(&TabReadbackRequest::OnFinishGetTabThumbnailBitmap,
                       weak_factory_.GetWeakPtr());

    gfx::Size view_size_in_pixels =
        rwhv->GetNativeView()->GetPhysicalBackingSize();
    if (view_size_in_pixels.IsEmpty()) {
      std::move(result_callback).Run(SkBitmap());
      return;
    }
    if (crop_to_match_aspect_ratio) {
      double aspect_ratio = base::GetFieldTrialParamByFeatureAsDouble(
          chrome::android::kTabGridLayoutAndroid, "thumbnail_aspect_ratio",
          1.0);
      aspect_ratio = ThumbnailCache::clampAspectRatio(aspect_ratio, 0.5, 2.0);
      int height = std::min(view_size_in_pixels.height(),
                            (int)(view_size_in_pixels.width() / aspect_ratio));
      view_size_in_pixels.set_height(height);
    }
    gfx::Rect source_rect = gfx::Rect(view_size_in_pixels);
    gfx::Size thumbnail_size(
        gfx::ScaleToCeiledSize(view_size_in_pixels, thumbnail_scale_));
    rwhv->CopyFromSurface(source_rect, thumbnail_size,
                          std::move(result_callback));
  }

  virtual ~TabReadbackRequest() {}

  void OnFinishGetTabThumbnailBitmap(const SkBitmap& bitmap) {
    if (bitmap.drawsNothing() || drop_after_readback_) {
      std::move(end_callback_).Run(0.f, SkBitmap());
      return;
    }

    SkBitmap result_bitmap = bitmap;
    result_bitmap.setImmutable();
    std::move(end_callback_).Run(thumbnail_scale_, bitmap);
  }

  void SetToDropAfterReadback() { drop_after_readback_ = true; }

 private:
  const float thumbnail_scale_;
  TabReadbackCallback end_callback_;
  bool drop_after_readback_;

  base::WeakPtrFactory<TabReadbackRequest> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TabReadbackRequest);
};

// static
TabContentManager* TabContentManager::FromJavaObject(
    const JavaRef<jobject>& jobj) {
  if (jobj.is_null())
    return nullptr;
  return reinterpret_cast<TabContentManager*>(
      Java_TabContentManager_getNativePtr(base::android::AttachCurrentThread(),
                                          jobj));
}

TabContentManager::TabContentManager(JNIEnv* env,
                                     jobject obj,
                                     jint default_cache_size,
                                     jint approximation_cache_size,
                                     jint compression_queue_max_size,
                                     jint write_queue_max_size,
                                     jboolean use_approximation_thumbnail,
                                     jboolean save_jpeg_thumbnails)
    : weak_java_tab_content_manager_(env, obj) {
  double jpeg_aspect_ratio = base::GetFieldTrialParamByFeatureAsDouble(
      chrome::android::kTabGridLayoutAndroid, "thumbnail_aspect_ratio", 1.0);
  thumbnail_cache_ = std::make_unique<ThumbnailCache>(
      static_cast<size_t>(default_cache_size),
      static_cast<size_t>(approximation_cache_size),
      static_cast<size_t>(compression_queue_max_size),
      static_cast<size_t>(write_queue_max_size), use_approximation_thumbnail,
      save_jpeg_thumbnails, jpeg_aspect_ratio);
  thumbnail_cache_->AddThumbnailCacheObserver(this);
}

TabContentManager::~TabContentManager() {
}

void TabContentManager::Destroy(JNIEnv* env) {
  thumbnail_cache_->RemoveThumbnailCacheObserver(this);
  delete this;
}

void TabContentManager::SetUIResourceProvider(
    ui::UIResourceProvider* ui_resource_provider) {
  thumbnail_cache_->SetUIResourceProvider(ui_resource_provider);
}

scoped_refptr<cc::Layer> TabContentManager::GetLiveLayer(int tab_id) {
  return live_layer_list_[tab_id];
}

scoped_refptr<ThumbnailLayer> TabContentManager::GetStaticLayer(int tab_id) {
  return static_layer_cache_[tab_id];
}

scoped_refptr<ThumbnailLayer> TabContentManager::GetOrCreateStaticLayer(
    int tab_id,
    bool force_disk_read) {
  Thumbnail* thumbnail = thumbnail_cache_->Get(tab_id, force_disk_read, true);
  scoped_refptr<ThumbnailLayer> static_layer = static_layer_cache_[tab_id];

  if (!thumbnail || !thumbnail->ui_resource_id()) {
    if (static_layer.get()) {
      static_layer->layer()->RemoveFromParent();
      static_layer_cache_.erase(tab_id);
    }
    return nullptr;
  }

  if (!static_layer.get()) {
    static_layer = ThumbnailLayer::Create();
    static_layer_cache_[tab_id] = static_layer;
  }

  static_layer->SetThumbnail(thumbnail);
  return static_layer;
}

void TabContentManager::AttachTab(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  const JavaParamRef<jobject>& jtab,
                                  jint tab_id) {
  TabAndroid* tab = TabAndroid::GetNativeTab(env, jtab);
  scoped_refptr<cc::Layer> layer = tab->GetContentLayer();
  if (!layer.get())
    return;

  scoped_refptr<cc::Layer> cached_layer = live_layer_list_[tab_id];
  if (cached_layer != layer)
    live_layer_list_[tab_id] = layer;
}

void TabContentManager::DetachTab(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  const JavaParamRef<jobject>& jtab,
                                  jint tab_id) {
  scoped_refptr<cc::Layer> current_layer = live_layer_list_[tab_id];
  if (!current_layer.get()) {
    // Empty cached layer should not exist but it is ok if it happens.
    return;
  }

  TabAndroid* tab = TabAndroid::GetNativeTab(env, jtab);
  scoped_refptr<cc::Layer> layer = tab->GetContentLayer();
  // We need to remove if we're getting a detach for our current layer or we're
  // getting a detach with NULL and we have a current layer, which means remove
  //  all layers.
  if (current_layer.get() &&
      (layer.get() == current_layer.get() || !layer.get())) {
    live_layer_list_.erase(tab_id);
  }
}

jboolean TabContentManager::HasFullCachedThumbnail(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint tab_id) {
  return thumbnail_cache_->Get(tab_id, false, false) != nullptr;
}

content::RenderWidgetHostView* TabContentManager::GetRwhvForTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& tab) {
  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, tab);
  DCHECK(tab_android);
  const int tab_id = tab_android->GetAndroidId();
  if (pending_tab_readbacks_.find(tab_id) != pending_tab_readbacks_.end()) {
    return nullptr;
  }

  content::WebContents* web_contents = tab_android->web_contents();
  DCHECK(web_contents);

  content::RenderViewHost* rvh = web_contents->GetRenderViewHost();
  if (!rvh)
    return nullptr;

  content::RenderWidgetHost* rwh = rvh->GetWidget();
  content::RenderWidgetHostView* rwhv = rwh ? rwh->GetView() : nullptr;
  if (!rwhv || !rwhv->IsSurfaceAvailableForCopy())
    return nullptr;

  return rwhv;
}

void TabContentManager::CaptureThumbnail(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& tab,
    jfloat thumbnail_scale,
    jboolean write_to_cache,
    const base::android::JavaParamRef<jobject>& j_callback) {
  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, tab);
  DCHECK(tab_android);
  const int tab_id = tab_android->GetAndroidId();

  content::RenderWidgetHostView* rwhv = GetRwhvForTab(env, obj, tab);
  if (!rwhv) {
    if (j_callback)
      RunObjectCallbackAndroid(j_callback, nullptr);
    return;
  }
  if (write_to_cache && !thumbnail_cache_->CheckAndUpdateThumbnailMetaData(
                            tab_id, tab_android->GetURL())) {
    return;
  }
  TabReadbackCallback readback_done_callback = base::BindOnce(
      &TabContentManager::OnTabReadback, weak_factory_.GetWeakPtr(), tab_id,
      base::android::ScopedJavaGlobalRef<jobject>(j_callback), write_to_cache);
  pending_tab_readbacks_[tab_id] = std::make_unique<TabReadbackRequest>(
      rwhv, thumbnail_scale, !write_to_cache,
      std::move(readback_done_callback));
}

void TabContentManager::CacheTabWithBitmap(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj,
                                           const JavaParamRef<jobject>& tab,
                                           const JavaParamRef<jobject>& bitmap,
                                           jfloat thumbnail_scale) {
  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, tab);
  DCHECK(tab_android);
  int tab_id = tab_android->GetAndroidId();
  GURL url = tab_android->GetURL();

  gfx::JavaBitmap java_bitmap_lock(bitmap);
  SkBitmap skbitmap = gfx::CreateSkBitmapFromJavaBitmap(java_bitmap_lock);
  skbitmap.setImmutable();

  if (thumbnail_cache_->CheckAndUpdateThumbnailMetaData(tab_id, url))
    OnTabReadback(tab_id, nullptr, true, thumbnail_scale, skbitmap);
}

void TabContentManager::InvalidateIfChanged(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj,
                                            jint tab_id,
                                            const JavaParamRef<jstring>& jurl) {
  thumbnail_cache_->InvalidateThumbnailIfChanged(
      tab_id, GURL(base::android::ConvertJavaStringToUTF8(env, jurl)));
}

void TabContentManager::UpdateVisibleIds(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jintArray>& priority,
    jint primary_tab_id) {
  std::list<int> priority_ids;
  jsize length = env->GetArrayLength(priority);
  jint* ints = env->GetIntArrayElements(priority, nullptr);
  for (jsize i = 0; i < length; ++i)
    priority_ids.push_back(static_cast<int>(ints[i]));

  env->ReleaseIntArrayElements(priority, ints, JNI_ABORT);
  thumbnail_cache_->UpdateVisibleIds(priority_ids, primary_tab_id);
}

void TabContentManager::NativeRemoveTabThumbnail(int tab_id) {
  TabReadbackRequestMap::iterator readback_iter =
      pending_tab_readbacks_.find(tab_id);
  if (readback_iter != pending_tab_readbacks_.end())
    readback_iter->second->SetToDropAfterReadback();
  thumbnail_cache_->Remove(tab_id);
}

void TabContentManager::RemoveTabThumbnail(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj,
                                           jint tab_id) {
  NativeRemoveTabThumbnail(tab_id);
}

void TabContentManager::GetEtc1TabThumbnail(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint tab_id,
    const base::android::JavaParamRef<jobject>& j_callback) {
  thumbnail_cache_->DecompressThumbnailFromFile(
      tab_id,
      base::BindRepeating(
          &TabContentManager::SendThumbnailToJava, weak_factory_.GetWeakPtr(),
          base::android::ScopedJavaGlobalRef<jobject>(j_callback),
          /* need_downsampling */ true));
}

void TabContentManager::OnUIResourcesWereEvicted() {
  thumbnail_cache_->OnUIResourcesWereEvicted();
}

void TabContentManager::OnFinishedThumbnailRead(int tab_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TabContentManager_notifyListenersOfThumbnailChange(
      env, weak_java_tab_content_manager_.get(env), tab_id);
}

void TabContentManager::OnTabReadback(
    int tab_id,
    base::android::ScopedJavaGlobalRef<jobject> j_callback,
    bool write_to_cache,
    float thumbnail_scale,
    const SkBitmap& bitmap) {
  TabReadbackRequestMap::iterator readback_iter =
      pending_tab_readbacks_.find(tab_id);

  if (readback_iter != pending_tab_readbacks_.end())
    pending_tab_readbacks_.erase(tab_id);

  if (j_callback) {
    SendThumbnailToJava(j_callback, write_to_cache, true, bitmap);
  }

  if (write_to_cache && thumbnail_scale > 0 && !bitmap.empty())
    thumbnail_cache_->Put(tab_id, bitmap, thumbnail_scale);
}

void TabContentManager::SendThumbnailToJava(
    base::android::ScopedJavaGlobalRef<jobject> j_callback,
    bool need_downsampling,
    bool result,
    const SkBitmap& bitmap) {
  ScopedJavaLocalRef<jobject> j_bitmap;
  if (!bitmap.isNull() && result) {
    // In portrait mode, we want to show thumbnails in squares.
    // Therefore, the thumbnail saved in portrait mode needs to be cropped to
    // a square, or it would be vertically center-aligned, and the top would
    // be hidden.
    // It's fine to horizontally center-align thumbnail saved in landscape
    // mode.
    int scale = need_downsampling ? 2 : 1;
    double aspect_ratio = base::GetFieldTrialParamByFeatureAsDouble(
        chrome::android::kTabGridLayoutAndroid, "thumbnail_aspect_ratio", 1.0);
    aspect_ratio = ThumbnailCache::clampAspectRatio(aspect_ratio, 0.5, 2.0);
    SkIRect dest_subset = {
        0, 0, bitmap.width() / scale,
        std::min(bitmap.height() / scale,
                 (int)(bitmap.width() / aspect_ratio / scale))};
    SkBitmap result_bitmap = skia::ImageOperations::Resize(
        bitmap, skia::ImageOperations::RESIZE_BETTER, bitmap.width() / scale,
        bitmap.height() / scale, dest_subset);
    j_bitmap = gfx::ConvertToJavaBitmap(&result_bitmap);
  }
  RunObjectCallbackAndroid(j_callback, j_bitmap);
}

void TabContentManager::SetCaptureMinRequestTimeForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint timeMs) {
  thumbnail_cache_->SetCaptureMinRequestTimeForTesting(timeMs);
}

jint TabContentManager::GetPendingReadbacksForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return pending_tab_readbacks_.size();
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong JNI_TabContentManager_Init(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj,
                                 jint default_cache_size,
                                 jint approximation_cache_size,
                                 jint compression_queue_max_size,
                                 jint write_queue_max_size,
                                 jboolean use_approximation_thumbnail,
                                 jboolean save_jpeg_thumbnails) {
  TabContentManager* manager = new TabContentManager(
      env, obj, default_cache_size, approximation_cache_size,
      compression_queue_max_size, write_queue_max_size,
      use_approximation_thumbnail, save_jpeg_thumbnails);
  return reinterpret_cast<intptr_t>(manager);
}

}  // namespace android
