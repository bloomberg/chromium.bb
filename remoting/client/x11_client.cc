// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements a X11 chromoting client.

#include <iostream>

#include "base/at_exit.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/task.h"
#include "base/waitable_event.h"
#include "remoting/client/client_util.h"
#include "remoting/client/host_connection.h"
#include "remoting/client/jingle_host_connection.h"
#include "remoting/jingle_glue/jingle_thread.h"

// Include Xlib at the end because it clashes with ClientMessage defined in
// the protocol buffer.
// x11_view.h also includes Xlib.h so put it behind all other headers but
// before Xlib.h
#include "remoting/client/x11_view.h"
#include <X11/Xlib.h>

using remoting::JingleHostConnection;
using remoting::JingleThread;

namespace remoting {

class X11Client : public HostConnection::HostEventCallback {
 public:
  X11Client(MessageLoop* loop, base::WaitableEvent* client_done)
      : message_loop_(loop),
        client_done_(client_done),
        display_(NULL),
        window_(0),
        width_(0),
        height_(0) {
  }

  virtual ~X11Client() {
    DCHECK(!display_);
    DCHECK(!window_);
  }

  ////////////////////////////////////////////////////////////////////////////
  // HostConnection::EventHandler implementations.
  virtual void HandleMessages(HostConnection* conn,
                              remoting::HostMessageList* messages) {
    for (size_t i = 0; i < messages->size(); ++i) {
      HostMessage* msg = (*messages)[i];
      if (msg->has_init_client()) {
        message_loop_->PostTask(
            FROM_HERE, NewRunnableMethod(this, &X11Client::DoInitClient, msg));
      } else if (msg->has_begin_update_stream()) {
        message_loop_->PostTask(
            FROM_HERE,
            NewRunnableMethod(this, &X11Client::DoBeginUpdate, msg));
      } else if (msg->has_update_stream_packet()) {
        message_loop_->PostTask(
            FROM_HERE,
            NewRunnableMethod(this, &X11Client::DoHandleUpdate, msg));
      } else if (msg->has_end_update_stream()) {
        message_loop_->PostTask(
            FROM_HERE, NewRunnableMethod(this, &X11Client::DoEndUpdate, msg));
      } else {
        NOTREACHED() << "Unknown message received";
      }
    }
    // Assume we have processed all the messages.
    messages->clear();
  }

  virtual void OnConnectionOpened(HostConnection* conn) {
    std::cout << "Connection established." << std::endl;
  }

  virtual void OnConnectionClosed(HostConnection* conn) {
    std::cout << "Connection closed." << std::endl;
    client_done_->Signal();
  }

  virtual void OnConnectionFailed(HostConnection* conn) {
    std::cout << "Conection failed." << std::endl;
    client_done_->Signal();
  }

  void InitX11() {
    message_loop_->PostTask(FROM_HERE,
                         NewRunnableMethod(this, &X11Client::DoInitX11));
  }

  void DoInitX11() {
    display_ = XOpenDisplay(NULL);
    if (!display_) {
      std::cout << "Error - cannot open display" << std::endl;
      client_done_->Signal();
      return;
    }

    // Get properties of the screen.
    int screen = DefaultScreen(display_);
    int root_window = RootWindow(display_, screen);

    // Creates the window.
    window_ = XCreateSimpleWindow(display_, root_window, 1, 1, 640, 480, 0,
                                  BlackPixel(display_, screen),
                                  BlackPixel(display_, screen));
    DCHECK(window_);
    XStoreName(display_, window_, "X11 Remoting");

    // Specifies what kind of messages we want to receive.
    XSelectInput(display_, window_, ExposureMask | ButtonPressMask);
    XMapWindow(display_, window_);
  }

  void DestroyX11() {
    message_loop_->PostTask(FROM_HERE,
                         NewRunnableMethod(this, &X11Client::DoDestroyX11));
  }

