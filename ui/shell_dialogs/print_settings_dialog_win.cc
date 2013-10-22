// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/print_settings_dialog_win.h"

#include "base/bind.h"
#include "base/threading/thread.h"

#if defined(USE_AURA)
#include "ui/aura/root_window.h"
#endif

namespace ui {

PrintSettingsDialogWin::PrintSettingsDialogWin(
    PrintSettingsDialogWin::Observer* observer)
    : observer_(observer) {
}

PrintSettingsDialogWin::~PrintSettingsDialogWin() {
}

void PrintSettingsDialogWin::GetPrintSettings(PrintDialogFunc print_dialog_func,
                                              HWND owning_window,
                                              PRINTDLGEX* dialog_options) {
  DCHECK(observer_);

  ExecutePrintSettingsParams execute_params(BeginRun(owning_window),
                                            owning_window,
                                            print_dialog_func,
                                            dialog_options);
  execute_params.run_state.dialog_thread->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(
          &PrintSettingsDialogWin::ExecutePrintSettings, this, execute_params));
}

void PrintSettingsDialogWin::ExecutePrintSettings(
    const ExecutePrintSettingsParams& params) {
  HRESULT hr = (*params.print_dialog_func)(params.dialog_options);
  params.ui_proxy->PostTask(
      FROM_HERE,
      base::Bind(
          &PrintSettingsDialogWin::PrintSettingsCompleted, this, hr, params));
}

void PrintSettingsDialogWin::PrintSettingsCompleted(
    HRESULT hresult,
    const ExecutePrintSettingsParams& params) {
  EndRun(params.run_state);
  if (hresult != S_OK)
    observer_->PrintSettingsCancelled(params.dialog_options);
  else
    observer_->PrintSettingsConfirmed(params.dialog_options);
}

}  // namespace ui
