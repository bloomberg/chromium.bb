// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/work_area_watcher_x.h"

#include "ui/base/x/root_window_property_watcher_x.h"
#include "ui/base/x/work_area_watcher_x_observer.h"
#include "ui/base/x/x11_util.h"

namespace ui {

static const char* const kNetWorkArea = "_NET_WORKAREA";

// static
WorkAreaWatcherX* WorkAreaWatcherX::GetInstance() {
  return Singleton<WorkAreaWatcherX>::get();
}

// static
void WorkAreaWatcherX::AddObserver(WorkAreaWatcherXObserver* observer) {
  // Ensure that RootWindowPropertyWatcherX exists.
  internal::RootWindowPropertyWatcherX::GetInstance();
  GetInstance()->observers_.AddObserver(observer);
}

// static
void WorkAreaWatcherX::RemoveObserver(WorkAreaWatcherXObserver* observer) {
  GetInstance()->observers_.RemoveObserver(observer);
}

// static
void WorkAreaWatcherX::Notify() {
  GetInstance()->NotifyWorkAreaChanged();
}

// static
Atom WorkAreaWatcherX::GetPropertyAtom() {
  return GetAtom(kNetWorkArea);
}

WorkAreaWatcherX::WorkAreaWatcherX() {
}

WorkAreaWatcherX::~WorkAreaWatcherX() {
}

void WorkAreaWatcherX::NotifyWorkAreaChanged() {
  FOR_EACH_OBSERVER(WorkAreaWatcherXObserver, observers_, WorkAreaChanged());
}

}  // namespace ui
