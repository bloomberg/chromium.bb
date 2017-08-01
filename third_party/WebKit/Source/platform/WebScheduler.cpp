// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/web_scheduler.h"

#include "platform/WebFrameScheduler.h"
#include "platform/wtf/Assertions.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

namespace {

class IdleTaskRunner : public WebThread::IdleTask {
  USING_FAST_MALLOC(IdleTaskRunner);
  WTF_MAKE_NONCOPYABLE(IdleTaskRunner);

 public:
  explicit IdleTaskRunner(WebScheduler::IdleTask task)
      : task_(std::move(task)) {}

  ~IdleTaskRunner() override {}

  // WebThread::IdleTask implementation.
  void Run(double deadline_seconds) override { task_(deadline_seconds); }

 private:
  WebScheduler::IdleTask task_;
};

}  // namespace

void WebScheduler::PostIdleTask(const WebTraceLocation& location,
                                IdleTask idle_task) {
  PostIdleTask(location, new IdleTaskRunner(std::move(idle_task)));
}

void WebScheduler::PostNonNestableIdleTask(const WebTraceLocation& location,
                                           IdleTask idle_task) {
  PostNonNestableIdleTask(location, new IdleTaskRunner(std::move(idle_task)));
}

}  // namespace blink
