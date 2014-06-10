// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdarg.h>
#include <stdio.h>

#include "debug.h"

namespace relocation_packer {

Logger* Logger::instance_ = NULL;

Logger* Logger::GetInstance() {
  if (instance_ == NULL)
    instance_ = new Logger;
  return instance_;
}

void Logger::Log(const char* format, va_list args) {
  vfprintf(stdout, format, args);
}

void Logger::Log(const char* format, ...) {
  va_list args;
  va_start(args, format);
  GetInstance()->Log(format, args);
  va_end(args);
}

void Logger::VLog(const char* format, ...) {
  if (GetInstance()->is_verbose_) {
    va_list args;
    va_start(args, format);
    GetInstance()->Log(format, args);
    va_end(args);
  }
}

void Logger::SetVerbose(bool flag) {
  GetInstance()->is_verbose_ = flag;
}

}  // namespace relocation_packer
