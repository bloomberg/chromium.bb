// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements a X11 chromoting client.

#include <iostream>

#include "base/at_exit.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "remoting/client/host_connection.h"
#include "remoting/client/client_util.h"

// Include Xlib at the end because it clashes with ClientMessage defined in
// the protocol buffer.
// x11_view.h also includes Xlib.h so put it behind all other headers but
// before Xlib.h
#include "remoting/client/x11_view.h"
#include <X11/Xlib.h>

namespace remoting {

class X11Client : public base::RefCountedThreadSafe<X11Client>,
                  public HostConnection::EventHandler {
 public:
  X11Client(std::string host_jid, std::string username, std::string auth_token)
      : display_(NULL),
        window_(0),
        width_(0),
        height_(0),
        host_jid_(host_jid),
        username_(username),
        auth_token_(auth_token) {
  }

  virtual ~X11Client() {
    DCHECK(!display_);
    DCHECK(!window_);
  }

  // Starts the remoting client and the message loop. Returns only after
  // the message loop has terminated.
  void Run() {
    message_loop_.PostTask(FROM_HERE,
                        NewRunnableMethod(this, &X11Client::DoInitX11));
    message_loop_.PostTask(FROM_HERE,
                        NewRunnableMethod(this, &X11Client::DoInitConnection));
    message_loop_.Run();
  }

  ////////////////////////////////////////////////////////////////////////////
  // HostConnection::EventHandler implementations.
  virtual void HandleMessages(HostConnection* conn,
                              remoting::HostMessageList* messages) {
    for (size_t i = 0; i < messages->size(); ++i) {
      HostMessage* msg = (*messages)[i];
      if (msg->has_init_client()) {
        message_loop_.PostTask(
            FROM_HERE, NewRunnableMethod(this, &X11Client::DoInitClient, msg));
      } else if (msg->has_begin_update_stream()) {
        message_loop_.PostTask(
            FROM_HERE,
            NewRunnableMethod(this, &X11Client::DoBeginUpdate, msg));
      } else if (msg->has_update_stream_packet()) {
        message_loop_.PostTask(
            FROM_HERE,
            NewRunnableMethod(this, &X11Client::DoHandleUpdate, msg));
      } else if (msg->has_end_update_stream()) {
        message_loop_.PostTask(
            FROM_HERE, NewRunnableMethod(this, &X11Client::DoEndUpdate, msg));
      } else {
        NOTREACHED() << "Unknown message received";
      }
    }
    // Assume we have processed all the messages.
    messages->clear();
  }

  virtual void OnConnectionOpened(HostConnection* conn) {
    std::cout << "Connection establised." << std::endl;
  }

  virtual void OnConnectionClosed(HostConnection* conn) {
    std::cout << "Connection closed." << std::endl;
    Exit();
  }

  virtual void OnConnectionFailed(HostConnection* conn) {
    std::cout << "Conection failed." << std::endl;
    Exit();
  }

 private:
  void DoInitX11() {
    display_ = XOpenDisplay(NULL);
    if (!display_) {
      std::cout << "Error - cannot open display" << std::endl;
      Exit();
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

  void DoInitConnection() {
    // If the initialization of X11 has failed then return directly.
    if (!display_)
      return;

    // Creates a HostConnection object and connection to the host.
    LOG(INFO) << "Connecting...";
    connection_.reset(new HostConnection(new ProtocolDecoder(), this));
    connection_->Connect(username_, auth_token_, host_jid_);
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

  void DoDisconnect() {
    if (connection_.get())
      connection_->Disconnect();
  }

  void Exit() {
    // Disconnect the jingle channel and client.
    message_loop_.PostTask(FROM_HERE,
                        NewRunnableMethod(this, &X11Client::DoDisconnect));

    // Post a task to shutdown X11.
    message_loop_.PostTask(FROM_HERE,
                        NewRunnableMethod(this, &X11Client::DoDestroyX11));

    // Quit the current message loop.
    message_loop_.PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

  // This method is executed on the main loop.
  void DoInitClient(HostMessage* msg) {
    DCHECK_EQ(&message_loop_, MessageLoop::current());
    DCHECK(msg->has_init_client());
    scoped_ptr<HostMessage> deleter(msg);

    // Saves the dimension and resize the window.
    width_ = msg->init_client().width();
    height_ = msg->init_client().height();
    LOG(INFO) << "Init client receievd: " << width_ << "x" << height_;
    XResizeWindow(display_, window_, width_, height_);

    // Now construct the X11View that renders the remote desktop.
    view_ = new X11View(display_, window_, width_, height_);

    // Schedule the event handler to process the event queue of X11.
    ScheduleX11EventHandler();
  }

  // The following methods are executed on the same thread as libjingle.
  void DoBeginUpdate(HostMessage* msg) {
    DCHECK_EQ(&message_loop_, MessageLoop::current());
    DCHECK(msg->has_begin_update_stream());

    view_->HandleBeginUpdateStream(msg);
  }

  void DoHandleUpdate(HostMessage* msg) {
    DCHECK_EQ(&message_loop_, MessageLoop::current());
    DCHECK(msg->has_update_stream_packet());

    view_->HandleUpdateStreamPacket(msg);
  }

  void DoEndUpdate(HostMessage* msg) {
    DCHECK_EQ(&message_loop_, MessageLoop::current());
    DCHECK(msg->has_end_update_stream());

    view_->HandleEndUpdateStream(msg);
  }

  void DoProcessX11Events() {
    DCHECK_EQ(&message_loop_, MessageLoop::current());
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
    message_loop_.PostDelayedTask(
        FROM_HERE,
        NewRunnableMethod(this, &X11Client::DoProcessX11Events),
        kProcessEventsInterval);
  }

  // Members used for display.
  Display* display_;
  Window window_;

  // Dimension of the window. They are initialized when InitClient message is
  // received.
  int width_;
  int height_;

  std::string host_jid_;
  std::string username_;
  std::string auth_token_;
  scoped_ptr<HostConnection> connection_;
  scoped_refptr<ChromotingView> view_;

  MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(X11Client);
};

}  // namespace remoting

int main() {
  base::AtExitManager at_exit;
  std::string host_jid, username, auth_token;

  if (!remoting::GetLoginInfo(host_jid, username, auth_token)) {
    std::cout << "Cannot obtain login info" << std::endl;
    return 1;
  }

  scoped_refptr<remoting::X11Client> client =
      new remoting::X11Client(host_jid, username, auth_token);
  client->Run();
}
