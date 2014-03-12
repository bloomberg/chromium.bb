// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/transient_window_manager.h"

#include <algorithm>
#include <functional>

#include "base/auto_reset.h"
#include "base/stl_util.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/wm/core/transient_window_observer.h"
#include "ui/wm/core/transient_window_stacking_client.h"
#include "ui/wm/core/window_util.h"

using aura::Window;

namespace views {
namespace corewm {

DEFINE_OWNED_WINDOW_PROPERTY_KEY(TransientWindowManager, kPropertyKey, NULL);

TransientWindowManager::~TransientWindowManager() {
}

// static
TransientWindowManager* TransientWindowManager::Get(Window* window) {
  TransientWindowManager* manager = window->GetProperty(kPropertyKey);
  if (!manager) {
    manager = new TransientWindowManager(window);
    window->SetProperty(kPropertyKey, manager);
  }
  return manager;
}

// static
const TransientWindowManager* TransientWindowManager::Get(
    const Window* window) {
  return window->GetProperty(kPropertyKey);
}

void TransientWindowManager::AddObserver(TransientWindowObserver* observer) {
  observers_.AddObserver(observer);
}

void TransientWindowManager::RemoveObserver(TransientWindowObserver* observer) {
  observers_.RemoveObserver(observer);
}

void TransientWindowManager::AddTransientChild(Window* child) {
  // TransientWindowStackingClient does the stacking of transient windows. If it
  // isn't installed stacking is going to be wrong.
  DCHECK(TransientWindowStackingClient::instance_);

  TransientWindowManager* child_manager = Get(child);
  if (child_manager->transient_parent_)
    Get(child_manager->transient_parent_)->RemoveTransientChild(child);
  DCHECK(std::find(transient_children_.begin(), transient_children_.end(),
                   child) == transient_children_.end());
  transient_children_.push_back(child);
  child_manager->transient_parent_ = window_;
  FOR_EACH_OBSERVER(TransientWindowObserver, observers_,
                    OnTransientChildAdded(window_, child));
}

void TransientWindowManager::RemoveTransientChild(Window* child) {
  Windows::iterator i =
      std::find(transient_children_.begin(), transient_children_.end(), child);
  DCHECK(i != transient_children_.end());
  transient_children_.erase(i);
  TransientWindowManager* child_manager = Get(child);
  DCHECK_EQ(window_, child_manager->transient_parent_);
  child_manager->transient_parent_ = NULL;
  FOR_EACH_OBSERVER(TransientWindowObserver, observers_,
                    OnTransientChildRemoved(window_, child));
}

bool TransientWindowManager::IsStackingTransient(
    const aura::Window* child,
    const aura::Window* target) const {
  return stacking_pair_ && stacking_pair_->child == child &&
      stacking_pair_->target == target;
}

TransientWindowManager::TransientWindowManager(Window* window)
    : window_(window),
      transient_parent_(NULL),
      stacking_pair_(NULL) {
  window_->AddObserver(this);
}

void TransientWindowManager::OnChildStackingChanged(aura::Window* child) {
  // Do nothing if we initiated the stacking change.
  // TODO(sky): change args to OnWindowStackingChanged() so that this lookup
  // can be simpler.
  if (stacking_pair_ && stacking_pair_->child == child) {
    Windows::const_iterator child_i = std::find(
        window_->children().begin(), window_->children().end(), child);
    DCHECK(child_i != window_->children().end());
    if (child_i != window_->children().begin() &&
        (*(child_i - 1) == stacking_pair_->target))
      return;
  }

  // Stack any transient children that share the same parent to be in front of
  // |child|. The existing stacking order is preserved by iterating backwards
  // and always stacking on top.
  Window::Windows children(window_->children());
  for (Window::Windows::reverse_iterator it = children.rbegin();
       it != children.rend(); ++it) {
    if ((*it) != child && HasTransientAncestor(*it, child)) {
      StackingPair pair(*it, child);
      base::AutoReset<StackingPair*> resetter(&stacking_pair_, &pair);
      window_->StackChildAbove((*it), child);
    }
  }
}

void TransientWindowManager::OnWindowVisibilityChanging(Window* window,
                                                        bool visible) {
  // TODO(sky): move handling of becoming visible here.
  if (!visible) {
    std::for_each(transient_children_.begin(), transient_children_.end(),
                  std::mem_fun(&Window::Hide));
  }
}

void TransientWindowManager::OnWindowStackingChanged(Window* window) {
  TransientWindowManager* parent_manager = Get(window->parent());
  parent_manager->OnChildStackingChanged(window);
}

void TransientWindowManager::OnWindowDestroying(Window* window) {
  // TODO(sky): remove notes after safely landing and baking.

  // Removes ourselves from our transient parent (if it hasn't been done by the
  // RootWindow).
  // NOTE: This use to be done after children where removed, now it is before.
  if (transient_parent_) {
    TransientWindowManager::Get(transient_parent_)->RemoveTransientChild(
        window_);
  }

  // Destroy transient children, only after we've removed ourselves from our
  // parent, as destroying an active transient child may otherwise attempt to
  // refocus us.
  // NOTE: this use to be after removed from parent, now its before.
  Windows transient_children(transient_children_);
  STLDeleteElements(&transient_children);
  DCHECK(transient_children_.empty());
}

}  // namespace corewm
}  // namespace views