  void DoDestroyX11() {
    if (display_ && window_) {
      // Shutdown the window system.
      XDestroyWindow(display_, window_);
      XCloseDisplay(display_);
      display_ = NULL;
      window_ = 0;
    }
  }

 private:
  // This method is executed on the main loop.
  void DoInitClient(HostMessage* msg) {
    DCHECK_EQ(message_loop_, MessageLoop::current());
    DCHECK(msg->has_init_client());
    scoped_ptr<HostMessage> deleter(msg);

    // Saves the dimension and resize the window.
    width_ = msg->init_client().width();
    height_ = msg->init_client().height();
    LOG(INFO) << "Init client received: " << width_ << "x" << height_;
    XResizeWindow(display_, window_, width_, height_);

    // Now construct the X11View that renders the remote desktop.
    view_.reset(new X11View(display_, window_, width_, height_));

    // Schedule the event handler to process the event queue of X11.
    ScheduleX11EventHandler();
  }

  // The following methods are executed on the same thread as libjingle.
  void DoBeginUpdate(HostMessage* msg) {
    DCHECK_EQ(message_loop_, MessageLoop::current());
    DCHECK(msg->has_begin_update_stream());

    view_->HandleBeginUpdateStream(msg);
  }

  void DoHandleUpdate(HostMessage* msg) {
    DCHECK_EQ(message_loop_, MessageLoop::current());
    DCHECK(msg->has_update_stream_packet());

    view_->HandleUpdateStreamPacket(msg);
  }

  void DoEndUpdate(HostMessage* msg) {
    DCHECK_EQ(message_loop_, MessageLoop::current());
    DCHECK(msg->has_end_update_stream());

    view_->HandleEndUpdateStream(msg);
  }

  void DoProcessX11Events() {
    DCHECK_EQ(message_loop_, MessageLoop::current());
    if (XPending(display_)) {
      XEvent e;
      XNextEvent(display_, &e);
      if (e.type == Expose) {
        // Tell the ChromotingView to paint again.
        view_->Paint();
      } else if (e.type == ButtonPress) {
        // TODO(hclam): Implement.
        NOTIMPLEMENTED();
      } else {
        // TODO(hclam): Implement.
        NOTIMPLEMENTED();
      }
    }

    // Schedule the next event handler.
    ScheduleX11EventHandler();
  }

  void ScheduleX11EventHandler() {
    // Schedule a delayed task to process X11 events in 10ms.
    static const int kProcessEventsInterval = 10;
    message_loop_->PostDelayedTask(
        FROM_HERE,
        NewRunnableMethod(this, &X11Client::DoProcessX11Events),
        kProcessEventsInterval);
  }

  MessageLoop* message_loop_;
  base::WaitableEvent* client_done_;

  // Members used for display.
  Display* display_;
  Window window_;

  // Dimension of the window. They are initialized when InitClient message is
  // received.
  int width_;
  int height_;

  scoped_ptr<ChromotingView> view_;

  DISALLOW_COPY_AND_ASSIGN(X11Client);
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::X11Client);

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  std::string host_jid;
  std::string username;
  std::string auth_token;

  if (!remoting::GetLoginInfo(argc, argv, &host_jid, &username, &auth_token)) {
    std::cout << "Cannot obtain login info" << std::endl;
    return 1;
  }

  JingleThread network_thread;
  network_thread.Start();

  base::WaitableEvent client_done(false, false);
  remoting::X11Client client(network_thread.message_loop(), &client_done);
  JingleHostConnection connection(&network_thread);
  connection.Connect(username, auth_token, host_jid, &client);

  client.InitX11();

  // Wait until the main loop is done.
  client_done.Wait();

  connection.Disconnect();

  client.DestroyX11();

  // Quit the current message loop.
  network_thread.message_loop()->PostTask(FROM_HERE,
                                          new MessageLoop::QuitTask());
  network_thread.Stop();

  return 0;
}
