// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/test/test_window_manager.h"

#include "base/files/file_util.h"
#include "base/location.h"

namespace ui {

TestWindowManager::TestWindowManager(const base::FilePath& dump_location)
    : location_(dump_location) {
}

TestWindowManager::~TestWindowManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void TestWindowManager::Initialize() {
  if (location_.empty())
    return;
  if (!DirectoryExists(location_) && !base::CreateDirectory(location_) &&
      location_ != base::FilePath("/dev/null"))
    PLOG(FATAL) << "unable to create output directory";
  if (!base::PathIsWritable(location_))
    PLOG(FATAL) << "unable to write to output location";
}

int32_t TestWindowManager::AddWindow(TestWindow* window) {
  return windows_.Add(window);
}

void TestWindowManager::RemoveWindow(int32_t window_id, TestWindow* window) {
  DCHECK_EQ(window, windows_.Lookup(window_id));
  windows_.Remove(window_id);
}

TestWindow* TestWindowManager::GetWindow(int32_t window_id) {
  return windows_.Lookup(window_id);
}

base::FilePath TestWindowManager::base_path() const {
  return location_;
}

}  // namespace ui
