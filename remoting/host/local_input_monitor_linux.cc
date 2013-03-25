// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/local_input_monitor.h"

#include <sys/select.h>
#include <unistd.h>
#define XK_MISCELLANY
#include <X11/keysymdef.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/message_pump_libevent.h"
#include "base/posix/eintr_wrapper.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/non_thread_safe.h"
#include "remoting/host/client_session_control.h"
#include "third_party/skia/include/core/SkPoint.h"

// These includes need to be later than dictated by the style guide due to
// Xlib header pollution, specifically the min, max, and Status macros.
#include <X11/XKBlib.h>
#include <X11/Xlibint.h>
#include <X11/extensions/record.h>

namespace remoting {

namespace {

class LocalInputMonitorLinux : public base::NonThreadSafe,
                               public LocalInputMonitor {
 public:
  LocalInputMonitorLinux(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      base::WeakPtr<ClientSessionControl> client_session_control);
  virtual ~LocalInputMonitorLinux();

 private:
  // The actual implementation resides in LocalInputMonitorLinux::Core class.
  class Core
      : public base::RefCountedThreadSafe<Core>,
        public base::MessagePumpLibevent::Watcher {
   public:
    Core(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
         scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
         base::WeakPtr<ClientSessionControl> client_session_control);

    void Start();
    void Stop();

   private:
    friend class base::RefCountedThreadSafe<Core>;
    virtual ~Core();

    void StartOnInputThread();
    void StopOnInputThread();

    // base::MessagePumpLibevent::Watcher interface.
    virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
    virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

    // Processes key and mouse events.
    void ProcessXEvent(xEvent* event);

    static void ProcessReply(XPointer self, XRecordInterceptData* data);

    // Task runner on which public methods of this class must be called.
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

    // Task runner on which X Window events are received.
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;

    // Points to the object receiving mouse event notifications and session
    // disconnect requests.
    base::WeakPtr<ClientSessionControl> client_session_control_;

    // Used to receive base::MessagePumpLibevent::Watcher events.
    base::MessagePumpLibevent::FileDescriptorWatcher controller_;

    // True when Alt is pressed.
    bool alt_pressed_;

    // True when Ctrl is pressed.
    bool ctrl_pressed_;

    Display* display_;
    Display* x_record_display_;
    XRecordRange* x_record_range_[2];
    XRecordContext x_record_context_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(LocalInputMonitorLinux);
};

LocalInputMonitorLinux::LocalInputMonitorLinux(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control)
    : core_(new Core(caller_task_runner,
                     input_task_runner,
                     client_session_control)) {
  core_->Start();
}

LocalInputMonitorLinux::~LocalInputMonitorLinux() {
  core_->Stop();
}

LocalInputMonitorLinux::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control)
    : caller_task_runner_(caller_task_runner),
      input_task_runner_(input_task_runner),
      client_session_control_(client_session_control),
      alt_pressed_(false),
      ctrl_pressed_(false),
      display_(NULL),
      x_record_display_(NULL),
      x_record_context_(0) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(client_session_control_);

  x_record_range_[0] = NULL;
  x_record_range_[1] = NULL;
}

void LocalInputMonitorLinux::Core::Start() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  input_task_runner_->PostTask(FROM_HERE,
                               base::Bind(&Core::StartOnInputThread, this));
}

void LocalInputMonitorLinux::Core::Stop() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  input_task_runner_->PostTask(FROM_HERE,
                               base::Bind(&Core::StopOnInputThread, this));
}

LocalInputMonitorLinux::Core::~Core() {
  DCHECK(!display_);
  DCHECK(!x_record_display_);
  DCHECK(!x_record_range_[0]);
  DCHECK(!x_record_range_[1]);
  DCHECK(!x_record_context_);
}

