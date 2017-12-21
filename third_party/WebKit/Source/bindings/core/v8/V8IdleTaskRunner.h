/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef V8IdleTaskRunner_h
#define V8IdleTaskRunner_h

#include <memory>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "core/CoreExport.h"
#include "gin/public/v8_idle_task_runner.h"
#include "platform/runtime_enabled_features.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"

namespace blink {

class V8IdleTaskRunner : public gin::V8IdleTaskRunner {
  USING_FAST_MALLOC(V8IdleTaskRunner);
  WTF_MAKE_NONCOPYABLE(V8IdleTaskRunner);

 public:
  V8IdleTaskRunner(WebScheduler* scheduler) : scheduler_(scheduler) {}
  ~V8IdleTaskRunner() override = default;
  void PostIdleTask(v8::IdleTask* task) override {
    DCHECK(RuntimeEnabledFeatures::V8IdleTasksEnabled());
    scheduler_->PostIdleTask(
        FROM_HERE, WTF::Bind(&v8::IdleTask::Run, base::WrapUnique(task)));
  }

 private:
  WebScheduler* scheduler_;
};

}  // namespace blink

#endif  // V8Initializer_h
