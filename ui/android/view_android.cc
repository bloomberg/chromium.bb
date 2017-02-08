// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/view_android.h"

#include <algorithm>

#include "base/android/jni_android.h"
#include "cc/layers/layer.h"
#include "jni/ViewAndroidDelegate_jni.h"
#include "ui/android/window_android.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ui {

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

// ViewAndroid::ScopedAndroidView
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

// ViewAndroid
ViewAndroid::ViewAndroid(ViewClient* client) : parent_(nullptr),
                                               client_(client) {}
ViewAndroid::ViewAndroid() : ViewAndroid(nullptr) {}

ViewAndroid::~ViewAndroid() {
  RemoveFromParent();

  auto children_copy = std::list<ViewAndroid*>(children_);
  for (auto& child : children_copy)
    RemoveChild(child);
}

void ViewAndroid::SetDelegate(const JavaRef<jobject>& delegate) {
  // A ViewAndroid may have its own delegate or otherwise will
  // use the next available parent's delegate.
  JNIEnv* env = base::android::AttachCurrentThread();
  delegate_ = JavaObjectWeakGlobalRef(env, delegate);
}

void ViewAndroid::AddChild(ViewAndroid* child) {
  DCHECK(child);
  DCHECK(!child->IsViewRoot());  // ViewRoot cannot be a child.
  DCHECK(std::find(children_.begin(), children_.end(), child) ==
         children_.end());

  // The new child goes to the top, which is the end of the list.
  children_.push_back(child);
  if (child->parent_)
    child->RemoveFromParent();
  child->parent_ = this;
}

void ViewAndroid::MoveToTop(ViewAndroid* child) {
  DCHECK(child);
  auto it = std::find(children_.begin(), children_.end(), child);
  DCHECK(it != children_.end());

  // Top element is placed at the end of the list.
  if (*it != children_.back())
    children_.splice(children_.rbegin().base(), children_, it);
}

void ViewAndroid::RemoveFromParent() {
  if (parent_)
    parent_->RemoveChild(this);
}

void ViewAndroid::SetLayout(int x,
                            int y,
                            int width,
                            int height,
                            bool match_parent) {
  DCHECK(!match_parent || (x == 0 && y == 0 && width == 0 && height == 0));
  origin_.SetPoint(x, y);
  size_.SetSize(width, height);
  match_parent_ = match_parent;
}

ViewAndroid::ScopedAnchorView ViewAndroid::AcquireAnchorView() {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return ViewAndroid::ScopedAnchorView();

  JNIEnv* env = base::android::AttachCurrentThread();
  return ViewAndroid::ScopedAnchorView(
      env, Java_ViewAndroidDelegate_acquireView(env, delegate), delegate);
}

float ViewAndroid::GetDipScale() {
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(this)
      .device_scale_factor();
}

void ViewAndroid::SetAnchorRect(const JavaRef<jobject>& anchor,
                                const gfx::RectF& bounds) {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return;

  float scale = GetDipScale();
  int left_margin = std::round(bounds.x() * scale);
  int top_margin = std::round((content_offset().y() + bounds.y()) * scale);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ViewAndroidDelegate_setViewPosition(
      env, delegate, anchor, bounds.x(), bounds.y(), bounds.width(),
      bounds.height(), scale, left_margin, top_margin);
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

ViewAndroid* ViewAndroid::GetViewRoot() {
  return parent_ ? parent_->GetViewRoot() : nullptr;
}

bool ViewAndroid::IsViewRoot() {
  return GetViewRoot() == this;
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

bool ViewAndroid::OnTouchEventInternal(const MotionEventData& event) {
  if (!children_.empty()) {
    const MotionEventData& e =
      origin_.IsOrigin() ? event : event.Offset(-origin_.x(), -origin_.y());

    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
      bool matched = (*it)->match_parent_;
      if (!matched) {
        gfx::Rect bound((*it)->origin_, (*it)->size_);
        matched = bound.Contains(e.GetX(), e.GetY());
      }
      if (matched && (*it)->OnTouchEventInternal(e))
        return true;
    }
  }

  if (client_ && client_->OnTouchEvent(event))
    return true;

  return false;
}

}  // namespace ui
