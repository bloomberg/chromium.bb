// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_TEST_TEST_WINDOW_MANAGER_H_
#define UI_OZONE_PLATFORM_TEST_TEST_WINDOW_MANAGER_H_

#include "base/files/file_path.h"
#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

class TestWindow;

class TestWindowManager {
 public:
  explicit TestWindowManager(const base::FilePath& dump_location);
  ~TestWindowManager();

  // Initialize (mainly check that we have a place to write output to).
  void Initialize();

  // Register a new window. Returns the window id.
  int32_t AddWindow(TestWindow* window);

  // Remove a window.
  void RemoveWindow(int32_t window_id, TestWindow* window);

  // Find a window object by id;
  TestWindow* GetWindow(int32_t window_id);

  // User-supplied path for images.
  base::FilePath base_path() const;

 private:
  base::FilePath location_;

  IDMap<TestWindow> windows_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_TEST_TEST_WINDOW_MANAGER_H_
