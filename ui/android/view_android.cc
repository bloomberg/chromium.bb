// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/view_android.h"

#include <algorithm>
#include <cmath>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/containers/adapters.h"
#include "base/stl_util.h"
#include "cc/layers/layer.h"
#include "jni/ViewAndroidDelegate_jni.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "ui/android/event_forwarder.h"
#include "ui/android/view_client.h"
#include "ui/android/window_android.h"
#include "ui/base/layout.h"
#include "ui/events/android/drag_event_android.h"
#include "ui/events/android/gesture_event_android.h"
#include "ui/events/android/motion_event_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/gurl.h"

namespace ui {

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using blink::WebCursorInfo;

ViewAndroid::ScopedAnchorView::ScopedAnchorView(
    JNIEnv* env,
    const JavaRef<jobject>& jview,
    const JavaRef<jobject>& jdelegate)
    : view_(env, jview.obj()), delegate_(env, jdelegate.obj()) {
  // If there's a view, then we need a delegate to remove it.
  DCHECK(!jdelegate.is_null() || jview.is_null());
}

ViewAndroid::ScopedAnchorView::ScopedAnchorView() { }

ViewAndroid::ScopedAnchorView::ScopedAnchorView(ScopedAnchorView&& other) {
  view_ = other.view_;
  other.view_.reset();
  delegate_ = other.delegate_;
  other.delegate_.reset();
}

ViewAndroid::ScopedAnchorView&
ViewAndroid::ScopedAnchorView::operator=(ScopedAnchorView&& other) {
  if (this != &other) {
    view_ = other.view_;
    other.view_.reset();
    delegate_ = other.delegate_;
    other.delegate_.reset();
  }
  return *this;
}

ViewAndroid::ScopedAnchorView::~ScopedAnchorView() {
  Reset();
}

void ViewAndroid::ScopedAnchorView::Reset() {
  JNIEnv* env = base::android::AttachCurrentThread();
  const ScopedJavaLocalRef<jobject> view = view_.get(env);
  const ScopedJavaLocalRef<jobject> delegate = delegate_.get(env);
  if (!view.is_null() && !delegate.is_null()) {
    Java_ViewAndroidDelegate_removeView(env, delegate, view);
  }
  view_.reset();
  delegate_.reset();
}

const base::android::ScopedJavaLocalRef<jobject>
ViewAndroid::ScopedAnchorView::view() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return view_.get(env);
}

ViewAndroid::ViewAndroid(ViewClient* view_client, LayoutType layout_type)
    : parent_(nullptr), client_(view_client), layout_type_(layout_type) {}

ViewAndroid::ViewAndroid() : ViewAndroid(nullptr, LayoutType::NORMAL) {}

ViewAndroid::~ViewAndroid() {
  observer_list_.Clear();
  RemoveFromParent();

  for (std::list<ViewAndroid*>::iterator it = children_.begin();
       it != children_.end(); it++) {
    DCHECK_EQ((*it)->parent_, this);
    (*it)->parent_ = nullptr;
  }
}

void ViewAndroid::SetDelegate(const JavaRef<jobject>& delegate) {
  // A ViewAndroid may have its own delegate or otherwise will use the next
  // available parent's delegate.
  JNIEnv* env = base::android::AttachCurrentThread();
  delegate_ = JavaObjectWeakGlobalRef(env, delegate);
}

void ViewAndroid::UpdateFrameInfo(const FrameInfo& frame_info) {
  frame_info_ = frame_info;
}

float ViewAndroid::GetDipScale() {
  return ui::GetScaleFactorForNativeView(this);
}

ScopedJavaLocalRef<jobject> ViewAndroid::GetEventForwarder() {
  if (!event_forwarder_) {
    DCHECK(!RootPathHasEventForwarder(parent_))
        << "The view tree path already has an event forwarder.";
    DCHECK(!SubtreeHasEventForwarder(this))
        << "The view tree path already has an event forwarder.";
    event_forwarder_.reset(new EventForwarder(this));
  }
  return event_forwarder_->GetJavaObject();
}

