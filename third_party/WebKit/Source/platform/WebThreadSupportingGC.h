// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebThreadSupportingGC_h
#define WebThreadSupportingGC_h

#include <memory>
#include "platform/WebTaskRunner.h"
#include "platform/heap/GCTaskRunner.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"

namespace blink {

// WebThreadSupportingGC wraps a WebThread and adds support for attaching
// to and detaching from the Blink GC infrastructure.
//
// The initialize method must be called during initialization on the WebThread
// and before the thread allocates any objects managed by the Blink GC. The
// shutdown method must be called on the WebThread during shutdown when the
// thread no longer needs to access objects managed by the Blink GC.
//
// WebThreadSupportingGC usually internally creates and owns WebThread unless
// an existing WebThread is given via createForThread.
class PLATFORM_EXPORT WebThreadSupportingGC final {
  USING_FAST_MALLOC(WebThreadSupportingGC);
  WTF_MAKE_NONCOPYABLE(WebThreadSupportingGC);

 public:
  static std::unique_ptr<WebThreadSupportingGC> Create(const char* name);
  static std::unique_ptr<WebThreadSupportingGC> CreateForThread(WebThread*);
  ~WebThreadSupportingGC();

  void PostTask(const WebTraceLocation& location, WTF::Closure task) {
    thread_->GetWebTaskRunner()->PostTask(location, std::move(task));
  }

  void PostDelayedTask(const WebTraceLocation& location,
                       WTF::Closure task,
                       TimeDelta delay) {
    thread_->GetWebTaskRunner()->PostDelayedTask(location, std::move(task),
                                                 delay);
  }

  void PostTask(const WebTraceLocation& location, CrossThreadClosure task) {
    thread_->GetWebTaskRunner()->PostTask(location, std::move(task));
  }

  void PostDelayedTask(const WebTraceLocation& location,
                       CrossThreadClosure task,
                       TimeDelta delay) {
    thread_->GetWebTaskRunner()->PostDelayedTask(location, std::move(task),
                                                 delay);
  }

  bool IsCurrentThread() const { return thread_->IsCurrentThread(); }

  void AddTaskObserver(WebThread::TaskObserver* observer) {
    thread_->AddTaskObserver(observer);
  }

  void RemoveTaskObserver(WebThread::TaskObserver* observer) {
    thread_->RemoveTaskObserver(observer);
  }

  // Must be called on the WebThread.
  void InitializeOnThread();
  void ShutdownOnThread();

  WebThread& PlatformThread() const {
    DCHECK(thread_);
    return *thread_;
  }

 private:
  WebThreadSupportingGC(const char* name, WebThread*);

  std::unique_ptr<GCTaskRunner> gc_task_runner_;

  // m_thread is guaranteed to be non-null after this instance is constructed.
  // m_owningThread is non-null unless this instance is constructed for an
  // existing thread via createForThread().
  WebThread* thread_ = nullptr;
  std::unique_ptr<WebThread> owning_thread_;
};

}  // namespace blink

#endif
