// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DAEMON_INSTALLER_WIN_H_
#define REMOTING_HOST_DAEMON_INSTALLER_WIN_H_

#include <objbase.h>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"

namespace remoting {

class DaemonInstallerWin {
 public:
  typedef base::Callback<void (HRESULT result)> CompletionCallback;

  virtual ~DaemonInstallerWin();

  // Initiates download and installation of the Chromoting Host.
  virtual void Install() = 0;

  // Creates an instance of the Chromoting Host installer passing the completion
  // callback to be called when the installation finishes. In case of an error
  // returns NULL and passed the error code to |done|.
  static scoped_ptr<DaemonInstallerWin> Create(HWND window_handle,
                                               CompletionCallback done);

 protected:
  DaemonInstallerWin(const CompletionCallback& done);

  // Invokes the completion callback to report the installation result.
  void Done(HRESULT result);

 private:
  // The completion callback that should be called to report the installation
  // result.
  CompletionCallback done_;

  DISALLOW_COPY_AND_ASSIGN(DaemonInstallerWin);
};

// Returns the first top-level (i.e. WS_OVERLAPPED or WS_POPUP) window in
// the chain of parents of |window|. Returns |window| if it represents
// a top-level window. Returns NULL when |window| is NULL.
HWND GetTopLevelWindow(HWND window);

}  // namespace remoting

#endif  // REMOTING_HOST_DAEMON_INSTALLER_WIN_H_
