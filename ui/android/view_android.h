// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_VIEW_ANDROID_H_
#define UI_ANDROID_VIEW_ANDROID_H_

#include <list>

#include "base/android/jni_weak_ref.h"
#include "base/memory/ref_counted.h"
#include "ui/android/ui_android_export.h"
#include "ui/android/view_client.h"
#include "ui/gfx/geometry/rect_f.h"

namespace cc {
class Layer;
}

namespace ui {

class WindowAndroid;

// A simple container for a UI layer.
// At the root of the hierarchy is a WindowAndroid, when attached.
// TODO(jinsukkim): Replace WindowAndroid with ViewRoot for the root of the
//     view hierarchy. See https://crbug.com/671401
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

  // Returns |ViewRoot| of this hierarchy. |null| if the hierarchy isn't
  // attached to a |ViewRoot|.
  virtual ViewAndroid* GetViewRoot();

  // Used to return and set the layer for this view. May be |null|.
  cc::Layer* GetLayer() const;
  void SetLayer(scoped_refptr<cc::Layer> layer);

  void SetDelegate(const base::android::JavaRef<jobject>& delegate);

  // Adds a child to this view.
  void AddChild(ViewAndroid* child);

  // Move the give child ViewAndroid to the top of the list
  // so that it can be the first responder of events.
  void MoveToTop(ViewAndroid* child);

  // Detaches this view from its parent.
  void RemoveFromParent();

  // Set the layout relative to parent. Used to do hit testing against events.
  void SetLayout(int x, int y, int width, int height, bool match_parent);

  bool StartDragAndDrop(const base::android::JavaRef<jstring>& jtext,
                        const base::android::JavaRef<jobject>& jimage);

  ScopedAnchorView AcquireAnchorView();
  void SetAnchorRect(const base::android::JavaRef<jobject>& anchor,
                     const gfx::RectF& bounds);

  // This may return null.
  base::android::ScopedJavaLocalRef<jobject> GetContainerView();

 protected:
  // Internal implementation of ViewClient forwarding calls to the interface.
  bool OnTouchEventInternal(const MotionEventData& event);

  // Virtual for testing.
  virtual float GetDipScale();

  ViewAndroid* parent_;

 private:
  // Returns true only if this is of type |ViewRoot|.
  bool IsViewRoot();

  void RemoveChild(ViewAndroid* child);

  // Returns the Java delegate for this view. This is used to delegate work
  // up to the embedding view (or the embedder that can deal with the
  // implementation details).
  const base::android::ScopedJavaLocalRef<jobject>
      GetViewAndroidDelegate() const;

  // The child view at the back of the list receives event first.
  std::list<ViewAndroid*> children_;
  scoped_refptr<cc::Layer> layer_;
  JavaObjectWeakGlobalRef delegate_;
  ViewClient* const client_;

  // Basic view layout information. Used to do hit testing deciding whether
  // the passed events should be processed by the view.
  gfx::Point origin_;  // In parent's coordinate space.
  gfx::Size size_;
  bool match_parent_;  // Bounds matches that of the parent if true.

  gfx::Vector2dF content_offset_;  // In CSS pixel.

  DISALLOW_COPY_AND_ASSIGN(ViewAndroid);
};

}  // namespace ui

#endif  // UI_ANDROID_VIEW_ANDROID_H_
