// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/WebTaskRunner.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/RefCounted.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"

namespace blink {

class MultiThreadedTest : public testing::Test {
 public:
  // RunOnThreads run a closure num_threads * callbacks_per_thread times.
  // The default for this is 10*100 = 1000 times.

  template <typename FunctionType, typename... Ps>
  void RunOnThreads(FunctionType function, Ps&&... parameters) {
    Vector<std::unique_ptr<WebThread>> threads;
    Vector<std::unique_ptr<WaitableEvent>> waits;

    for (int i = 0; i < num_threads_; ++i) {
      threads.push_back(Platform::Current()->CreateThread(""));
      waits.push_back(WTF::MakeUnique<WaitableEvent>());
    }

    for (int i = 0; i < num_threads_; ++i) {
      WebTaskRunner* task_runner = threads[i]->GetWebTaskRunner();

      for (int j = 0; j < callbacks_per_thread_; ++j) {
        task_runner->PostTask(FROM_HERE,
                              CrossThreadBind(function, parameters...));
      }

      task_runner->PostTask(
          FROM_HERE, CrossThreadBind([](WaitableEvent* w) { w->Signal(); },
                                     CrossThreadUnretained(waits[i].get())));
    }

    for (int i = 0; i < num_threads_; ++i) {
      waits[i]->Wait();
    }
  }

 protected:
  int num_threads_ = 10;
  int callbacks_per_thread_ = 100;
};

}  // namespace blink
