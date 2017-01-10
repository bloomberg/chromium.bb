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

namespace cc {
class Layer;
}

namespace ui {

class ViewClient;
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

  explicit ViewAndroid(ViewClient* client);

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

  // Returns |ViewRoot| associated with the current ViewAndroid.
  // Create one if not present.
  base::android::ScopedJavaLocalRef<jobject> GetViewRoot();

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

  ScopedAnchorView AcquireAnchorView();
  void SetAnchorRect(const base::android::JavaRef<jobject>& anchor,
                     const gfx::RectF& bounds);

  gfx::Size GetPhysicalBackingSize();
  void UpdateLayerBounds();

  // Internal implementation of ViewClient forwarding calls to the interface.
  void OnPhysicalBackingSizeChanged(int width, int height);

 protected:
  ViewAndroid* parent_;

 private:
  void RemoveChild(ViewAndroid* child);

  // Checks if any ViewAndroid instance in the tree hierarchy (including
  // all the parents and the children) has |ViewRoot| already.
  bool HasViewRootInTreeHierarchy();

  // Checks if any children (plus this ViewAndroid itself) has |ViewRoot|.
  bool HasViewRootInSubtree();

  // Returns the Java delegate for this view. This is used to delegate work
  // up to the embedding view (or the embedder that can deal with the
  // implementation details).
  const base::android::ScopedJavaLocalRef<jobject>
      GetViewAndroidDelegate() const;

  // Creates a new |ViewRoot| for this ViewAndroid. No parent or child
  // should have |ViewRoot| for this ViewAndroid to have one.
  base::android::ScopedJavaLocalRef<jobject> CreateViewRoot();

  bool HasViewRoot();

  std::list<ViewAndroid*> children_;
  scoped_refptr<cc::Layer> layer_;
  JavaObjectWeakGlobalRef delegate_;
  JavaObjectWeakGlobalRef view_root_;
  ViewClient* const client_;

  int physical_width_pix_;
  int physical_height_pix_;
  gfx::Vector2dF content_offset_;  // in CSS pixel

  DISALLOW_COPY_AND_ASSIGN(ViewAndroid);
};

bool RegisterViewRoot(JNIEnv* env);

}  // namespace ui

#endif  // UI_ANDROID_VIEW_ANDROID_H_