void ViewAndroid::AddChild(ViewAndroid* child) {
  DCHECK(child);
  DCHECK(!base::ContainsValue(children_, child));
  DCHECK(!RootPathHasEventForwarder(this) || !SubtreeHasEventForwarder(child))
      << "Some view tree path will have more than one event forwarder "
         "if the child is added.";

  // The new child goes to the top, which is the end of the list.
  children_.push_back(child);
  if (child->parent_)
    child->RemoveFromParent();
  child->parent_ = this;

  // Empty physical backing size need not propagating down since it can
  // accidentally overwrite the valid ones in the children.
  if (!physical_size_.IsEmpty())
    child->OnPhysicalBackingSizeChanged(physical_size_);

  // Empty view size also need not propagating down in order to prevent
  // spurious events with empty size from being sent down.
  if (child->match_parent() && !view_rect_.IsEmpty() &&
      child->GetSize() != view_rect_.size()) {
    child->OnSizeChangedInternal(view_rect_.size());
    child->DispatchOnSizeChanged();
  }

  if (GetWindowAndroid())
    child->OnAttachedToWindow();
}

// static
bool ViewAndroid::RootPathHasEventForwarder(ViewAndroid* view) {
  while (view) {
    if (view->has_event_forwarder())
      return true;
    view = view->parent_;
  }

  return false;
}

// static
bool ViewAndroid::SubtreeHasEventForwarder(ViewAndroid* view) {
  if (view->has_event_forwarder())
    return true;

  for (auto* child : view->children_) {
    if (SubtreeHasEventForwarder(child))
      return true;
  }
  return false;
}

void ViewAndroid::MoveToFront(ViewAndroid* child) {
  DCHECK(child);
  auto it = std::find(children_.begin(), children_.end(), child);
  DCHECK(it != children_.end());

  // Top element is placed at the end of the list.
  if (*it != children_.back())
    children_.splice(children_.end(), children_, it);
}

void ViewAndroid::RemoveFromParent() {
  if (parent_)
    parent_->RemoveChild(this);
}

ViewAndroid::ScopedAnchorView ViewAndroid::AcquireAnchorView() {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return ViewAndroid::ScopedAnchorView();

  JNIEnv* env = base::android::AttachCurrentThread();
  return ViewAndroid::ScopedAnchorView(
      env, Java_ViewAndroidDelegate_acquireView(env, delegate), delegate);
}

void ViewAndroid::SetAnchorRect(const JavaRef<jobject>& anchor,
                                const gfx::RectF& bounds_dip) {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return;

  float dip_scale = GetDipScale();
  int left_margin = std::round(bounds_dip.x() * dip_scale);
  // Note that content_offset() is in CSS scale and bounds_dip is in DIP scale
  // (i.e., CSS pixels * page scale factor), but the height of browser control
  // is not affected by page scale factor. Thus, content_offset() in CSS scale
  // is also in DIP scale.
  int top_margin = std::round((content_offset() + bounds_dip.y()) * dip_scale);
  const gfx::RectF bounds_px = gfx::ScaleRect(bounds_dip, dip_scale);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ViewAndroidDelegate_setViewPosition(
      env, delegate, anchor, bounds_px.x(), bounds_px.y(), bounds_px.width(),
      bounds_px.height(), left_margin, top_margin);
}

ScopedJavaLocalRef<jobject> ViewAndroid::GetContainerView() {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return nullptr;

  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ViewAndroidDelegate_getContainerView(env, delegate);
}

gfx::Point ViewAndroid::GetLocationOfContainerViewInWindow() {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return gfx::Point();

  JNIEnv* env = base::android::AttachCurrentThread();
  gfx::Point result(
      Java_ViewAndroidDelegate_getXLocationOfContainerViewInWindow(env,
                                                                   delegate),
      Java_ViewAndroidDelegate_getYLocationOfContainerViewInWindow(env,
                                                                   delegate));

  return result;
}

gfx::PointF ViewAndroid::GetLocationOnScreen(float x, float y) {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return gfx::PointF();

  JNIEnv* env = base::android::AttachCurrentThread();
  float loc_x = Java_ViewAndroidDelegate_getXLocationOnScreen(env, delegate);
  float loc_y = Java_ViewAndroidDelegate_getYLocationOnScreen(env, delegate);
  return gfx::PointF(x + loc_x, y + loc_y);
}

