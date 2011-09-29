// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window.h"

#include <algorithm>

#include "base/logging.h"
#include "ui/aura/desktop.h"
#include "ui/aura/event.h"
#include "ui/aura/event_filter.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/animation/multi_animation.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/compositor/layer.h"

namespace aura {

using internal::RootWindow;

Window::Window(WindowDelegate* delegate)
    : delegate_(delegate),
      visibility_(VISIBILITY_HIDDEN),
      parent_(NULL),
      id_(-1),
      user_data_(NULL) {
}

Window::~Window() {
  // Let the delegate know we're in the processing of destroying.
  if (delegate_)
    delegate_->OnWindowDestroying();

  // Let the root know so that it can remove any references to us.
  RootWindow* root = GetRoot();
  if (root)
    root->WindowDestroying(this);

  // Then destroy the children.
  while (!children_.empty()) {
    Window* child = children_[0];
    delete child;
    // Deleting the child so remove it from out children_ list.
    DCHECK(std::find(children_.begin(), children_.end(), child) ==
           children_.end());
  }
  // And let the delegate do any post cleanup.
  if (delegate_)
    delegate_->OnWindowDestroyed();
  if (parent_)
    parent_->RemoveChild(this);
}

void Window::Init() {
  ui::Layer::LayerType type = ui::Layer::LAYER_HAS_NO_TEXTURE;
  if (delegate_)
    type = ui::Layer::LAYER_HAS_TEXTURE;
  layer_.reset(new ui::Layer(Desktop::GetInstance()->compositor(), type));
  layer_->set_delegate(this);
}

void Window::SetVisibility(Visibility visibility) {
  if (visibility_ == visibility)
    return;

  visibility_ = visibility;
  layer_->SetVisible(visibility_ != VISIBILITY_HIDDEN);
  if (layer_->visible())
    SchedulePaint();
  if (visibility_ != VISIBILITY_SHOWN)
    ReleaseCapture();
  if ((visibility_ == VISIBILITY_HIDDEN &&
       Desktop::GetInstance()->active_window() == this) ||
      (visibility_ == VISIBILITY_HIDDEN &&
       !Desktop::GetInstance()->active_window())) {
    Desktop::GetInstance()->ActivateTopmostWindow();
  }
}

void Window::SetLayoutManager(LayoutManager* layout_manager) {
  layout_manager_.reset(layout_manager);
}

void Window::SetBounds(const gfx::Rect& new_bounds) {
  // TODO: funnel this through the Desktop.
  gfx::Rect old_bounds = bounds();
  bool was_move = old_bounds.size() == new_bounds.size();
  layer_->SetBounds(new_bounds);

  if (layout_manager_.get())
    layout_manager_->OnWindowResized();
  if (delegate_)
    delegate_->OnBoundsChanged(old_bounds, new_bounds);
  if (was_move)
    SchedulePaintInRect(gfx::Rect());
  else
    SchedulePaint();
}

const gfx::Rect& Window::bounds() const {
  return layer_->bounds();
}

void Window::SchedulePaintInRect(const gfx::Rect& rect) {
  layer_->SchedulePaint(rect);
}

void Window::SetCanvas(const SkCanvas& canvas, const gfx::Point& origin) {
  // TODO: figure out how this is going to work when animating the layer. In
  // particular if we're animating the size then the underlying Texture is going
  // to be unhappy if we try to set a texture on a size bigger than the size of
  // the texture.
  layer_->SetCanvas(canvas, origin);
}

void Window::SetParent(Window* parent) {
  if (parent)
    parent->AddChild(this);
  else
    Desktop::GetInstance()->toplevel_window_container()->AddChild(this);
}

bool Window::IsToplevelWindowContainer() const {
  return false;
}

void Window::MoveChildToFront(Window* child) {
  DCHECK_EQ(child->parent(), this);
  const Windows::iterator i(std::find(children_.begin(), children_.end(),
                                      child));
  DCHECK(i != children_.end());
  children_.erase(i);

  // TODO(beng): this obviously has to handle different window types.
  children_.insert(children_.begin() + children_.size(), child);
  SchedulePaintInRect(gfx::Rect());

  ui::Layer* parent_layer = child->layer()->parent();
  parent_layer->Remove(child->layer());
  parent_layer->Add(child->layer());
}

void Window::AddChild(Window* child) {
  DCHECK(std::find(children_.begin(), children_.end(), child) ==
      children_.end());
  child->parent_ = this;
  layer_->Add(child->layer_.get());
  children_.push_back(child);
}

void Window::RemoveChild(Window* child) {
  Windows::iterator i = std::find(children_.begin(), children_.end(), child);
  DCHECK(i != children_.end());
  child->parent_ = NULL;
  layer_->Remove(child->layer_.get());
  children_.erase(i);
}

// static
void Window::ConvertPointToWindow(Window* source,
                                  Window* target,
                                  gfx::Point* point) {
  ui::Layer::ConvertPointToLayer(source->layer(), target->layer(), point);
}

void Window::SetEventFilter(EventFilter* event_filter) {
  event_filter_.reset(event_filter);
}

bool Window::OnMouseEvent(MouseEvent* event) {
  if (!parent_)
    return false;
  if (!parent_->event_filter_.get())
    parent_->SetEventFilter(new EventFilter(parent_));
  return parent_->event_filter_->OnMouseEvent(this, event) ||
      delegate_->OnMouseEvent(event);
}

bool Window::OnKeyEvent(KeyEvent* event) {
  return delegate_->OnKeyEvent(event);
}

bool Window::HitTest(const gfx::Point& point) {
  gfx::Rect local_bounds(gfx::Point(), bounds().size());
  // TODO(beng): hittest masks.
  return local_bounds.Contains(point);
}

Window* Window::GetEventHandlerForPoint(const gfx::Point& point) {
  Windows::const_reverse_iterator i = children_.rbegin();
  for (; i != children_.rend(); ++i) {
    Window* child = *i;
    if (child->visibility() == Window::VISIBILITY_HIDDEN)
      continue;
    gfx::Point point_in_child_coords(point);
    Window::ConvertPointToWindow(this, child, &point_in_child_coords);
    if (child->HitTest(point_in_child_coords)) {
      Window* handler = child->GetEventHandlerForPoint(point_in_child_coords);
      if (handler)
        return handler;
    }
  }
  return delegate_ ? this : NULL;
}

internal::FocusManager* Window::GetFocusManager() {
  return parent_ ? parent_->GetFocusManager() : NULL;
}

void Window::SetCapture() {
  if (visibility_ != VISIBILITY_SHOWN)
    return;

  RootWindow* root = GetRoot();
  if (!root)
    return;

  root->SetCapture(this);
}

void Window::ReleaseCapture() {
  RootWindow* root = GetRoot();
  if (!root)
    return;

  root->ReleaseCapture(this);
}

bool Window::HasCapture() {
  RootWindow* root = GetRoot();
  return root && root->capture_window() == this;
}

// static
ui::Animation* Window::CreateDefaultAnimation() {
  std::vector<ui::MultiAnimation::Part> parts;
  parts.push_back(ui::MultiAnimation::Part(200, ui::Tween::LINEAR));
  ui::MultiAnimation* multi_animation = new ui::MultiAnimation(parts);
  multi_animation->set_continuous(false);
  return multi_animation;
}

internal::RootWindow* Window::GetRoot() {
  return parent_ ? parent_->GetRoot() : NULL;
}

void Window::SchedulePaint() {
  SchedulePaintInRect(gfx::Rect(0, 0, bounds().width(), bounds().height()));
}

void Window::OnPaintLayer(gfx::Canvas* canvas) {
  delegate_->OnPaint(canvas);
}

}  // namespace aura
