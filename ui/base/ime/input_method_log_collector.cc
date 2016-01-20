// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_log_collector.h"

#include <string.h>

#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/macros.h"

namespace {

const size_t kMaxLogCount = 50;

}

namespace ui {

InputMethodLogCollector::InputMethodLogCollector() {}

InputMethodLogCollector::~InputMethodLogCollector() {}

void InputMethodLogCollector::AddString(const char* str_val) {
  if (logs_.size() >= kMaxLogCount)
    logs_.erase(logs_.begin());
  logs_.push_back(str_val);
}

void InputMethodLogCollector::AddBoolean(bool bool_val) {
  AddString(bool_val ? "true" : "false");
}

void InputMethodLogCollector::DumpLogs() {
  static int dump_times = 0;
  if (dump_times > 5) {
    ClearLogs();
    return;
  }
  const char* logs_copy[kMaxLogCount];
  size_t log_count = logs_.size();
  for (size_t i = 0; i < log_count && i < arraysize(logs_copy); ++i)
    logs_copy[i] = logs_[i];
  base::debug::Alias(&log_count);
  base::debug::Alias(&logs_copy);
  base::debug::DumpWithoutCrashing();
  dump_times++;
  ClearLogs();
}

void InputMethodLogCollector::ClearLogs() {
  logs_.clear();
}

}  // namespace ui
