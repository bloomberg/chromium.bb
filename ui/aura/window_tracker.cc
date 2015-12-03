// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_tracker.h"

#include "ui/aura/window.h"

namespace aura {

WindowTracker::WindowTracker() {
}

WindowTracker::WindowTracker(const WindowList& windows) {
  // |windows| may contain dups, so call Add() instead of insert().
  for (auto iter = windows.begin(); iter != windows.end(); iter++)
    Add(*iter);
}

WindowTracker::~WindowTracker() {
  while (has_windows())
    Pop()->RemoveObserver(this);
}

void WindowTracker::Add(Window* window) {
  if (Contains(window))
    return;

  window->AddObserver(this);
  windows_.push_back(window);
}

void WindowTracker::Remove(Window* window) {
  auto iter = std::find(windows_.begin(), windows_.end(), window);
  if (iter != windows_.end()) {
    windows_.erase(iter);
    window->RemoveObserver(this);
  }
}

bool WindowTracker::Contains(Window* window) {
  return std::find(windows_.begin(), windows_.end(), window) != windows_.end();
}

aura::Window* WindowTracker::Pop() {
  DCHECK(!windows_.empty());
  aura::Window* window = *windows_.begin();
  Remove(window);
  return window;
}

void WindowTracker::OnWindowDestroying(Window* window) {
  DCHECK(Contains(window));
  Remove(window);
}

}  // namespace aura
