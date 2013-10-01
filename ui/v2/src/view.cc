// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/v2/public/view.h"

#include <algorithm>

#include "ui/v2/public/view_observer.h"

namespace v2 {

// Friend of View. Provides a way to get at a View's observer list for
// notification methods.
class ViewObserversAccessor {
 public:
  explicit ViewObserversAccessor(View* view) : view_(view) {}
  ~ViewObserversAccessor() {}

  ObserverList<ViewObserver>* get() { return &view_->observers_; }

 private:
  View* view_;

  DISALLOW_COPY_AND_ASSIGN(ViewObserversAccessor);
};

enum StackDirection {
  STACK_ABOVE,
  STACK_BELOW
};

void StackChildRelativeTo(View* parent,
                          std::vector<View*>* children,
                          View* child,
                          View* other,
                          StackDirection direction) {
  DCHECK_NE(child, other);
  DCHECK(child);
  DCHECK(other);
  DCHECK_EQ(parent, child->parent());
  DCHECK_EQ(parent, other->parent());

  // Notify stacking changing.

  // TODO(beng): This is simpler than aura::Window's for now.
  const size_t child_i =
      std::find(children->begin(), children->end(), child) - children->begin();
  const size_t other_i =
      std::find(children->begin(), children->end(), other) - children->begin();
  const size_t destination_i =
      direction == STACK_ABOVE ?
      (child_i < other_i ? other_i : other_i + 1) :
      (child_i < other_i ? other_i - 1 : other_i);
  children->erase(children->begin() + child_i);
  children->insert(children->begin() + destination_i, child);

  // TODO(beng): restack layers.

  // Notify stacking changed.
}

void NotifyViewTreeChangeAtReceiver(
    View* receiver,
    const ViewObserver::TreeChangeParams& params) {
  ViewObserver::TreeChangeParams local_params = params;
  local_params.receiver = receiver;
  FOR_EACH_OBSERVER(ViewObserver,
                    *ViewObserversAccessor(receiver).get(),
                    OnViewTreeChange(local_params));
}

void NotifyViewTreeChangeUp(View* start_at,
                            const ViewObserver::TreeChangeParams& params) {
  for (View* current = start_at; current; current = current->parent())
    NotifyViewTreeChangeAtReceiver(current, params);
}

void NotifyViewTreeChangeDown(View* start_at,
                              const ViewObserver::TreeChangeParams& params) {
  NotifyViewTreeChangeAtReceiver(start_at, params);
  View::Children::const_iterator it = start_at->children().begin();
  for (; it != start_at->children().end(); ++it)
    NotifyViewTreeChangeDown(*it, params);
}

void NotifyViewTreeChange(const ViewObserver::TreeChangeParams& params) {
  NotifyViewTreeChangeDown(params.target, params);
  switch (params.phase) {
  case ViewObserver::DISPOSITION_CHANGING:
    if (params.old_parent)
      NotifyViewTreeChangeUp(params.old_parent, params);
    break;
  case ViewObserver::DISPOSITION_CHANGED:
    if (params.new_parent)
      NotifyViewTreeChangeUp(params.new_parent, params);
    break;
  default:
    NOTREACHED();
    break;
  }
}

class ScopedTreeNotifier {
 public:
  ScopedTreeNotifier(View* target, View* old_parent, View* new_parent) {
    params_.target = target;
    params_.old_parent = old_parent;
    params_.new_parent = new_parent;
    NotifyViewTreeChange(params_);
  }
  ~ScopedTreeNotifier() {
    params_.phase = ViewObserver::DISPOSITION_CHANGED;
    NotifyViewTreeChange(params_);
  }

 private:
  ViewObserver::TreeChangeParams params_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTreeNotifier);
};

////////////////////////////////////////////////////////////////////////////////
// View, public:

// Creation, configuration -----------------------------------------------------

View::View() : visible_(false), owned_by_parent_(true), parent_(NULL) {
}

View::~View() {
  FOR_EACH_OBSERVER(ViewObserver, observers_, OnViewDestroying());

  while (!children_.empty()) {
    View* child = children_.front();
    if (child->owned_by_parent_) {
      delete child;
      // Deleting the child also removes it from our child list.
      DCHECK(std::find(children_.begin(), children_.end(), child) ==
             children_.end());
    } else {
      RemoveChild(child);
    }
  }

  if (parent_)
    parent_->RemoveChild(this);

  FOR_EACH_OBSERVER(ViewObserver, observers_, OnViewDestroyed());
}

void View::AddObserver(ViewObserver* observer) {
  observers_.AddObserver(observer);
}

void View::RemoveObserver(ViewObserver* observer) {
  observers_.RemoveObserver(observer);
}

void View::SetPainter(Painter* painter) {
  painter_.reset(painter);
}

void View::SetLayout(Layout* layout) {
  layout_.reset(layout);
}

// Disposition -----------------------------------------------------------------

void View::SetBounds(const gfx::Rect& bounds) {
  if (parent_ && parent_->layout_.get())
    parent_->layout_->SetChildBounds(this, bounds);
  else
    SetBoundsInternal(bounds);
}

void View::SetVisible(bool visible) {}

// Tree ------------------------------------------------------------------------

void View::AddChild(View* child) {
  ScopedTreeNotifier notifier(child, child->parent(), this);
  children_.push_back(child);
  child->parent_ = this;
}

void View::RemoveChild(View* child) {
  ScopedTreeNotifier(child, this, NULL);
  std::vector<View*>::iterator it =
      std::find(children_.begin(), children_.end(), child);
  if (it != children_.end()) {
    children_.erase(it);
    child->parent_ = NULL;
  }
}

bool View::Contains(View* child) const {
  for (View* p = child->parent(); p; p = p->parent()) {
    if (p == this)
      return true;
  }
  return false;
}

void View::StackChildAtTop(View* child) {
  if (children_.size() <= 1 || child == children_.back())
    return;  // On top already.
  StackChildAbove(child, children_.back());
}

void View::StackChildAtBottom(View* child) {
  if (children_.size() <= 1 || child == children_.front())
    return;  // On bottom already.
  StackChildBelow(child, children_.front());
}

void View::StackChildAbove(View* child, View* other) {
  StackChildRelativeTo(this, &children_, child, other, STACK_ABOVE);
}

void View::StackChildBelow(View* child, View* other) {
  StackChildRelativeTo(this, &children_, child, other, STACK_BELOW);
}

////////////////////////////////////////////////////////////////////////////////
// View, private:

void View::SetBoundsInternal(const gfx::Rect& bounds) {
  bounds_ = bounds;

  // TODO(beng): Update layer.
  // TODO(beng): Notify changed.
}

}  // namespace v2
