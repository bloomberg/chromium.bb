// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_WINDOW_H_
#define REMOTING_HOST_HOST_WINDOW_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"

namespace remoting {

class ClientSessionControl;

class HostWindow : public base::NonThreadSafe {
 public:
  virtual ~HostWindow() {}

  // Creates a platform-specific instance of the continue window.
  static scoped_ptr<HostWindow> CreateContinueWindow();

  // Creates a platform-specific instance of the disconnect window.
  static scoped_ptr<HostWindow> CreateDisconnectWindow();

  // Starts the UI state machine. |client_session_control| will be used to
  // notify the caller about the local user's actions.
  virtual void Start(
      const base::WeakPtr<ClientSessionControl>& client_session_control) = 0;

 protected:
  HostWindow() {}

 private:
  // Let |HostWindowProxy| to call DetachFromThread() when passing an instance
  // of |HostWindow| to a different thread.
  friend class HostWindowProxy;

  DISALLOW_COPY_AND_ASSIGN(HostWindow);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_WINDOW_H_
