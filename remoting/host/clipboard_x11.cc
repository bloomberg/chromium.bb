// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/clipboard.h"

#include <X11/Xlib.h>
#undef Status  // Xlib.h #defines this, which breaks protobuf headers.

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
  ~ClipboardX11() override;

  // Clipboard interface.
  void Start(scoped_ptr<protocol::ClipboardStub> client_clipboard) override;
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;

  // MessageLoopForIO::Watcher interface.
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

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
    : display_(nullptr) {
}

ClipboardX11::~ClipboardX11() {
  if (display_)
    XCloseDisplay(display_);
}

void ClipboardX11::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  // TODO(lambroslambrou): Share the X connection with InputInjector.
  display_ = XOpenDisplay(nullptr);
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
  return make_scoped_ptr(new ClipboardX11());
}

}  // namespace remoting
