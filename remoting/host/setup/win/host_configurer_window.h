// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_WIN_HOST_CONFIGURER_WINDOW_H_
#define REMOTING_HOST_SETUP_WIN_HOST_CONFIGURER_WINDOW_H_

// altbase.h must be included before atlapp.h
#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atluser.h>
#include <atlwin.h>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/host/setup/win/host_configurer_resource.h"

namespace remoting {

// This dialog is modeless, so that it can be created before its message loop
// starts to run.
class HostConfigurerWindow : public ATL::CDialogImpl<HostConfigurerWindow> {
 public:
  enum { IDD = IDD_MAIN };

  BEGIN_MSG_MAP_EX(HostConfigurerWindow)
    MSG_WM_INITDIALOG(OnInitDialog)
    MSG_WM_CLOSE(OnClose)
    COMMAND_ID_HANDLER_EX(IDC_START_HOST, OnStartHost)
  END_MSG_MAP()

  HostConfigurerWindow(
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      base::Closure on_destroy);

 private:
  // All methods in this class must be called on the UI thread.
  void OnCancel(UINT code, int id, HWND control);
  void OnClose();
  LRESULT OnInitDialog(HWND wparam, LPARAM lparam);
  void OnOk(UINT code, int id, HWND control);
  void OnStartHost(UINT code, int id, HWND control);

  // Destroys the window and quits the UI message loop.
  void Finish();

  // Sets an appropriate initial position for the dialog.
  void PositionWindow();

  // Context needed to start the host.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  // The UI task runner.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // A task used to notify the caller that this window has been destroyed.
  base::Closure on_destroy_;

  DISALLOW_COPY_AND_ASSIGN(HostConfigurerWindow);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_WIN_HOST_CONFIGURER_WINDOW_H_
