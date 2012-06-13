// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_EVENT_EXECUTOR_H_
#define REMOTING_HOST_EVENT_EXECUTOR_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/host_event_stub.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class Capturer;

class EventExecutor : public protocol::HostEventStub {
 public:
  // Creates a default event executor for the current platform. This
  // object should do as much work as possible on |main_task_runner|,
  // using |ui_task_runner| only for tasks actually requiring a UI
  // thread.
  static scoped_ptr<EventExecutor> Create(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      Capturer* capturer);

  // Initialises any objects needed to execute events.
  virtual void OnSessionStarted(
      scoped_ptr<protocol::ClipboardStub> client_clipboard) = 0;

  // Destroys any objects constructed by Start().
  virtual void OnSessionFinished() = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_EVENT_EXECUTOR_H_
