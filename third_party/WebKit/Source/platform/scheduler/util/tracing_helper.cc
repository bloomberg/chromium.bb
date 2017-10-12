// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/util/tracing_helper.h"

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"

namespace blink {
namespace scheduler {

const char kTracingCategoryNameDefault[] = "renderer.scheduler";
const char kTracingCategoryNameInfo[] =
    TRACE_DISABLED_BY_DEFAULT("renderer.scheduler");
const char kTracingCategoryNameDebug[] =
    TRACE_DISABLED_BY_DEFAULT("renderer.scheduler.debug");

namespace {

// No trace events should be created with this category.
const char kTracingCategoryNameVerboseSnapshots[] =
    TRACE_DISABLED_BY_DEFAULT("renderer.scheduler.enable_verbose_snapshots");

}  // namespace

bool AreVerboseSnapshotsEnabled() {
  bool result = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(kTracingCategoryNameVerboseSnapshots,
                                     &result);
  return result;
}

void WarmupTracingCategories() {
  TRACE_EVENT_WARMUP_CATEGORY(kTracingCategoryNameDefault);
  TRACE_EVENT_WARMUP_CATEGORY(kTracingCategoryNameInfo);
  TRACE_EVENT_WARMUP_CATEGORY(kTracingCategoryNameDebug);
  TRACE_EVENT_WARMUP_CATEGORY(kTracingCategoryNameVerboseSnapshots);
}

std::string PointerToString(const void* pointer) {
  return base::StringPrintf(
      "0x%" PRIx64,
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointer)));
}

}  // namespace scheduler
}  // namespace blink
