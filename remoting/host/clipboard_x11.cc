// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/clipboard.h"

#include <X11/Xlib.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "remoting/host/linux/x_server_clipboard.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"

namespace remoting {

// This code is expected to be called on the desktop thread only.
class ClipboardX11 : public Clipboard,
                     public base::MessageLoopForIO::Watcher {
 public:
  ClipboardX11();
  virtual ~ClipboardX11();

  // Clipboard interface.
  virtual void Start(
      scoped_ptr<protocol::ClipboardStub> client_clipboard) OVERRIDE;
  virtual void InjectClipboardEvent(
      const protocol::ClipboardEvent& event) OVERRIDE;
  virtual void Stop() OVERRIDE;

  // MessageLoopForIO::Watcher interface.
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

 private:
  void OnClipboardChanged(const std::string& mime_type,
                          const std::string& data);
  void PumpXEvents();

  scoped_ptr<protocol::ClipboardStub> client_clipboard_;

  // Underlying X11 clipboard implementation.
  XServerClipboard x_server_clipboard_;

  // Connection to the X server, used by |x_server_clipboard_|. This is created
  // and owned by this class.
  Display* display_;

  // Watcher used to handle X11 events from |display_|.
  base::MessageLoopForIO::FileDescriptorWatcher x_connection_watcher_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardX11);
};

ClipboardX11::ClipboardX11()
    : display_(NULL) {
}

ClipboardX11::~ClipboardX11() {
  Stop();
}

void ClipboardX11::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  // TODO(lambroslambrou): Share the X connection with InputInjector.
  display_ = XOpenDisplay(NULL);
  if (!display_) {
    LOG(ERROR) << "Couldn't open X display";
    return;
  }
  client_clipboard_.swap(client_clipboard);

  x_server_clipboard_.Init(display_,
                           base::Bind(&ClipboardX11::OnClipboardChanged,
                                      base::Unretained(this)));

  base::MessageLoopForIO::current()->WatchFileDescriptor(
      ConnectionNumber(display_),
      true,
      base::MessageLoopForIO::WATCH_READ,
      &x_connection_watcher_,
      this);
  PumpXEvents();
}

void ClipboardX11::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  x_server_clipboard_.SetClipboard(event.mime_type(), event.data());
}

void ClipboardX11::Stop() {
  client_clipboard_.reset();
  x_connection_watcher_.StopWatchingFileDescriptor();

  if (display_) {
    XCloseDisplay(display_);
    display_ = NULL;
  }
}

void ClipboardX11::OnFileCanReadWithoutBlocking(int fd) {
  PumpXEvents();
}

void ClipboardX11::OnFileCanWriteWithoutBlocking(int fd) {
}

void ClipboardX11::OnClipboardChanged(const std::string& mime_type,
                                      const std::string& data) {
  protocol::ClipboardEvent event;
  event.set_mime_type(mime_type);
  event.set_data(data);

  if (client_clipboard_.get()) {
    client_clipboard_->InjectClipboardEvent(event);
  }
}

void ClipboardX11::PumpXEvents() {
  DCHECK(display_);

  while (XPending(display_)) {
    XEvent event;
    XNextEvent(display_, &event);
    x_server_clipboard_.ProcessXEvent(&event);
  }
}

scoped_ptr<Clipboard> Clipboard::Create() {
  return scoped_ptr<Clipboard>(new ClipboardX11());
}

}  // namespace remoting
