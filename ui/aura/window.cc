// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "ui/aura/client/event_client.h"
#include "ui/aura/client/stacking_client.h"
#include "ui/aura/client/visibility_client.h"
#include "ui/aura/env.h"
#include "ui/aura/event.h"
#include "ui/aura/event_filter.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_observer.h"
#include "ui/base/animation/multi_animation.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/screen.h"

namespace aura {

namespace {

Window* GetParentForWindow(Window* window, Window* suggested_parent) {
  if (suggested_parent)
    return suggested_parent;
  if (client::GetStackingClient())
    return client::GetStackingClient()->GetDefaultParent(window);
  return NULL;
}

}  // namespace

Window::TestApi::TestApi(Window* window) : window_(window) {}

bool Window::TestApi::OwnsLayer() const {
  return !!window_->layer_owner_.get();
}

Window::Window(WindowDelegate* delegate)
    : type_(client::WINDOW_TYPE_UNKNOWN),
      owned_by_parent_(true),
      delegate_(delegate),
      layer_(NULL),
      parent_(NULL),
      transient_parent_(NULL),
      visible_(false),
      id_(-1),
      transparent_(false),
      user_data_(NULL),
      ignore_events_(false) {
}

Window::~Window() {
  // layer_ can be NULL if Init() wasn't invoked, which can happen
  // only in tests.
  if (layer_)
    layer_->SuppressPaint();

  // Let the delegate know we're in the processing of destroying.
  if (delegate_)
    delegate_->OnWindowDestroying();
  FOR_EACH_OBSERVER(WindowObserver, observers_, OnWindowDestroying(this));

  // Let the root know so that it can remove any references to us.
  RootWindow* root_window = GetRootWindow();
  if (root_window)
    root_window->OnWindowDestroying(this);

  // Then destroy the children.
  size_t child_count = children_.size();
  while (child_count-- > 0) {
    Window* child = children_[0];
    if (child->owned_by_parent_) {
      delete child;
      // Deleting the child so remove it from out children_ list.
      DCHECK(std::find(children_.begin(), children_.end(), child) ==
             children_.end());
    } else {
      // Even if we can't delete the child, we still need to remove it from the
      // parent so that relevant bookkeeping (parent_ back-pointers etc) are
      // updated.
      RemoveChild(child);
    }
  }

  // Removes ourselves from our transient parent (if it hasn't been done by the
  // RootWindow).
  if (transient_parent_)
    transient_parent_->RemoveTransientChild(this);

  // The window needs to be removed from the parent before calling the
  // WindowDestroyed callbacks of delegate and the observers.
  if (parent_)
    parent_->RemoveChild(this);

  // And let the delegate do any post cleanup.
  // TODO(beng): Figure out if this notification needs to happen here, or if it
  // can be moved down adjacent to the observer notification. If it has to be
  // done here, the reason why should be documented.
  if (delegate_)
    delegate_->OnWindowDestroyed();

  // Destroy transient children, only after we've removed ourselves from our
  // parent, as destroying an active transient child may otherwise attempt to
  // refocus us.
  Windows transient_children(transient_children_);
  STLDeleteElements(&transient_children);
  DCHECK(transient_children_.empty());

  FOR_EACH_OBSERVER(WindowObserver, observers_, OnWindowDestroyed(this));

  // Clear properties.
  for (std::map<const void*, Value>::const_iterator iter = prop_map_.begin();
       iter != prop_map_.end();
       ++iter) {
    if (iter->second.deallocator)
      (*iter->second.deallocator)(iter->second.value);
  }
  prop_map_.clear();

  // If we have layer it will either be destroyed by layer_owner_'s dtor, or by
  // whoever acquired it. We don't have a layer if Init() wasn't invoked, which
  // can happen in tests.
  if (layer_)
    layer_->set_delegate(NULL);
  layer_ = NULL;
}

void Window::Init(ui::LayerType layer_type) {
  layer_ = new ui::Layer(layer_type);
  layer_owner_.reset(layer_);
  layer_->SetVisible(false);
  layer_->set_delegate(this);
  UpdateLayerName(name_);
  layer_->SetFillsBoundsOpaquely(!transparent_);

  Env::GetInstance()->NotifyWindowInitialized(this);
}

void Window::SetType(client::WindowType type) {
  // Cannot change type after the window is initialized.
  DCHECK(!layer());
  type_ = type;
}

void Window::SetName(const std::string& name) {
  name_ = name;

  if (layer())
    UpdateLayerName(name_);
}

void Window::SetTransparent(bool transparent) {
  // Cannot change transparent flag after the window is initialized.
  DCHECK(!layer());
  transparent_ = transparent;
}

ui::Layer* Window::AcquireLayer() {
  return layer_owner_.release();
}

RootWindow* Window::GetRootWindow() {
  return const_cast<RootWindow*>(
      static_cast<const Window*>(this)->GetRootWindow());
}

const RootWindow* Window::GetRootWindow() const {
  return parent_ ? parent_->GetRootWindow() : NULL;
}

void Window::Show() {
  SetVisible(true);
}

void Window::Hide() {
  for (Windows::iterator it = transient_children_.begin();
       it != transient_children_.end(); ++it) {
    (*it)->Hide();
  }
  SetVisible(false);
  ReleaseCapture();
}

bool Window::IsVisible() const {
  // Layer visibility can be inconsistent with window visibility, for example
  // when a Window is hidden, we want this function to return false immediately
  // after, even though the client may decide to animate the hide effect (and
  // so the layer will be visible for some time after Hide() is called).
  return visible_ && layer_ && layer_->IsDrawn();
}

gfx::Rect Window::GetBoundsInRootWindow() const {
  // TODO(beng): There may be a better way to handle this, and the existing code
  //             is likely wrong anyway in a multi-monitor world, but this will
  //             do for now.
  if (!GetRootWindow())
    return bounds();
  gfx::Point origin = bounds().origin();
  Window::ConvertPointToWindow(parent_, GetRootWindow(), &origin);
  return gfx::Rect(origin, bounds().size());
}

void Window::SetTransform(const ui::Transform& transform) {
  RootWindow* root_window = GetRootWindow();
  bool contained_mouse = IsVisible() && root_window &&
      ContainsPointInRoot(root_window->last_mouse_location());
  layer()->SetTransform(transform);
  if (root_window)
    root_window->OnWindowTransformed(this, contained_mouse);
}

void Window::SetLayoutManager(LayoutManager* layout_manager) {
  if (layout_manager == layout_manager_.get())
    return;
  layout_manager_.reset(layout_manager);
  if (!layout_manager)
    return;
  // If we're changing to a new layout manager, ensure it is aware of all the
  // existing child windows.
  for (Windows::const_iterator it = children_.begin();
       it != children_.end();
       ++it)
    layout_manager_->OnWindowAddedToLayout(*it);
}

void Window::SetBounds(const gfx::Rect& new_bounds) {
  if (parent_ && parent_->layout_manager())
    parent_->layout_manager()->SetChildBounds(this, new_bounds);
  else
    SetBoundsInternal(new_bounds);
}

gfx::Rect Window::GetTargetBounds() const {
  return layer_->GetTargetBounds();
}

const gfx::Rect& Window::bounds() const {
  return layer_->bounds();
}

void Window::SchedulePaintInRect(const gfx::Rect& rect) {
  if (layer_->SchedulePaint(rect)) {
    FOR_EACH_OBSERVER(
        WindowObserver, observers_, OnWindowPaintScheduled(this, rect));
  }
}

void Window::SetExternalTexture(ui::Texture* texture) {
  layer_->SetExternalTexture(texture);
  gfx::Rect region(bounds().size());
  FOR_EACH_OBSERVER(
      WindowObserver, observers_, OnWindowPaintScheduled(this, region));
}

void Window::SetParent(Window* parent) {
  GetParentForWindow(this, parent)->AddChild(this);
}

void Window::StackChildAtTop(Window* child) {
  if (children_.size() <= 1 || child == children_.back())
    return;  // In the front already.
  StackChildAbove(child, children_.back());
}

void Window::StackChildAbove(Window* child, Window* target) {
  StackChildRelativeTo(child, target, STACK_ABOVE);
}

void Window::StackChildBelow(Window* child, Window* target) {
  StackChildRelativeTo(child, target, STACK_BELOW);
}

void Window::AddChild(Window* child) {
  DCHECK(std::find(children_.begin(), children_.end(), child) ==
      children_.end());
  if (child->parent())
    child->parent()->RemoveChild(child);
  child->parent_ = this;

  layer_->Add(child->layer_);

  children_.push_back(child);
  if (layout_manager_.get())
    layout_manager_->OnWindowAddedToLayout(child);
  FOR_EACH_OBSERVER(WindowObserver, observers_, OnWindowAdded(child));
  child->OnParentChanged();

  RootWindow* root_window = child->GetRootWindow();
  if (root_window) {
    root_window->OnWindowAddedToRootWindow(child);
    NotifyAddedToRootWindow();
  }
}

void Window::AddTransientChild(Window* child) {
  if (child->transient_parent_)
    child->transient_parent_->RemoveTransientChild(child);
  DCHECK(std::find(transient_children_.begin(), transient_children_.end(),
                   child) == transient_children_.end());
  transient_children_.push_back(child);
  child->transient_parent_ = this;
}

void Window::RemoveTransientChild(Window* child) {
  Windows::iterator i =
      std::find(transient_children_.begin(), transient_children_.end(), child);
  DCHECK(i != transient_children_.end());
  transient_children_.erase(i);
  if (child->transient_parent_ == this)
    child->transient_parent_ = NULL;
}

void Window::RemoveChild(Window* child) {
  Windows::iterator i = std::find(children_.begin(), children_.end(), child);
  DCHECK(i != children_.end());
  if (layout_manager_.get())
    layout_manager_->OnWillRemoveWindowFromLayout(child);
  FOR_EACH_OBSERVER(WindowObserver, observers_, OnWillRemoveWindow(child));
  RootWindow* root_window = child->GetRootWindow();
  if (root_window) {
    root_window->OnWindowRemovedFromRootWindow(child);
    child->NotifyRemovingFromRootWindow();
  }
  child->parent_ = NULL;
  // We should only remove the child's layer if the child still owns that layer.
  // Someone else may have acquired ownership of it via AcquireLayer() and may
  // expect the hierarchy to go unchanged as the Window is destroyed.
  if (child->layer_owner_.get())
    layer_->Remove(child->layer_);
  children_.erase(i);
  child->OnParentChanged();
  if (layout_manager_.get())
    layout_manager_->OnWindowRemovedFromLayout(child);
}

bool Window::Contains(const Window* other) const {
  for (const Window* parent = other; parent; parent = parent->parent_) {
    if (parent == this)
      return true;
  }
  return false;
}

Window* Window::GetChildById(int id) {
  return const_cast<Window*>(const_cast<const Window*>(this)->GetChildById(id));
}

const Window* Window::GetChildById(int id) const {
  Windows::const_iterator i;
  for (i = children_.begin(); i != children_.end(); ++i) {
    if ((*i)->id() == id)
      return *i;
    const Window* result = (*i)->GetChildById(id);
    if (result)
      return result;
  }
  return NULL;
}

// static
void Window::ConvertPointToWindow(const Window* source,
                                  const Window* target,
                                  gfx::Point* point) {
  if (!source)
    return;
  ui::Layer::ConvertPointToLayer(source->layer(), target->layer(), point);
}

gfx::NativeCursor Window::GetCursor(const gfx::Point& point) const {
  return delegate_ ? delegate_->GetCursor(point) : gfx::kNullCursor;
}

void Window::SetEventFilter(EventFilter* event_filter) {
  event_filter_.reset(event_filter);
}

void Window::AddObserver(WindowObserver* observer) {
  observers_.AddObserver(observer);
}

void Window::RemoveObserver(WindowObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool Window::ContainsPointInRoot(const gfx::Point& point_in_root) {
  Window* root_window = GetRootWindow();
  if (!root_window)
    return false;
  gfx::Point local_point(point_in_root);
  ConvertPointToWindow(root_window, this, &local_point);
  return GetTargetBounds().Contains(local_point);
}

bool Window::ContainsPoint(const gfx::Point& local_point) {
  gfx::Rect local_bounds(gfx::Point(), bounds().size());
  return local_bounds.Contains(local_point);
}

bool Window::HitTest(const gfx::Point& local_point) {
  // Expand my bounds for hit testing (override is usually zero but it's
  // probably cheaper to do the math every time than to branch).
  gfx::Rect local_bounds(gfx::Point(), bounds().size());
  local_bounds.Inset(hit_test_bounds_override_outer_);
  // TODO(beng): hittest masks.
  return local_bounds.Contains(local_point);
}

Window* Window::GetEventHandlerForPoint(const gfx::Point& local_point) {
  return GetWindowForPoint(local_point, true, true);
}

Window* Window::GetTopWindowContainingPoint(const gfx::Point& local_point) {
  return GetWindowForPoint(local_point, false, false);
}

Window* Window::GetToplevelWindow() {
  Window* topmost_window_with_delegate = NULL;
  for (aura::Window* window = this; window != NULL; window = window->parent()) {
    if (window->delegate())
      topmost_window_with_delegate = window;
  }
  return topmost_window_with_delegate;
}

void Window::Focus() {
  DCHECK(GetFocusManager());
  GetFocusManager()->SetFocusedWindow(this, NULL);
}

void Window::Blur() {
  DCHECK(GetFocusManager());
  GetFocusManager()->SetFocusedWindow(NULL, NULL);
}

bool Window::HasFocus() const {
  const internal::FocusManager* focus_manager = GetFocusManager();
  return focus_manager ? focus_manager->IsFocusedWindow(this) : false;
}

// For a given window, we determine its focusability and ability to
// receive events by inspecting each sibling after it (i.e. drawn in
// front of it in the z-order) to see if it stops propagation of
// events that would otherwise be targeted at windows behind it.  We
// then perform this same check on every window up to the root.
bool Window::CanFocus() const {
  if (!IsVisible() || !parent_ || (delegate_ && !delegate_->CanFocus()))
    return false;

  // The client may forbid certain windows from receiving focus at a given point
  // in time.
  client::EventClient* client = client::GetEventClient(GetRootWindow());
  if (client && !client->CanProcessEventsWithinSubtree(this))
    return false;

  return parent_->CanFocus();
}

bool Window::CanReceiveEvents() const {
  // The client may forbid certain windows from receiving events at a given
  // point in time.
  client::EventClient* client = client::GetEventClient(GetRootWindow());
  if (client && !client->CanProcessEventsWithinSubtree(this))
    return false;

  return parent_ && IsVisible() && parent_->CanReceiveEvents();
}

internal::FocusManager* Window::GetFocusManager() {
  return const_cast<internal::FocusManager*>(
      static_cast<const Window*>(this)->GetFocusManager());
}

const internal::FocusManager* Window::GetFocusManager() const {
  return parent_ ? parent_->GetFocusManager() : NULL;
}

void Window::SetCapture() {
  if (!IsVisible())
    return;

  RootWindow* root_window = GetRootWindow();
  if (!root_window)
    return;

  root_window->SetCapture(this);
}

void Window::ReleaseCapture() {
  RootWindow* root_window = GetRootWindow();
  if (!root_window)
    return;

  root_window->ReleaseCapture(this);
}

bool Window::HasCapture() {
  RootWindow* root_window = GetRootWindow();
  return root_window && root_window->capture_window() == this;
}

void Window::SuppressPaint() {
  layer_->SuppressPaint();
}

// {Set,Get,Clear}Property are implemented in window_property.h.

void Window::SetNativeWindowProperty(const char* key, void* value) {
  SetPropertyInternal(
      key, key, NULL, reinterpret_cast<intptr_t>(value), 0);
}

void* Window::GetNativeWindowProperty(const char* key) const {
  return reinterpret_cast<void*>(GetPropertyInternal(key, 0));
}

///////////////////////////////////////////////////////////////////////////////
// Window, private:

intptr_t Window::SetPropertyInternal(const void* key,
                                     const char* name,
                                     PropertyDeallocator deallocator,
                                     intptr_t value,
                                     intptr_t default_value) {
  intptr_t old = GetPropertyInternal(key, default_value);
  if (value == default_value) {
    prop_map_.erase(key);
  } else {
    Value prop_value;
    prop_value.name = name;
    prop_value.value = value;
    prop_value.deallocator = deallocator;
    prop_map_[key] = prop_value;
  }
  FOR_EACH_OBSERVER(WindowObserver, observers_,
                    OnWindowPropertyChanged(this, key, old));
  return old;
}

intptr_t Window::GetPropertyInternal(const void* key,
                                     intptr_t default_value) const {
  std::map<const void*, Value>::const_iterator iter = prop_map_.find(key);
  if (iter == prop_map_.end())
    return default_value;
  return iter->second.value;
}

void Window::SetBoundsInternal(const gfx::Rect& new_bounds) {
  gfx::Rect actual_new_bounds(new_bounds);

  // Ensure we don't go smaller than our minimum bounds.
  if (delegate_) {
    const gfx::Size& min_size = delegate_->GetMinimumSize();
    actual_new_bounds.set_width(
        std::max(min_size.width(), actual_new_bounds.width()));
    actual_new_bounds.set_height(
        std::max(min_size.height(), actual_new_bounds.height()));
  }
  RootWindow* root_window = GetRootWindow();

  bool contained_mouse =
      IsVisible() &&
      root_window && ContainsPointInRoot(root_window->last_mouse_location());

  const gfx::Rect old_bounds = GetTargetBounds();

  // Always need to set the layer's bounds -- even if it is to the same thing.
  // This may cause important side effects such as stopping animation.
  layer_->SetBounds(actual_new_bounds);

  // If we're not changing the effective bounds, then we can bail early and skip
  // notifying our listeners.
  if (old_bounds == actual_new_bounds)
    return;

  if (layout_manager_.get())
    layout_manager_->OnWindowResized();
  if (delegate_)
    delegate_->OnBoundsChanged(old_bounds, actual_new_bounds);
  FOR_EACH_OBSERVER(WindowObserver,
                    observers_,
                    OnWindowBoundsChanged(this, old_bounds, actual_new_bounds));

  if (root_window)
    root_window->OnWindowBoundsChanged(this, contained_mouse);
}

void Window::SetVisible(bool visible) {
  if (visible == layer_->visible())
    return;  // No change.

  if (visible != layer_->visible()) {
    RootWindow* root_window = GetRootWindow();
    if (client::GetVisibilityClient(root_window)) {
      client::GetVisibilityClient(root_window)->UpdateLayerVisibility(
          this, visible);
    } else {
      layer_->SetVisible(visible);
    }
  }
  visible_ = visible;
  SchedulePaint();
  if (delegate_)
    delegate_->OnWindowVisibilityChanged(visible);

  if (parent_ && parent_->layout_manager_.get())
    parent_->layout_manager_->OnChildWindowVisibilityChanged(this, visible);
  FOR_EACH_OBSERVER(WindowObserver, observers_,
                    OnWindowVisibilityChanged(this, visible));

  RootWindow* root_window = GetRootWindow();
  if (root_window)
    root_window->OnWindowVisibilityChanged(this, visible);
}

void Window::SchedulePaint() {
  SchedulePaintInRect(gfx::Rect(0, 0, bounds().width(), bounds().height()));
}

Window* Window::GetWindowForPoint(const gfx::Point& local_point,
                                  bool return_tightest,
                                  bool for_event_handling) {
  if (!IsVisible())
    return NULL;

  if ((for_event_handling && !HitTest(local_point)) ||
      (!for_event_handling && !ContainsPoint(local_point)))
    return NULL;

  // Check if I should claim this event and not pass it to my children because
  // the location is inside my hit test override area.  For details, see
  // set_hit_test_bounds_override_inner().
  if (for_event_handling && !hit_test_bounds_override_inner_.empty()) {
    gfx::Rect inset_local_bounds(gfx::Point(), bounds().size());
    inset_local_bounds.Inset(hit_test_bounds_override_inner_);
    // We know we're inside the normal local bounds, so if we're outside the
    // inset bounds we must be in the special hit test override area.
    DCHECK(HitTest(local_point));
    if (!inset_local_bounds.Contains(local_point))
      return delegate_ ? this : NULL;
  }

  if (!return_tightest && delegate_)
    return this;

  for (Windows::const_reverse_iterator it = children_.rbegin(),
           rend = children_.rend();
       it != rend; ++it) {
    Window* child = *it;

    if (for_event_handling) {
      // The client may not allow events to be processed by certain subtrees.
      client::EventClient* client = client::GetEventClient(GetRootWindow());
      if (client && !client->CanProcessEventsWithinSubtree(child))
        continue;
    }

    // We don't process events for invisible windows or those that have asked
    // to ignore events.
    if (!child->IsVisible() || (for_event_handling && child->ignore_events_))
      continue;

    gfx::Point point_in_child_coords(local_point);
    Window::ConvertPointToWindow(this, child, &point_in_child_coords);
    Window* match = child->GetWindowForPoint(point_in_child_coords,
                                             return_tightest,
                                             for_event_handling);
    if (match)
      return match;
  }

  return delegate_ ? this : NULL;
}

void Window::OnParentChanged() {
  FOR_EACH_OBSERVER(
      WindowObserver, observers_, OnWindowParentChanged(this, parent_));
}

void Window::StackChildRelativeTo(Window* child,
                                  Window* target,
                                  StackDirection direction) {
  DCHECK_NE(child, target);
  DCHECK(child);
  DCHECK(target);
  DCHECK_EQ(this, child->parent());
  DCHECK_EQ(this, target->parent());

  const size_t target_i =
      std::find(children_.begin(), children_.end(), target) - children_.begin();

  // By convention we don't stack on top of windows with layers with NULL
  // delegates.  Walk backward to find a valid target window.
  // See tests WindowTest.StackingMadrigal and StackOverClosingTransient
  // for an explanation of this.
  size_t final_target_i = target_i;
  while (final_target_i > 0 &&
         children_[final_target_i]->layer()->delegate() == NULL)
    --final_target_i;
  Window* final_target = children_[final_target_i];

  // If we couldn't find a valid target position, don't move anything.
  if (final_target->layer()->delegate() == NULL)
    return;

  // Don't try to stack a child above itself.
  if (child == final_target)
    return;

  // Move the child and all its transients.
  StackChildRelativeToImpl(child, final_target, direction);
}

void Window::StackChildRelativeToImpl(Window* child,
                                      Window* target,
                                      StackDirection direction) {
  DCHECK_NE(child, target);
  DCHECK(child);
  DCHECK(target);
  DCHECK_EQ(this, child->parent());
  DCHECK_EQ(this, target->parent());

  const size_t child_i =
      std::find(children_.begin(), children_.end(), child) - children_.begin();
  const size_t target_i =
      std::find(children_.begin(), children_.end(), target) - children_.begin();

  // Don't move the child if it is already in the right place.
  if ((direction == STACK_ABOVE && child_i == target_i + 1) ||
      (direction == STACK_BELOW && child_i + 1 == target_i))
    return;

  const size_t dest_i =
      direction == STACK_ABOVE ?
      (child_i < target_i ? target_i : target_i + 1) :
      (child_i < target_i ? target_i - 1 : target_i);
  children_.erase(children_.begin() + child_i);
  children_.insert(children_.begin() + dest_i, child);

  if (direction == STACK_ABOVE)
    layer()->StackAbove(child->layer(), target->layer());
  else
    layer()->StackBelow(child->layer(), target->layer());

  // Stack any transient children that share the same parent to be in front of
  // 'child'.
  Window* last_transient = child;
  for (Windows::iterator it = child->transient_children_.begin();
       it != child->transient_children_.end(); ++it) {
    Window* transient_child = *it;
    if (transient_child->parent_ == this) {
      StackChildRelativeToImpl(transient_child, last_transient, STACK_ABOVE);
      last_transient = transient_child;
    }
  }

  child->OnStackingChanged();
}

void Window::OnStackingChanged() {
  FOR_EACH_OBSERVER(WindowObserver, observers_, OnWindowStackingChanged(this));
}

void Window::NotifyRemovingFromRootWindow() {
  FOR_EACH_OBSERVER(WindowObserver, observers_,
                    OnWindowRemovingFromRootWindow(this));
  for (Window::Windows::const_iterator it = children_.begin();
       it != children_.end(); ++it) {
    (*it)->NotifyRemovingFromRootWindow();
  }
}

void Window::NotifyAddedToRootWindow() {
  FOR_EACH_OBSERVER(WindowObserver, observers_,
                    OnWindowAddedToRootWindow(this));
  for (Window::Windows::const_iterator it = children_.begin();
       it != children_.end(); ++it) {
    (*it)->NotifyAddedToRootWindow();
  }
}

void Window::OnPaintLayer(gfx::Canvas* canvas) {
  if (delegate_)
    delegate_->OnPaint(canvas);
}

void Window::OnDeviceScaleFactorChanged(float device_scale_factor) {
  if (delegate_)
    delegate_->OnDeviceScaleFactorChanged(device_scale_factor);
}

void Window::UpdateLayerName(const std::string& name) {
#if !defined(NDEBUG)
  DCHECK(layer());

  std::string layer_name(name_);
  if (layer_name.empty())
    layer_name.append("Unnamed Window");

  if (id_ != -1) {
    char id_buf[10];
    base::snprintf(id_buf, sizeof(id_buf), " %d", id_);
    layer_name.append(id_buf);
  }
  layer()->set_name(layer_name);
#endif
}

#ifndef NDEBUG
std::string Window::GetDebugInfo() const {
  return StringPrintf(
      "%s<%d> bounds(%d, %d, %d, %d) %s",
      name().empty() ? "Unknown" : name().c_str(), id(),
      bounds().x(), bounds().y(), bounds().width(), bounds().height(),
      IsVisible() ? "Visible" : "Hidden");
}

void Window::PrintWindowHierarchy(int depth) const {
  printf("%*s%s\n", depth * 2, "", GetDebugInfo().c_str());
  for (Windows::const_reverse_iterator it = children_.rbegin(),
           rend = children_.rend();
       it != rend; ++it) {
    Window* child = *it;
    child->PrintWindowHierarchy(depth + 1);
  }
}
#endif

}  // namespace aura