void LocalInputMonitorLinux::Core::StartOnInputThread() {
  DCHECK(input_task_runner_->BelongsToCurrentThread());
  DCHECK(!display_);
  DCHECK(!x_record_display_);
  DCHECK(!x_record_range_[0]);
  DCHECK(!x_record_range_[1]);
  DCHECK(!x_record_context_);

  // TODO(jamiewalch): We should pass the display in. At that point, since
  // XRecord needs a private connection to the X Server for its data channel
  // and both channels are used from a separate thread, we'll need to duplicate
  // them with something like the following:
  //   XOpenDisplay(DisplayString(display));
  display_ = XOpenDisplay(NULL);
  x_record_display_ = XOpenDisplay(NULL);
  if (!display_ || !x_record_display_) {
    LOG(ERROR) << "Couldn't open X display";
    return;
  }

  int xr_opcode, xr_event, xr_error;
  if (!XQueryExtension(display_, "RECORD", &xr_opcode, &xr_event, &xr_error)) {
    LOG(ERROR) << "X Record extension not available.";
    return;
  }

  x_record_range_[0] = XRecordAllocRange();
  x_record_range_[1] = XRecordAllocRange();
  if (!x_record_range_[0] || !x_record_range_[1]) {
    LOG(ERROR) << "XRecordAllocRange failed.";
    return;
  }
  x_record_range_[0]->device_events.first = MotionNotify;
  x_record_range_[0]->device_events.last = MotionNotify;
  x_record_range_[1]->device_events.first = KeyPress;
  x_record_range_[1]->device_events.last = KeyRelease;
  XRecordClientSpec client_spec = XRecordAllClients;

  x_record_context_ = XRecordCreateContext(
      x_record_display_, 0, &client_spec, 1, x_record_range_,
      arraysize(x_record_range_));
  if (!x_record_context_) {
    LOG(ERROR) << "XRecordCreateContext failed.";
    return;
  }

  if (!XRecordEnableContextAsync(x_record_display_, x_record_context_,
                                 &Core::ProcessReply,
                                 reinterpret_cast<XPointer>(this))) {
    LOG(ERROR) << "XRecordEnableContextAsync failed.";
    return;
  }

  // Register OnFileCanReadWithoutBlocking() to be called every time there is
  // something to read from |x_record_display_|.
  MessageLoopForIO* message_loop = MessageLoopForIO::current();
  int result = message_loop->WatchFileDescriptor(
      ConnectionNumber(x_record_display_), true, MessageLoopForIO::WATCH_READ,
      &controller_, this);
  if (!result) {
    LOG(ERROR) << "Failed to create X record task.";
    return;
  }

  // Fetch pending events if any.
  while (XPending(x_record_display_)) {
    XEvent ev;
    XNextEvent(x_record_display_, &ev);
  }
}

void LocalInputMonitorLinux::Core::StopOnInputThread() {
  DCHECK(input_task_runner_->BelongsToCurrentThread());

  // Context must be disabled via the control channel because we can't send
  // any X protocol traffic over the data channel while it's recording.
  if (x_record_context_) {
    XRecordDisableContext(display_, x_record_context_);
    XFlush(display_);
  }

  controller_.StopWatchingFileDescriptor();

  if (x_record_range_[0]) {
    XFree(x_record_range_[0]);
    x_record_range_[0] = NULL;
  }
  if (x_record_range_[1]) {
    XFree(x_record_range_[1]);
    x_record_range_[1] = NULL;
  }
  if (x_record_context_) {
    XRecordFreeContext(x_record_display_, x_record_context_);
    x_record_context_ = 0;
  }
  if (x_record_display_) {
    XCloseDisplay(x_record_display_);
    x_record_display_ = NULL;
  }
  if (display_) {
    XCloseDisplay(display_);
    display_ = NULL;
  }
}

void LocalInputMonitorLinux::Core::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK(input_task_runner_->BelongsToCurrentThread());

  // Fetch pending events if any.
  while (XPending(x_record_display_)) {
    XEvent ev;
    XNextEvent(x_record_display_, &ev);
  }
}

void LocalInputMonitorLinux::Core::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

void LocalInputMonitorLinux::Core::ProcessXEvent(xEvent* event) {
  DCHECK(input_task_runner_->BelongsToCurrentThread());

  if (event->u.u.type == MotionNotify) {
    SkIPoint position(SkIPoint::Make(event->u.keyButtonPointer.rootX,
                                     event->u.keyButtonPointer.rootY));
    caller_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ClientSessionControl::OnLocalMouseMoved,
                              client_session_control_,
                              position));
  } else {
    int key_code = event->u.u.detail;
    bool down = event->u.u.type == KeyPress;
    KeySym key_sym = XkbKeycodeToKeysym(display_, key_code, 0, 0);
    if (key_sym == XK_Control_L || key_sym == XK_Control_R) {
      ctrl_pressed_ = down;
    } else if (key_sym == XK_Alt_L || key_sym == XK_Alt_R) {
      alt_pressed_ = down;
    } else if (key_sym == XK_Escape && down && alt_pressed_ && ctrl_pressed_) {
      caller_task_runner_->PostTask(
          FROM_HERE, base::Bind(&ClientSessionControl::DisconnectSession,
                                client_session_control_));
    }
  }
}

// static
void LocalInputMonitorLinux::Core::ProcessReply(XPointer self,
                                                XRecordInterceptData* data) {
  if (data->category == XRecordFromServer) {
    xEvent* event = reinterpret_cast<xEvent*>(data->data);
    reinterpret_cast<Core*>(self)->ProcessXEvent(event);
  }
  XRecordFreeData(data);
}

}  // namespace

scoped_ptr<LocalInputMonitor> LocalInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control) {
  return scoped_ptr<LocalInputMonitor>(
      new LocalInputMonitorLinux(caller_task_runner,
                                 input_task_runner,
                                 client_session_control));
}

}  // namespace remoting