void ViewAndroid::RemoveChild(ViewAndroid* child) {
  DCHECK(child);
  DCHECK_EQ(child->parent_, this);

  if (GetWindowAndroid())
    child->OnDetachedFromWindow();
  std::list<ViewAndroid*>::iterator it =
      std::find(children_.begin(), children_.end(), child);
  DCHECK(it != children_.end());
  children_.erase(it);
  child->parent_ = nullptr;
}

void ViewAndroid::AddObserver(ViewAndroidObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ViewAndroid::RemoveObserver(ViewAndroidObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void ViewAndroid::OnAttachedToWindow() {
  for (auto& observer : observer_list_)
    observer.OnAttachedToWindow();
  for (auto* child : children_)
    child->OnAttachedToWindow();
}

void ViewAndroid::OnDetachedFromWindow() {
  for (auto& observer : observer_list_)
    observer.OnDetachedFromWindow();
  for (auto* child : children_)
    child->OnDetachedFromWindow();
}

WindowAndroid* ViewAndroid::GetWindowAndroid() const {
  return parent_ ? parent_->GetWindowAndroid() : nullptr;
}

const ScopedJavaLocalRef<jobject> ViewAndroid::GetViewAndroidDelegate()
    const {
  JNIEnv* env = base::android::AttachCurrentThread();
  const ScopedJavaLocalRef<jobject> delegate = delegate_.get(env);
  if (!delegate.is_null())
    return delegate;

  return parent_ ? parent_->GetViewAndroidDelegate() : delegate;
}

cc::Layer* ViewAndroid::GetLayer() const {
  return layer_.get();
}

bool ViewAndroid::HasFocus() {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return false;
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ViewAndroidDelegate_hasFocus(env, delegate);
}

void ViewAndroid::RequestFocus() {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ViewAndroidDelegate_requestFocus(env, delegate);
}

void ViewAndroid::SetLayer(scoped_refptr<cc::Layer> layer) {
  layer_ = layer;
}

bool ViewAndroid::StartDragAndDrop(const JavaRef<jstring>& jtext,
                                   const JavaRef<jobject>& jimage) {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return false;
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ViewAndroidDelegate_startDragAndDrop(env, delegate, jtext,
                                                   jimage);
}

void ViewAndroid::OnCursorChanged(int type,
                                  const SkBitmap& custom_image,
                                  const gfx::Point& hotspot) {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  if (type == WebCursorInfo::kTypeCustom) {
    if (custom_image.drawsNothing()) {
      Java_ViewAndroidDelegate_onCursorChanged(env, delegate,
                                               WebCursorInfo::kTypePointer);
      return;
    }
    ScopedJavaLocalRef<jobject> java_bitmap =
        gfx::ConvertToJavaBitmap(&custom_image);
    Java_ViewAndroidDelegate_onCursorChangedToCustom(env, delegate, java_bitmap,
                                                     hotspot.x(), hotspot.y());
  } else {
    Java_ViewAndroidDelegate_onCursorChanged(env, delegate, type);
  }
}

void ViewAndroid::OnBackgroundColorChanged(unsigned int color) {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ViewAndroidDelegate_onBackgroundColorChanged(env, delegate, color);
}

void ViewAndroid::OnTopControlsChanged(float top_controls_offset,
                                       float top_content_offset) {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ViewAndroidDelegate_onTopControlsChanged(
      env, delegate, top_controls_offset, top_content_offset);
}

void ViewAndroid::OnBottomControlsChanged(float bottom_controls_offset,
                                          float bottom_content_offset) {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ViewAndroidDelegate_onBottomControlsChanged(
      env, delegate, bottom_controls_offset, bottom_content_offset);
}

int ViewAndroid::GetSystemWindowInsetBottom() {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return 0;
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ViewAndroidDelegate_getSystemWindowInsetBottom(env, delegate);
}

void ViewAndroid::OnSizeChanged(int width, int height) {
  // Match-parent view must not receive size events.
  DCHECK(!match_parent());

  float scale = GetDipScale();
  gfx::Size size(std::ceil(width / scale), std::ceil(height / scale));
  if (view_rect_.size() == size)
    return;

  OnSizeChangedInternal(size);

  // Signal resize event after all the views in the tree get the updated size.
  DispatchOnSizeChanged();
}

void ViewAndroid::OnSizeChangedInternal(const gfx::Size& size) {
  if (view_rect_.size() == size)
    return;

  view_rect_.set_size(size);
  for (auto* child : children_) {
    if (child->match_parent())
      child->OnSizeChangedInternal(size);
  }
}

void ViewAndroid::DispatchOnSizeChanged() {
  client_->OnSizeChanged();
  for (auto* child : children_) {
    if (child->match_parent())
      child->DispatchOnSizeChanged();
  }
}

void ViewAndroid::OnPhysicalBackingSizeChanged(const gfx::Size& size) {
  if (physical_size_ == size)
    return;
  physical_size_ = size;
  client_->OnPhysicalBackingSizeChanged();

  for (auto* child : children_)
    child->OnPhysicalBackingSizeChanged(size);
}

gfx::Size ViewAndroid::GetPhysicalBackingSize() const {
  return physical_size_;
}

gfx::Size ViewAndroid::GetSize() const {
  return view_rect_.size();
}

bool ViewAndroid::OnDragEvent(const DragEventAndroid& event) {
  return HitTest(base::Bind(&ViewAndroid::SendDragEventToClient), event,
                 event.location_f());
}

// static
bool ViewAndroid::SendDragEventToClient(ViewClient* client,
                                        const DragEventAndroid& event) {
  return client->OnDragEvent(event);
}

bool ViewAndroid::OnTouchEvent(const MotionEventAndroid& event) {
  return HitTest(base::Bind(&ViewAndroid::SendTouchEventToClient), event,
                 event.GetPoint());
}

// static
bool ViewAndroid::SendTouchEventToClient(ViewClient* client,
                                         const MotionEventAndroid& event) {
  return client->OnTouchEvent(event);
}

bool ViewAndroid::OnMouseEvent(const MotionEventAndroid& event) {
  return HitTest(base::Bind(&ViewAndroid::SendMouseEventToClient), event,
                 event.GetPoint());
}

// static
bool ViewAndroid::SendMouseEventToClient(ViewClient* client,
                                         const MotionEventAndroid& event) {
  return client->OnMouseEvent(event);
}

bool ViewAndroid::OnMouseWheelEvent(const MotionEventAndroid& event) {
  return HitTest(base::Bind(&ViewAndroid::SendMouseWheelEventToClient), event,
                 event.GetPoint());
}

// static
bool ViewAndroid::SendMouseWheelEventToClient(ViewClient* client,
                                              const MotionEventAndroid& event) {
  return client->OnMouseWheelEvent(event);
}

bool ViewAndroid::OnGestureEvent(const GestureEventAndroid& event) {
  return HitTest(base::Bind(&ViewAndroid::SendGestureEventToClient), event,
                 event.location());
}

// static
bool ViewAndroid::SendGestureEventToClient(ViewClient* client,
                                           const GestureEventAndroid& event) {
  return client->OnGestureEvent(event);
}

template <typename E>
bool ViewAndroid::HitTest(ViewClientCallback<E> send_to_client,
                          const E& event,
                          const gfx::PointF& point) {
  if (client_) {
    if (view_rect_.origin().IsOrigin()) {  // (x, y) == (0, 0)
      if (send_to_client.Run(client_, event))
        return true;
    } else {
      std::unique_ptr<E> e(event.CreateFor(point));
      if (send_to_client.Run(client_, *e))
        return true;
    }
  }

  if (!children_.empty()) {
    gfx::PointF offset_point(point);
    offset_point.Offset(-view_rect_.x(), -view_rect_.y());
    gfx::Point int_point = gfx::ToFlooredPoint(offset_point);

    // Match from back to front for hit testing.
    for (auto* child : base::Reversed(children_)) {
      bool matched = child->match_parent();
      if (!matched)
        matched = child->view_rect_.Contains(int_point);
      if (matched && child->HitTest(send_to_client, event, offset_point))
        return true;
    }
  }
  return false;
}

void ViewAndroid::SetLayoutForTesting(int x, int y, int width, int height) {
  view_rect_.SetRect(x, y, width, height);
}

}  // namespace ui
