// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_PRINT_SETTINGS_DIALOG_WIN_H_
#define UI_SHELL_DIALOGS_PRINT_SETTINGS_DIALOG_WIN_H_

#include <ocidl.h>
#include <commdlg.h>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "ui/shell_dialogs/base_shell_dialog_win.h"
#include "ui/shell_dialogs/shell_dialogs_export.h"

namespace ui {

// A thin wrapper around the native window print dialog that uses
// BaseShellDialog to have the dialog run on a background thread.
class SHELL_DIALOGS_EXPORT PrintSettingsDialogWin
    : public base::RefCountedThreadSafe<PrintSettingsDialogWin>,
      public BaseShellDialogImpl {
 public:
  class SHELL_DIALOGS_EXPORT Observer {
   public:
    virtual void PrintSettingsConfirmed(PRINTDLGEX* dialog_options) = 0;
    virtual void PrintSettingsCancelled(PRINTDLGEX* dialog_options) = 0;
  };
  typedef HRESULT(__stdcall* PrintDialogFunc)(PRINTDLGEX*);

  explicit PrintSettingsDialogWin(Observer* observer);
  virtual ~PrintSettingsDialogWin();

  // Called to open the system print dialog on a background thread.
  // |print_dialog_func| should generally be ::PrintDlgEx, however alternate
  // functions are used for testing. |owning_window| is the parent HWND, and
  // |dialog_options| is passed to |print_dialog_func|.
  void GetPrintSettings(
      PrintDialogFunc print_dialog_func,
      HWND owning_window,
      PRINTDLGEX* dialog_options);

 private:
  // A struct for holding all the state necessary for displaying the print
  // settings dialog.
  struct ExecutePrintSettingsParams {
    ExecutePrintSettingsParams(RunState run_state,
                               HWND owner,
                               PrintDialogFunc print_dialog_func,
                               PRINTDLGEX* dialog_options)
        : run_state(run_state),
          owner(owner),
          print_dialog_func(print_dialog_func),
          dialog_options(dialog_options),
          ui_proxy(base::MessageLoopForUI::current()->message_loop_proxy()) {}

    RunState run_state;
    HWND owner;
    PrintDialogFunc print_dialog_func;
    PRINTDLGEX* dialog_options;
    scoped_refptr<base::MessageLoopProxy> ui_proxy;
  };

  // Runs the print dialog. Should be run on the the BaseShellDialogImpl thread.
  // Posts back to PrintSettingsCompleted on the UI thread on completion.
  void ExecutePrintSettings(const ExecutePrintSettingsParams& params);

  // Handler for the result of the print settings dialog. Should be run on the
  // UI thread, and notifies the observer of the result of the dialog.
  void PrintSettingsCompleted(HRESULT hresult,
                              const ExecutePrintSettingsParams& params);

  // Observer that's notified when the dialog is confirmed or cancelled.
  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(PrintSettingsDialogWin);
};

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_PRINT_SETTINGS_DIALOG_WIN_H_
