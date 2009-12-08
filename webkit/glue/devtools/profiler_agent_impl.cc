// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#undef LOG

#include "webkit/glue/devtools/profiler_agent_impl.h"

void ProfilerAgentImpl::GetActiveProfilerModules() {
  delegate_->DidGetActiveProfilerModules(
      v8::V8::GetActiveProfilerModules());
}

void ProfilerAgentImpl::GetLogLines(int position) {
  static char buffer[65536];
  const int read_size = v8::V8::GetLogLines(
      position, buffer, sizeof(buffer) - 1);
  buffer[read_size] = '\0';
  position += read_size;
  delegate_->DidGetLogLines(position, buffer);
}
