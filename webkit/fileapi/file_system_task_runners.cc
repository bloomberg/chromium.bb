// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_task_runners.h"

#include "base/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"

namespace fileapi {

const char kMediaTaskRunnerName[] = "media-task-runner";

FileSystemTaskRunners::FileSystemTaskRunners(
    base::SingleThreadTaskRunner* io_task_runner,
    base::SingleThreadTaskRunner* file_task_runner,
    base::SequencedTaskRunner* media_task_runner)
    : io_task_runner_(io_task_runner),
      file_task_runner_(file_task_runner),
      media_task_runner_(media_task_runner) {
}

// static
scoped_ptr<FileSystemTaskRunners>
FileSystemTaskRunners::CreateMockTaskRunners() {
  return make_scoped_ptr(new FileSystemTaskRunners(
      base::MessageLoopProxy::current(),
      base::MessageLoopProxy::current(),
      base::MessageLoopProxy::current()));
}

FileSystemTaskRunners::~FileSystemTaskRunners() {
}

}  // namespace fileapi
