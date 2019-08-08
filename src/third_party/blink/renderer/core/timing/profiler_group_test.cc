// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/profiler_group.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/timing/profiler.h"
#include "third_party/blink/renderer/core/timing/profiler_init_options.h"

namespace blink {

// Tests that a leaked profiler doesn't crash the isolate on heap teardown.
TEST(ProfilerGroupTest, LeakProfiler) {
  V8TestingScope scope;

  ProfilerGroup* profiler_group = ProfilerGroup::From(scope.GetIsolate());

  ProfilerInitOptions* init_options = ProfilerInitOptions::Create();
  init_options->setSampleInterval(0);
  init_options->setMaxBufferSize(0);
  Profiler* profiler = profiler_group->CreateProfiler(
      scope.GetScriptState(), *init_options, base::TimeTicks(),
      scope.GetExceptionState());

  EXPECT_FALSE(profiler->stopped());
}

TEST(ProfilerGroupTest, StopProfiler) {
  V8TestingScope scope;

  ProfilerGroup* profiler_group = ProfilerGroup::From(scope.GetIsolate());

  ProfilerInitOptions* init_options = ProfilerInitOptions::Create();
  init_options->setSampleInterval(0);
  init_options->setMaxBufferSize(0);
  Profiler* profiler = profiler_group->CreateProfiler(
      scope.GetScriptState(), *init_options, base::TimeTicks(),
      scope.GetExceptionState());

  EXPECT_FALSE(profiler->stopped());
  profiler->stop(scope.GetScriptState());
  EXPECT_TRUE(profiler->stopped());
}

// Tests that attached profilers are stopped on ProfilerGroup deallocation.
TEST(ProfilerGroupTest, StopProfilerOnGroupDeallocate) {
  V8TestingScope scope;

  ProfilerGroup* profiler_group = ProfilerGroup::From(scope.GetIsolate());

  ProfilerInitOptions* init_options = ProfilerInitOptions::Create();
  init_options->setSampleInterval(0);
  init_options->setMaxBufferSize(0);
  Profiler* profiler = profiler_group->CreateProfiler(
      scope.GetScriptState(), *init_options, base::TimeTicks(),
      scope.GetExceptionState());

  EXPECT_FALSE(profiler->stopped());
  profiler_group->WillBeDestroyed();
  EXPECT_TRUE(profiler->stopped());
}

}  // namespace blink
