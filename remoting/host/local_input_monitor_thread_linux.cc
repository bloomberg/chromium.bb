// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/local_input_monitor_thread_linux.h"

#include <sys/select.h>
#include <unistd.h>
#define XK_MISCELLANY
#include <X11/keysymdef.h>

#include "base/basictypes.h"
#include "base/eintr_wrapper.h"
#include "remoting/host/chromoting_host.h"

// These includes need to be later than dictated by the style guide due to
// Xlib header pollution, specifically the min, max, and Status macros.
#include <X11/Xlibint.h>
#include <X11/extensions/record.h>

namespace {

struct scoped_x_record_context {
  scoped_x_record_context()
      : data_channel(NULL), context(0) {
    range[0] = range[1] = NULL;
  }
  ~scoped_x_record_context() {
    if (range[0])
      XFree(range[0]);
    if (range[1])
      XFree(range[1]);
    if (context)
      XRecordFreeContext(data_channel, context);
    if (data_channel)
      XCloseDisplay(data_channel);
  }
  Display* data_channel;
  XRecordRange* range[2];
  XRecordContext context;
};

}  // namespace


namespace remoting {

static void ProcessReply(XPointer thread,
                         XRecordInterceptData* data) {
  if (data->category == XRecordFromServer) {
    xEvent* event = reinterpret_cast<xEvent*>(data->data);
    if (event->u.u.type == MotionNotify) {
      gfx::Point pos(event->u.keyButtonPointer.rootX,
                     event->u.keyButtonPointer.rootY);
      reinterpret_cast<LocalInputMonitorThread*>(thread)->LocalMouseMoved(pos);
    } else {
      reinterpret_cast<LocalInputMonitorThread*>(thread)->LocalKeyPressed(
          event->u.u.detail, event->u.u.type == KeyPress);
    }
  }
  XRecordFreeData(data);
}

LocalInputMonitorThread::LocalInputMonitorThread(ChromotingHost* host)
    : base::SimpleThread("LocalInputMonitor"),
      host_(host), display_(NULL), alt_pressed_(false), ctrl_pressed_(false) {
  wakeup_pipe_[0] = -1;
  wakeup_pipe_[1] = -1;
  CHECK_EQ(pipe(wakeup_pipe_), 0);
}

LocalInputMonitorThread::~LocalInputMonitorThread() {
  close(wakeup_pipe_[0]);
  close(wakeup_pipe_[1]);
}

void LocalInputMonitorThread::Stop() {
  if (HANDLE_EINTR(write(wakeup_pipe_[1], "", 1)) != 1) {
    NOTREACHED() << "Could not write to the local input monitor wakeup pipe!";
  }
}

void LocalInputMonitorThread::Run() {
  // TODO(jamiewalch): For now, just don't run the thread if the wakeup pipe
  // could not be created. As part of the task of cleaning up the dis/connect
  // actions, this should be treated as an initialization failure.
  if (wakeup_pipe_[0] == -1)
    return;

  // TODO(jamiewalch): We should pass the display in. At that point, since
  // XRecord needs a private connection to the X Server for its data channel
  // and both channels are used from a separate thread, we'll need to duplicate
  // them with something like the following:
  //   XOpenDisplay(DisplayString(display));
  display_ = XOpenDisplay(NULL);

  // Inner scope needed here, because the |scoper| destructor may call into
  // LocalKeyPressed() which needs |display_| to be still open.
  {
    scoped_x_record_context scoper;
    scoper.data_channel = XOpenDisplay(NULL);
    if (!display_ || !scoper.data_channel) {
      LOG(ERROR) << "Couldn't open X display";
      return;
    }

    int xr_opcode, xr_event, xr_error;
    if (!XQueryExtension(display_, "RECORD", &xr_opcode, &xr_event,
                         &xr_error)) {
      LOG(ERROR) << "X Record extension not available.";
      return;
    }

    scoper.range[0] = XRecordAllocRange();
    scoper.range[1] = XRecordAllocRange();
    if (!scoper.range[0] || !scoper.range[1]) {
      LOG(ERROR) << "XRecordAllocRange failed.";
      return;
    }
    scoper.range[0]->device_events.first = MotionNotify;
    scoper.range[0]->device_events.last = MotionNotify;
    scoper.range[1]->device_events.first = KeyPress;
    scoper.range[1]->device_events.last = KeyRelease;
    XRecordClientSpec client_spec = XRecordAllClients;

    scoper.context = XRecordCreateContext(
        scoper.data_channel, 0, &client_spec, 1, scoper.range,
        arraysize(scoper.range));
    if (!scoper.context) {
      LOG(ERROR) << "XRecordCreateContext failed.";
      return;
    }

    if (!XRecordEnableContextAsync(scoper.data_channel, scoper.context,
                                   ProcessReply,
                                   reinterpret_cast<XPointer>(this))) {
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

    // Context must be disabled via the control channel because we can't send
    // any X protocol traffic over the data channel while it's recording.
    XRecordDisableContext(display_, scoper.context);
    XFlush(display_);
  }

  XCloseDisplay(display_);
  display_ = NULL;
}

void LocalInputMonitorThread::LocalMouseMoved(const gfx::Point& pos) {
  host_->LocalMouseMoved(pos);
}

void LocalInputMonitorThread::LocalKeyPressed(int key_code, bool down) {
  int key_sym = XKeycodeToKeysym(display_, key_code, 0);
  if (key_sym == XK_Control_L || key_sym == XK_Control_R) {
    ctrl_pressed_ = down;
  } else if (key_sym == XK_Alt_L || key_sym == XK_Alt_R) {
    alt_pressed_ = down;
  } else if (alt_pressed_ && ctrl_pressed_ && key_sym == XK_Escape && down) {
    host_->Shutdown(NULL);
  }
}

}  // namespace remoting
