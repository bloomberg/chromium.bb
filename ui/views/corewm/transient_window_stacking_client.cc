// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/transient_window_stacking_client.h"

#include <algorithm>

using aura::Window;

namespace views {
namespace corewm {

namespace {

// Populates |ancestors| with all transient ancestors of |window| that are
// siblings of |window|. Returns true if any ancestors were found, false if not.
bool GetAllTransientAncestors(Window* window, Window::Windows* ancestors) {
  Window* parent = window->parent();
  for (; window; window = window->transient_parent()) {
    if (window->parent() == parent)
      ancestors->push_back(window);
  }
  return (!ancestors->empty());
}

// Replaces |window1| and |window2| with their possible transient ancestors that
// are still siblings (have a common transient parent).  |window1| and |window2|
// are not modified if such ancestors cannot be found.
void FindCommonTransientAncestor(Window** window1, Window** window2) {
  DCHECK(window1);
  DCHECK(window2);
  DCHECK(*window1);
  DCHECK(*window2);
  // Assemble chains of ancestors of both windows.
  Window::Windows ancestors1;
  Window::Windows ancestors2;
  if (!GetAllTransientAncestors(*window1, &ancestors1) ||
      !GetAllTransientAncestors(*window2, &ancestors2)) {
    return;
  }
  // Walk the two chains backwards and look for the first difference.
  Window::Windows::const_reverse_iterator it1 = ancestors1.rbegin();
  Window::Windows::const_reverse_iterator it2 = ancestors2.rbegin();
  for (; it1  != ancestors1.rend() && it2  != ancestors2.rend(); ++it1, ++it2) {
    if (*it1 != *it2) {
      *window1 = *it1;
      *window2 = *it2;
      break;
    }
  }
}

// Returns true if |window| has |ancestor| as a transient ancestor. A transient
// ancestor is found by following the transient parent chain of the window.
bool HasTransientAncestor(const Window* window, const Window* ancestor) {
  if (window->transient_parent() == ancestor)
    return true;
  return window->transient_parent() ?
      HasTransientAncestor(window->transient_parent(), ancestor) : false;
}

}  // namespace

TransientWindowStackingClient::TransientWindowStackingClient() {
}

TransientWindowStackingClient::~TransientWindowStackingClient() {
}

void TransientWindowStackingClient::AdjustStacking(
    Window** child,
    Window** target,
    Window::StackDirection* direction) {
  // For windows that have transient children stack the transient ancestors that
  // are siblings. This prevents one transient group from being inserted in the
  // middle of another.
  FindCommonTransientAncestor(child, target);

  // When stacking above skip to the topmost transient descendant of the target.
  if (*direction == Window::STACK_ABOVE &&
      !HasTransientAncestor(*child, *target)) {
    const Window::Windows& siblings((*child)->parent()->children());
    size_t target_i =
        std::find(siblings.begin(), siblings.end(), *target) - siblings.begin();
    while (target_i + 1 < siblings.size() &&
           HasTransientAncestor(siblings[target_i + 1], *target)) {
      ++target_i;
    }
    *target = siblings[target_i];
  }
}

}  // namespace corewm
}  // namespace views
