// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/local_input_monitor_thread_linux.h"
#include "remoting/host/chromoting_host.h"

#include <sys/select.h>
#include <unistd.h>
#include <X11/Xlibint.h>
#include <X11/extensions/record.h>

namespace {

struct scoped_x_record_context {
  scoped_x_record_context()
      : control_channel(NULL), data_channel(NULL), range(NULL), context(0) {}
  ~scoped_x_record_context() {
    if (range)
      XFree(range);
    if (context)
      XRecordFreeContext(data_channel, context);
    if (data_channel)
      XCloseDisplay(data_channel);
    if (control_channel)
      XCloseDisplay(control_channel);
  }
  Display* control_channel;
  Display* data_channel;
  XRecordRange* range;
  XRecordContext context;
};

}  // namespace


namespace remoting {

static void ProcessReply(XPointer host,
                         XRecordInterceptData* data) {
  if (data->category == XRecordFromServer) {
    xEvent* event = reinterpret_cast<xEvent*>(data->data);
    gfx::Point pos(event->u.keyButtonPointer.rootX,
                   event->u.keyButtonPointer.rootY);
    reinterpret_cast<ChromotingHost*>(host)->LocalMouseMoved(pos);
  }
  XRecordFreeData(data);
}

LocalInputMonitorThread::LocalInputMonitorThread(ChromotingHost* host)
    : base::SimpleThread("LocalInputMonitor"),
      host_(host) {
  wakeup_pipe_[0] = -1;
  wakeup_pipe_[1] = -1;
  CHECK_EQ(pipe(wakeup_pipe_), 0);
}

LocalInputMonitorThread::~LocalInputMonitorThread() {
  close(wakeup_pipe_[0]);
  close(wakeup_pipe_[1]);
}

void LocalInputMonitorThread::Stop() {
  write(wakeup_pipe_[1], "", 1);
}

void LocalInputMonitorThread::Run() {
  // TODO(jamiewalch): For now, just don't run the thread if the wakeup pipe
  // could not be created. As part of the task of cleaning up the dis/connect
  // actions, this should be treated as an initialization failure.
  if (wakeup_pipe_[0] == -1)
    return;

  scoped_x_record_context scoper;

  // TODO(jamiewalch): We should pass the display in. At that point, since
  // XRecord needs a private connection to the X Server for its data channel,
  // we'll need something like the following:
  //   data_channel = XOpenDisplay(DisplayString(control_channel));
  scoper.data_channel = XOpenDisplay(NULL);
  scoper.control_channel = XOpenDisplay(NULL);
  if (!scoper.data_channel || !scoper.control_channel) {
    LOG(ERROR) << "Couldn't open X display";
    return;
  }

  int xr_opcode, xr_event, xr_error;
  if (!XQueryExtension(scoper.control_channel, "RECORD",
                       &xr_opcode, &xr_event, &xr_error)) {
    LOG(ERROR) << "X Record extension not available.";
    return;
  }

  scoper.range = XRecordAllocRange();
  if (!scoper.range) {
    LOG(ERROR) << "XRecordAllocRange failed.";
    return;
  }
  scoper.range->device_events.first = MotionNotify;
  scoper.range->device_events.last = MotionNotify;
  XRecordClientSpec client_spec = XRecordAllClients;

  scoper.context = XRecordCreateContext(scoper.data_channel, 0, &client_spec, 1,
                                        &scoper.range, 1);
  if (!scoper.context) {
    LOG(ERROR) << "XRecordCreateContext failed.";
    return;
  }

  if (!XRecordEnableContextAsync(scoper.data_channel, scoper.context,
                                 ProcessReply,
                                 reinterpret_cast<XPointer>(host_))) {
    LOG(ERROR) << "XRecordEnableContextAsync failed.";
    return;
  }

  bool stopped = false;
  while (!stopped) {
    while (XPending(scoper.data_channel)) {
      XEvent ev;
      XNextEvent(scoper.data_channel, &ev);
    }
    fd_set read_fs;
    FD_ZERO(&read_fs);
    FD_SET(ConnectionNumber(scoper.data_channel), &read_fs);
    FD_SET(wakeup_pipe_[0], &read_fs);
    select(FD_SETSIZE, &read_fs, NULL, NULL, NULL);
    stopped = FD_ISSET(wakeup_pipe_[0], &read_fs);
  }

  // Context must be disabled via the control channel because we can't send any
  // X protocol traffic over the data channel while it's recording.
  XRecordDisableContext(scoper.control_channel, scoper.context);
  XFlush(scoper.control_channel);
}

}  // namespace remoting
