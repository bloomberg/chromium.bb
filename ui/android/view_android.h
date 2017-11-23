// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_VIEW_ANDROID_H_
#define UI_ANDROID_VIEW_ANDROID_H_

#include <list>

#include "base/android/jni_array.h"
#include "base/android/jni_weak_ref.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "ui/android/ui_android_export.h"
#include "ui/android/view_android_observer.h"
#include "ui/gfx/geometry/rect_f.h"

class SkBitmap;

namespace cc {
class Layer;
}

namespace gfx {
class Point;
class Size;
}

namespace ui {
class DragEventAndroid;
class EventForwarder;
class GestureEventAndroid;
class MotionEventAndroid;
class ViewClient;
class WindowAndroid;
class ViewAndroidObserver;

// View-related parameters from frame updates.
struct FrameInfo {
  gfx::SizeF viewport_size;  // In CSS pixels.

  // Content offset from the top. Used to translate snapshots to
  // the correct part of the view. In CSS pixels.
  float content_offset;
};

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

  enum class LayoutType {
    // Can have its own size given by |OnSizeChanged| events.
    NORMAL,
    // Always follows its parent's size.
    MATCH_PARENT
  };

  ViewAndroid(ViewClient* view_client, LayoutType layout_type);

  ViewAndroid();
  virtual ~ViewAndroid();

  void UpdateFrameInfo(const FrameInfo& frame_info);
  // content_offset is in CSS scale.
  float content_offset() const { return frame_info_.content_offset; }
  gfx::SizeF viewport_size() const { return frame_info_.viewport_size; }

  // Returns the window at the root of this hierarchy, or |null|
  // if disconnected.
  virtual WindowAndroid* GetWindowAndroid() const;

  // Virtual for testing.
  virtual float GetDipScale();

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

  bool HasFocus();
  void RequestFocus();

  bool StartDragAndDrop(const base::android::JavaRef<jstring>& jtext,
                        const base::android::JavaRef<jobject>& jimage);

  gfx::Size GetPhysicalBackingSize() const;
  gfx::Size GetSize() const;

  void OnSizeChanged(int width, int height);
  void OnPhysicalBackingSizeChanged(const gfx::Size& size);
  void OnCursorChanged(int type,
                       const SkBitmap& custom_image,
                       const gfx::Point& hotspot);
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

  // Return the location of the container view in physical pixels.
  gfx::Point GetLocationOfContainerViewInWindow();

  // Return the location of the point relative to screen coordinate in pixels.
  gfx::PointF GetLocationOnScreen(float x, float y);

  // ViewAndroid does not own |observer|s.
  void AddObserver(ViewAndroidObserver* observer);
  void RemoveObserver(ViewAndroidObserver* observer);

 protected:
  ViewAndroid* parent_;

 private:
  FRIEND_TEST_ALL_PREFIXES(ViewAndroidBoundsTest, MatchesViewInFront);
  FRIEND_TEST_ALL_PREFIXES(ViewAndroidBoundsTest, MatchesViewArea);
  FRIEND_TEST_ALL_PREFIXES(ViewAndroidBoundsTest, MatchesViewAfterMove);
  FRIEND_TEST_ALL_PREFIXES(ViewAndroidBoundsTest,
                           MatchesViewSizeOfkMatchParent);
  FRIEND_TEST_ALL_PREFIXES(ViewAndroidBoundsTest, MatchesViewsWithOffset);
  FRIEND_TEST_ALL_PREFIXES(ViewAndroidBoundsTest, OnSizeChanged);
  friend class EventForwarder;
  friend class ViewAndroidBoundsTest;

  bool OnDragEvent(const DragEventAndroid& event);
  bool OnTouchEvent(const MotionEventAndroid& event);
  bool OnMouseEvent(const MotionEventAndroid& event);
  bool OnMouseWheelEvent(const MotionEventAndroid& event);
  bool OnGestureEvent(const GestureEventAndroid& event);

  void RemoveChild(ViewAndroid* child);

  void OnAttachedToWindow();
  void OnDetachedFromWindow();

  void SetLayoutForTesting(int x, int y, int width, int height);

  template <typename E>
  using ViewClientCallback = const base::Callback<bool(ViewClient*, const E&)>;

  template <typename E>
  bool HitTest(ViewClientCallback<E> send_to_client,
               const E& event,
               const gfx::PointF& point);

  static bool SendDragEventToClient(ViewClient* client,
                                    const DragEventAndroid& event);
  static bool SendTouchEventToClient(ViewClient* client,
                                     const MotionEventAndroid& event);
  static bool SendMouseEventToClient(ViewClient* client,
                                     const MotionEventAndroid& event);
  static bool SendMouseWheelEventToClient(ViewClient* client,
                                          const MotionEventAndroid& event);
  static bool SendGestureEventToClient(ViewClient* client,
                                       const GestureEventAndroid& event);

  bool has_event_forwarder() const { return !!event_forwarder_; }

  bool match_parent() const { return layout_type_ == LayoutType::MATCH_PARENT; }

  // Checks if there is any event forwarder in any node up to root.
  static bool RootPathHasEventForwarder(ViewAndroid* view);

  // Checks if there is any event forwarder in the node paths down to
  // each leaf of subtree.
  static bool SubtreeHasEventForwarder(ViewAndroid* view);

  void OnSizeChangedInternal(const gfx::Size& size);
  void DispatchOnSizeChanged();

  // Returns the Java delegate for this view. This is used to delegate work
  // up to the embedding view (or the embedder that can deal with the
  // implementation details).
  const base::android::ScopedJavaLocalRef<jobject>
      GetViewAndroidDelegate() const;

  std::list<ViewAndroid*> children_;
  base::ObserverList<ViewAndroidObserver> observer_list_;
  scoped_refptr<cc::Layer> layer_;
  JavaObjectWeakGlobalRef delegate_;

  ViewClient* const client_;

  // Basic view layout information. Used to do hit testing deciding whether
  // the passed events should be processed by the view. Unit in DIP.
  gfx::Rect view_rect_;
  const LayoutType layout_type_;

  // In physical pixel.
  gfx::Size physical_size_;

  FrameInfo frame_info_;

  std::unique_ptr<EventForwarder> event_forwarder_;

  DISALLOW_COPY_AND_ASSIGN(ViewAndroid);
};

}  // namespace ui

#endif  // UI_ANDROID_VIEW_ANDROID_H_
