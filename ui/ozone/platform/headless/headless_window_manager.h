// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_HEADLESS_HEADLESS_WINDOW_MANAGER_H_
#define UI_OZONE_PLATFORM_HEADLESS_HEADLESS_WINDOW_MANAGER_H_

#include <stdint.h>

#include <memory>

#include "base/containers/id_map.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

class HeadlessWindow;

class HeadlessWindowManager {
 public:
  HeadlessWindowManager();
  ~HeadlessWindowManager();

  // Register a new window. Returns the window id.
  int32_t AddWindow(HeadlessWindow* window);

  // Remove a window.
  void RemoveWindow(int32_t window_id, HeadlessWindow* window);

  // Find a window object by id;
  HeadlessWindow* GetWindow(int32_t window_id);

 private:
  base::IDMap<HeadlessWindow*> windows_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessWindowManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_HEADLESS_HEADLESS_WINDOW_MANAGER_H_
