// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An implementation of WebThread in terms of base::MessageLoop and
// base::Thread

#include "webkit/glue/webthread_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"

namespace webkit_glue {

WebThreadImpl::WebThreadImpl(const char* name)
    : thread_(new base::Thread(name)) {
  thread_->Start();
}

void WebThreadImpl::postTask(Task* task) {
  thread_->message_loop()->PostTask(
      FROM_HERE, base::Bind(&WebKit::WebThread::Task::run, base::Owned(task)));
}
void WebThreadImpl::postDelayedTask(
    Task* task, long long delay_ms) {
  thread_->message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&WebKit::WebThread::Task::run, base::Owned(task)),
      delay_ms);
}

WebThreadImpl::~WebThreadImpl() {
  thread_->Stop();
}

WebThreadImplForMessageLoop::WebThreadImplForMessageLoop(
    base::MessageLoopProxy* message_loop)
    : message_loop_(message_loop) {
}

void WebThreadImplForMessageLoop::postTask(Task* task) {
  message_loop_->PostTask(
      FROM_HERE, base::Bind(&WebKit::WebThread::Task::run, base::Owned(task)));
}

void WebThreadImplForMessageLoop::postDelayedTask(
    Task* task, long long delay_ms) {
  message_loop_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&WebKit::WebThread::Task::run, base::Owned(task)),
      delay_ms);
}

WebThreadImplForMessageLoop::~WebThreadImplForMessageLoop() {
}

}
