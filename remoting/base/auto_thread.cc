// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/auto_thread.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "base/synchronization/waitable_event.h"
#include "remoting/base/auto_thread_task_runner.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace remoting {

namespace {

#if defined(OS_WIN)
scoped_ptr<base::win::ScopedCOMInitializer> CreateComInitializer(
    AutoThread::ComInitType type) {
  scoped_ptr<base::win::ScopedCOMInitializer> initializer;
  if (type == AutoThread::COM_INIT_MTA) {
    initializer.reset(new base::win::ScopedCOMInitializer(
        base::win::ScopedCOMInitializer::kMTA));
  } else if (type == AutoThread::COM_INIT_STA) {
    initializer.reset(new base::win::ScopedCOMInitializer());
  }
  return initializer.Pass();
}
#endif

}

// Used to pass data to ThreadMain.  This structure is allocated on the stack
// from within StartWithType.
struct AutoThread::StartupData {
  // Fields describing the desired thread behaviour.
  base::MessageLoop::Type loop_type;

  // Used to receive the AutoThreadTaskRunner for the thread.
  scoped_refptr<AutoThreadTaskRunner> task_runner;

  // Used to synchronize thread startup.
  base::WaitableEvent event;

  explicit StartupData(base::MessageLoop::Type type)
      : loop_type(type), event(false, false) {}
};

// static
scoped_refptr<AutoThreadTaskRunner> AutoThread::CreateWithType(
    const char* name,
    scoped_refptr<AutoThreadTaskRunner> joiner,
    base::MessageLoop::Type type) {
  AutoThread* thread = new AutoThread(name, joiner.get());
  scoped_refptr<AutoThreadTaskRunner> task_runner = thread->StartWithType(type);
  if (!task_runner.get())
    delete thread;
  return task_runner;
}

// static
scoped_refptr<AutoThreadTaskRunner> AutoThread::Create(
    const char* name, scoped_refptr<AutoThreadTaskRunner> joiner) {
  return CreateWithType(name, joiner, base::MessageLoop::TYPE_DEFAULT);
}

#if defined(OS_WIN)
// static
scoped_refptr<AutoThreadTaskRunner> AutoThread::CreateWithLoopAndComInitTypes(
    const char* name,
    scoped_refptr<AutoThreadTaskRunner> joiner,
    base::MessageLoop::Type loop_type,
    ComInitType com_init_type) {
  AutoThread* thread = new AutoThread(name, joiner);
  thread->SetComInitType(com_init_type);
  scoped_refptr<AutoThreadTaskRunner> task_runner =
      thread->StartWithType(loop_type);
  if (!task_runner)
    delete thread;
  return task_runner;
}
#endif

AutoThread::AutoThread(const char* name)
  : startup_data_(NULL),
#if defined(OS_WIN)
    com_init_type_(COM_INIT_NONE),
#endif
    thread_(),
    name_(name),
    was_quit_properly_(false) {
}

AutoThread::AutoThread(const char* name, AutoThreadTaskRunner* joiner)
  : startup_data_(NULL),
#if defined(OS_WIN)
    com_init_type_(COM_INIT_NONE),
#endif
    thread_(),
    name_(name),
    was_quit_properly_(false),
    joiner_(joiner) {
}

AutoThread::~AutoThread() {
  DCHECK(!startup_data_);

  // Wait for the thread to exit.
  if (!thread_.is_null()) {
    base::PlatformThread::Join(thread_);
  }
}

scoped_refptr<AutoThreadTaskRunner> AutoThread::StartWithType(
    base::MessageLoop::Type type) {
  DCHECK(thread_.is_null());
#if defined(OS_WIN)
  DCHECK(com_init_type_ != COM_INIT_STA || type == base::MessageLoop::TYPE_UI);
#endif

  StartupData startup_data(type);
  startup_data_ = &startup_data;

  if (!base::PlatformThread::Create(0, this, &thread_)) {
    DLOG(ERROR) << "failed to create thread";
    startup_data_ = NULL;
    return NULL;
  }

  // Wait for the thread to start and initialize message_loop_
  // TODO(wez): Since at this point we know the MessageLoop _will_ run, and
  // the thread lifetime is controlled by the AutoThreadTaskRunner, we would
  // ideally return the AutoThreadTaskRunner to the caller without waiting for
  // the thread to signal us.
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  startup_data.event.Wait();

  // set it to NULL so we don't keep a pointer to some object on the stack.
  startup_data_ = NULL;

  DCHECK(startup_data.task_runner.get());
  return startup_data.task_runner;
}

#if defined(OS_WIN)
void AutoThread::SetComInitType(ComInitType com_init_type) {
  DCHECK_EQ(com_init_type_, COM_INIT_NONE);
  com_init_type_ = com_init_type;
}
#endif

void AutoThread::QuitThread(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (!task_runner->BelongsToCurrentThread()) {
    task_runner->PostTask(FROM_HERE, base::Bind(&AutoThread::QuitThread,
                                                base::Unretained(this),
                                                task_runner));
    return;
  }

  base::MessageLoop::current()->Quit();
  was_quit_properly_ = true;

  if (joiner_.get()) {
    joiner_->PostTask(
        FROM_HERE,
        base::Bind(&AutoThread::JoinAndDeleteThread, base::Unretained(this)));
  }
}

void AutoThread::JoinAndDeleteThread() {
  delete this;
}

void AutoThread::ThreadMain() {
  // The message loop for this thread.
  base::MessageLoop message_loop(startup_data_->loop_type);

  // Complete the initialization of our AutoThread object.
  base::PlatformThread::SetName(name_.c_str());
  ANNOTATE_THREAD_NAME(name_.c_str());  // Tell the name to race detector.
  message_loop.set_thread_name(name_);

  // Return an AutoThreadTaskRunner that will cleanly quit this thread when
  // no more references to it remain.
  startup_data_->task_runner =
      new AutoThreadTaskRunner(message_loop.message_loop_proxy(),
          base::Bind(&AutoThread::QuitThread,
                     base::Unretained(this),
                     message_loop.message_loop_proxy()));

  startup_data_->event.Signal();
  // startup_data_ can't be touched anymore since the starting thread is now
  // unlocked.

#if defined(OS_WIN)
  // Initialize COM on the thread, if requested.
  scoped_ptr<base::win::ScopedCOMInitializer> com_initializer(
      CreateComInitializer(com_init_type_));
#endif

  message_loop.Run();

  // Assert that MessageLoop::Quit was called by AutoThread::QuitThread.
  DCHECK(was_quit_properly_);
}

}  // namespace base
