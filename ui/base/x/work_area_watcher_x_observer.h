// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_WORK_AREA_WATCHER_X_OBSERVER_H_
#define UI_BASE_X_WORK_AREA_WATCHER_X_OBSERVER_H_
#pragma once

#include "ui/base/ui_export.h"

namespace ui {

class UI_EXPORT WorkAreaWatcherXObserver {
 public:
  virtual void WorkAreaChanged() = 0;

 protected:
  virtual ~WorkAreaWatcherXObserver() {}
};

}  // namespace ui

#endif  // UI_BASE_X_WORK_AREA_WATCHER_X_OBSERVER_H_
