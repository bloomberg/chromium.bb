// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/view_android.h"

#include <algorithm>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/containers/adapters.h"
#include "cc/layers/layer.h"
#include "jni/ViewAndroidDelegate_jni.h"
#include "ui/android/event_forwarder.h"
#include "ui/android/view_client.h"
#include "ui/android/window_android.h"
#include "ui/base/layout.h"
#include "ui/events/android/motion_event_android.h"
#include "url/gurl.h"

namespace ui {

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

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

ViewAndroid::ViewAndroid(ViewClient* view_client)
    : parent_(nullptr),
      client_(view_client),
      layout_params_(LayoutParams::MatchParent()) {}

ViewAndroid::ViewAndroid() : ViewAndroid(nullptr) {}

ViewAndroid::~ViewAndroid() {
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
  DCHECK(std::find(children_.begin(), children_.end(), child) ==
         children_.end());
  DCHECK(!RootPathHasEventForwarder(this) || !SubtreeHasEventForwarder(child))
      << "Some view tree path will have more than one event forwarder "
         "if the child is added.";

  // The new child goes to the top, which is the end of the list.
  children_.push_back(child);
  if (child->parent_)
    child->RemoveFromParent();
  child->parent_ = this;
  child->OnPhysicalBackingSizeChanged(physical_size_);
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
                                const gfx::RectF& bounds) {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return;

  float dip_scale = GetDipScale();
  int left_margin = std::round(bounds.x() * dip_scale);
  int top_margin = std::round((content_offset().y() + bounds.y()) * dip_scale);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ViewAndroidDelegate_setViewPosition(
      env, delegate, anchor, bounds.x(), bounds.y(), bounds.width(),
      bounds.height(), dip_scale, left_margin, top_margin);
}

ScopedJavaLocalRef<jobject> ViewAndroid::GetContainerView() {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return nullptr;

  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ViewAndroidDelegate_getContainerView(env, delegate);
}

void ViewAndroid::RemoveChild(ViewAndroid* child) {
  DCHECK(child);
  DCHECK_EQ(child->parent_, this);

  std::list<ViewAndroid*>::iterator it =
      std::find(children_.begin(), children_.end(), child);
  DCHECK(it != children_.end());
  children_.erase(it);
  child->parent_ = nullptr;
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

void ViewAndroid::SetLayer(scoped_refptr<cc::Layer> layer) {
  layer_ = layer;
}

void ViewAndroid::SetLayout(ViewAndroid::LayoutParams params) {
  layout_params_ = params;
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

void ViewAndroid::OnPhysicalBackingSizeChanged(const gfx::Size& size) {
  if (physical_size_ == size)
    return;
  physical_size_ = size;
  client_->OnPhysicalBackingSizeChanged();

  for (auto* child : children_)
    child->OnPhysicalBackingSizeChanged(size);
}

gfx::Size ViewAndroid::GetPhysicalBackingSize() {
  return physical_size_;
}

bool ViewAndroid::OnTouchEvent(const MotionEventAndroid& event,
                               bool for_touch_handle) {
  return HitTest(
      base::Bind(&ViewAndroid::SendTouchEventToClient, for_touch_handle),
      event);
}

bool ViewAndroid::SendTouchEventToClient(bool for_touch_handle,
                                         ViewClient* client,
                                         const MotionEventAndroid& event) {
  return client->OnTouchEvent(event, for_touch_handle);
}

bool ViewAndroid::OnMouseEvent(const MotionEventAndroid& event) {
  return HitTest(base::Bind(&ViewAndroid::SendMouseEventToClient), event);
}

// static
bool ViewAndroid::SendMouseEventToClient(ViewClient* client,
                                         const MotionEventAndroid& event) {
  return client->OnMouseEvent(event);
}

// static
bool ViewAndroid::OnMouseWheelEvent(const MotionEventAndroid& event) {
  return HitTest(base::Bind(&ViewAndroid::SendMouseWheelEventToClient), event);
}

// static
bool ViewAndroid::SendMouseWheelEventToClient(ViewClient* client,
                                              const MotionEventAndroid& event) {
  return client->OnMouseWheelEvent(event);
}

bool ViewAndroid::HitTest(ViewClientCallback send_to_client,
                          const MotionEventAndroid& event) {
  if (client_ && send_to_client.Run(client_, event))
    return true;

  if (!children_.empty()) {
    std::unique_ptr<MotionEventAndroid> e(
        event.Offset(-layout_params_.x, -layout_params_.y));

    // Match from back to front for hit testing.
    for (auto* child : base::Reversed(children_)) {
      bool matched = child->layout_params_.match_parent;
      if (!matched) {
        gfx::Rect bound(child->layout_params_.x, child->layout_params_.y,
                        child->layout_params_.width,
                        child->layout_params_.height);
        matched = bound.Contains(e->GetX(0), e->GetY(0));
      }
      if (matched && child->HitTest(send_to_client, *e))
        return true;
    }
  }
  return false;
}

}  // namespace ui
