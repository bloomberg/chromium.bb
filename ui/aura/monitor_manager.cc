// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/monitor_manager.h"

namespace aura {

MonitorManager::MonitorManager() {
}

MonitorManager::~MonitorManager() {
}

void MonitorManager::AddObserver(MonitorObserver* observer) {
  observers_.AddObserver(observer);
}

void MonitorManager::RemoveObserver(MonitorObserver* observer) {
  observers_.RemoveObserver(observer);
}

RootWindow* MonitorManager::CreateRootWindowForPrimaryMonitor() {
  return CreateRootWindowForMonitor(GetPrimaryMonitor());
}

void MonitorManager::NotifyBoundsChanged(const Monitor* monitor) {
  FOR_EACH_OBSERVER(MonitorObserver, observers_,
                    OnMonitorBoundsChanged(monitor));
}

}  // namespace aura
