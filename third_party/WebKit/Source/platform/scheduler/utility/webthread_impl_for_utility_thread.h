// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_UTILITY_WEBTHREAD_IMPL_FOR_UTILITY_THREAD_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_UTILITY_WEBTHREAD_IMPL_FOR_UTILITY_THREAD_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "platform/PlatformExport.h"
#include "public/platform/scheduler/child/webthread_base.h"

namespace blink {
namespace scheduler {

class PLATFORM_EXPORT WebThreadImplForUtilityThread
    : public scheduler::WebThreadBase {
 public:
  WebThreadImplForUtilityThread();
  ~WebThreadImplForUtilityThread() override;

  // WebThread implementation.
  WebScheduler* Scheduler() const override;
  PlatformThreadId ThreadId() const override;

  // WebThreadBase implementation.
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() const override;
  scheduler::SingleThreadIdleTaskRunner* GetIdleTaskRunner() const override;
  void Init() override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  PlatformThreadId thread_id_;

  DISALLOW_COPY_AND_ASSIGN(WebThreadImplForUtilityThread);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_UTILITY_WEBTHREAD_IMPL_FOR_UTILITY_THREAD_H_
