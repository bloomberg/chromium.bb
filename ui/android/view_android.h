// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_VIEW_ANDROID_H_
#define UI_ANDROID_VIEW_ANDROID_H_

#include <list>

#include "base/android/jni_weak_ref.h"
#include "base/memory/ref_counted.h"
#include "ui/android/ui_android_export.h"
#include "ui/gfx/geometry/rect_f.h"

class GURL;

namespace cc {
class Layer;
}

namespace ui {

class WindowAndroid;

// A simple container for a UI layer.
// At the root of the hierarchy is a WindowAndroid, when attached.
class UI_ANDROID_EXPORT ViewAndroid {
 public:
  // Stores an anchored view to delete itself at the end of its lifetime
  // automatically. This helps manage the lifecyle without the dependency
  // on |ViewAndroid|.
  class ScopedAnchorView {
   public:
    ScopedAnchorView(JNIEnv* env,
                     const base::android::JavaRef<jobject>& jview,
                     const base::android::JavaRef<jobject>& jdelegate);

    ScopedAnchorView();
    ScopedAnchorView(ScopedAnchorView&& other);
    ScopedAnchorView& operator=(ScopedAnchorView&& other);

    // Calls JNI removeView() on the delegate for cleanup.
    ~ScopedAnchorView();

    void Reset();

    const base::android::ScopedJavaLocalRef<jobject> view() const;

   private:
    // TODO(jinsukkim): Following weak refs can be cast to strong refs which
    //     cannot be garbage-collected and leak memory. Rewrite not to use them.
    //     see comments in crrev.com/2103243002.
    JavaObjectWeakGlobalRef view_;
    JavaObjectWeakGlobalRef delegate_;

    // Default copy/assign disabled by move constructor.
  };

  // A ViewAndroid may have its own delegate or otherwise will
  // use the next available parent's delegate.
  ViewAndroid(const base::android::JavaRef<jobject>& delegate);

  ViewAndroid();
  virtual ~ViewAndroid();

  // The content offset is in CSS pixels, and is used to translate
  // snapshots to the correct part of the view.
  void set_content_offset(const gfx::Vector2dF& content_offset) {
    content_offset_ = content_offset;
  }

  gfx::Vector2dF content_offset() const {
    return content_offset_;
  }

  // Returns the window at the root of this hierarchy, or |null|
  // if disconnected.
  virtual WindowAndroid* GetWindowAndroid() const;

  // Used to return and set the layer for this view. May be |null|.
  cc::Layer* GetLayer() const;
  void SetLayer(scoped_refptr<cc::Layer> layer);

  void SetDelegate(const base::android::JavaRef<jobject>& delegate);

  // Adds this view as a child of another view.
  void AddChild(ViewAndroid* child);

  // Detaches this view from its parent.
  void RemoveFromParent();

  bool StartDragAndDrop(const base::android::JavaRef<jstring>& jtext,
                        const base::android::JavaRef<jobject>& jimage);

  void OnBackgroundColorChanged(unsigned int color);
  void OnTopControlsChanged(float top_controls_offset,
                            float top_content_offset);
  void OnBottomControlsChanged(float bottom_controls_offset,
                               float bottom_content_offset);
  void StartContentIntent(const GURL& content_url, bool is_main_frame);

  ScopedAnchorView AcquireAnchorView();
  void SetAnchorRect(const base::android::JavaRef<jobject>& anchor,
                     const gfx::RectF& bounds);

  // This may return null.
  base::android::ScopedJavaLocalRef<jobject> GetContainerView();

 protected:
  ViewAndroid* parent_;

 private:
  void RemoveChild(ViewAndroid* child);

  // Returns the Java delegate for this view. This is used to delegate work
  // up to the embedding view (or the embedder that can deal with the
  // implementation details).
  const base::android::ScopedJavaLocalRef<jobject>
      GetViewAndroidDelegate() const;

  std::list<ViewAndroid*> children_;
  scoped_refptr<cc::Layer> layer_;
  JavaObjectWeakGlobalRef delegate_;
  gfx::Vector2dF content_offset_;  // in CSS pixel

  DISALLOW_COPY_AND_ASSIGN(ViewAndroid);
};

}  // namespace ui

#endif  // UI_ANDROID_VIEW_ANDROID_H_
