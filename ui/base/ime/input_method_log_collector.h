// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_LOG_COLLECTOR_H_
#define UI_BASE_IME_INPUT_METHOD_LOG_COLLECTOR_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "ui/base/ime/ui_base_ime_export.h"

namespace ui {

// The class to gather some logs and dump it through
// base::debug::DumpWithoutCrashing when InputMethod processing a key event.
// This is for tracing the issue that hardly repros: crbug.com/569339.
class UI_BASE_IME_EXPORT InputMethodLogCollector {
 public:
  InputMethodLogCollector();
  ~InputMethodLogCollector();
  void AddString(const char* str_val);
  void AddBoolean(bool bool_val);
  void DumpLogs();
  void ClearLogs();

 private:
  std::vector<const char*> logs_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodLogCollector);
};

}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_LOG_COLLECTOR_H_
