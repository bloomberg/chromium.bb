// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_VIEW_ANDROID_H_
#define UI_ANDROID_VIEW_ANDROID_H_

#include <list>

#include "base/android/jni_weak_ref.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "ui/android/ui_android_export.h"
#include "ui/gfx/geometry/rect_f.h"

namespace cc {
class Layer;
}

namespace ui {
class EventForwarder;
class MotionEventAndroid;
class ViewClient;
class WindowAndroid;

// A simple container for a UI layer.
// At the root of the hierarchy is a WindowAndroid, when attached.
// Dispatches input/view events coming from Java layer. Hit testing against
// those events is implemented so that the |ViewClient| will be invoked
// when the event got hit on the area defined by |layout_params_|.
// Hit testing is done in the order of parent -> child, and from top
// of the stack to back among siblings.
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

  // Layout parameters used to set the view's position and size.
  // Position is in parent's coordinate space.
  struct LayoutParams {
    static LayoutParams MatchParent() { return {true, 0, 0, 0, 0}; }
    static LayoutParams Normal(int x, int y, int width, int height) {
      return {false, x, y, width, height};
    };

    bool match_parent;  // Bounds matches that of the parent if true.
    int x;
    int y;
    int width;
    int height;

    LayoutParams(const LayoutParams& p) = default;

   private:
    LayoutParams(bool match_parent, int x, int y, int width, int height)
        : match_parent(match_parent),
          x(x),
          y(y),
          width(width),
          height(height) {}
  };

  explicit ViewAndroid(ViewClient* view_client);

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

  // Gets (creates one if not present) Java object of the EventForwarder
  // for a view tree in the view hierarchy including this node.
  // Only one instance per the view tree is allowed.
  base::android::ScopedJavaLocalRef<jobject> GetEventForwarder();

  // Adds a child to this view.
  void AddChild(ViewAndroid* child);

  // Moves the give child ViewAndroid to the front of the list so that it can be
  // the first responder of events.
  void MoveToFront(ViewAndroid* child);

  // Detaches this view from its parent.
  void RemoveFromParent();

  // Sets the layout relative to parent. Used to do hit testing against events.
  void SetLayout(LayoutParams params);

  bool StartDragAndDrop(const base::android::JavaRef<jstring>& jtext,
                        const base::android::JavaRef<jobject>& jimage);

  gfx::Size GetPhysicalBackingSize();
  void OnPhysicalBackingSizeChanged(const gfx::Size& size);
  void OnBackgroundColorChanged(unsigned int color);
  void OnTopControlsChanged(float top_controls_offset,
                            float top_content_offset);
  void OnBottomControlsChanged(float bottom_controls_offset,
                               float bottom_content_offset);
  int GetSystemWindowInsetBottom();

  ScopedAnchorView AcquireAnchorView();
  void SetAnchorRect(const base::android::JavaRef<jobject>& anchor,
                     const gfx::RectF& bounds);

  // This may return null.
  base::android::ScopedJavaLocalRef<jobject> GetContainerView();

  float GetDipScale();

 protected:
  ViewAndroid* parent_;

 private:
  friend class EventForwarder;
  friend class ViewAndroidBoundsTest;

  using ViewClientCallback =
      const base::Callback<bool(ViewClient*, const MotionEventAndroid&)>;

  bool OnTouchEvent(const MotionEventAndroid& event, bool for_touch_handle);
  bool OnMouseEvent(const MotionEventAndroid& event);
  bool OnMouseWheelEvent(const MotionEventAndroid& event);

  void RemoveChild(ViewAndroid* child);

  bool HitTest(ViewClientCallback send_to_client,
               const MotionEventAndroid& event);

  static bool SendTouchEventToClient(bool for_touch_handle,
                                     ViewClient* client,
                                     const MotionEventAndroid& event);
  static bool SendMouseEventToClient(ViewClient* client,
                                     const MotionEventAndroid& event);
  static bool SendMouseWheelEventToClient(ViewClient* client,
                                          const MotionEventAndroid& event);

  bool has_event_forwarder() const { return !!event_forwarder_; }

  // Checks if there is any event forwarder in any node up to root.
  static bool RootPathHasEventForwarder(ViewAndroid* view);

  // Checks if there is any event forwarder in the node paths down to
  // each leaf of subtree.
  static bool SubtreeHasEventForwarder(ViewAndroid* view);

  // Returns the Java delegate for this view. This is used to delegate work
  // up to the embedding view (or the embedder that can deal with the
  // implementation details).
  const base::android::ScopedJavaLocalRef<jobject>
      GetViewAndroidDelegate() const;

  std::list<ViewAndroid*> children_;
  scoped_refptr<cc::Layer> layer_;
  JavaObjectWeakGlobalRef delegate_;

  ViewClient* const client_;

  // Basic view layout information. Used to do hit testing deciding whether
  // the passed events should be processed by the view.
  LayoutParams layout_params_;

  gfx::Size physical_size_;

  gfx::Vector2dF content_offset_;  // in CSS pixel.
  std::unique_ptr<EventForwarder> event_forwarder_;

  DISALLOW_COPY_AND_ASSIGN(ViewAndroid);
};

}  // namespace ui

#endif  // UI_ANDROID_VIEW_ANDROID_H_
