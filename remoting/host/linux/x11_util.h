// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_X11_UTIL_H_
#define REMOTING_HOST_LINUX_X11_UTIL_H_

// Xlib.h defines XErrorEvent as an anonymous struct, so we can't forward-
// declare it in this header. Since Xlib.h is not generally something you
// should #include into arbitrary code, please refrain from #including this
// header in another header.
#include <X11/Xlib.h>

#include "base/callback.h"

namespace remoting {

// Temporarily install an alternative handler for X errors. The default handler
// exits the process, which is not what we want.
//
// Note that X error handlers are global, which means that this class is not
// thread safe.
class ScopedXErrorHandler {
 public:
  typedef base::Callback<void(Display*, XErrorEvent*)> Handler;

  explicit ScopedXErrorHandler(const Handler& handler);
  ~ScopedXErrorHandler();

  // Return false if any X errors have been encountered in the scope of this
  // handler.
  bool ok() const { return ok_; }

  // Basic handler that ignores X errors.
  static Handler Ignore();

 private:
  static int HandleXErrors(Display* display, XErrorEvent* error);

  Handler handler_;
  int (*previous_handler_)(Display*, XErrorEvent*);
  bool ok_;

  DISALLOW_COPY_AND_ASSIGN(ScopedXErrorHandler);
};


// Grab/release the X server within a scope. This can help avoid race
// conditions that would otherwise lead to X errors.
class ScopedXGrabServer {
 public:
  ScopedXGrabServer(Display* display);
  ~ScopedXGrabServer();

 private:
  Display* display_;

  DISALLOW_COPY_AND_ASSIGN(ScopedXGrabServer);
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_X11_UTIL_H_
