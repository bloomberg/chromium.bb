// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/x11_util.h"

#include "base/bind.h"

namespace remoting {

static ScopedXErrorHandler* g_handler = NULL;

ScopedXErrorHandler::ScopedXErrorHandler(const Handler& handler):
    handler_(handler),
    ok_(true) {
  // This is a poor-man's check for incorrect usage. It doesn't handle the case
  // where a mix of ScopedXErrorHandler and raw XSetErrorHandler calls are used,
  // and it disallows nested ScopedXErrorHandlers on the same thread, despite
  // these being perfectly safe.
  DCHECK(g_handler == NULL);
  g_handler = this;
  previous_handler_ = XSetErrorHandler(HandleXErrors);
}

ScopedXErrorHandler::~ScopedXErrorHandler() {
  g_handler = NULL;
  XSetErrorHandler(previous_handler_);
}

namespace {
void IgnoreXErrors(Display* display, XErrorEvent* error) {}
}  // namespace

// Static
ScopedXErrorHandler::Handler ScopedXErrorHandler::Ignore() {
  return base::Bind(IgnoreXErrors);
}

int ScopedXErrorHandler::HandleXErrors(Display* display, XErrorEvent* error) {
  DCHECK(g_handler != NULL);
  g_handler->ok_ = false;
  g_handler->handler_.Run(display, error);
  return 0;
}


ScopedXGrabServer::ScopedXGrabServer(Display* display)
    : display_(display) {
  XGrabServer(display_);
}

ScopedXGrabServer::~ScopedXGrabServer() {
  XUngrabServer(display_);
  XFlush(display_);
}

}  // namespace remoting
