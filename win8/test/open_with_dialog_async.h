// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_TEST_OPEN_WITH_DIALOG_ASYNC_H_
#define WIN8_TEST_OPEN_WITH_DIALOG_ASYNC_H_

#include <windows.h>

#include "base/callback_forward.h"
#include "base/strings/string16.h"

namespace win8 {

// Expected HRESULTS:
//   S_OK - A choice was made.
//   HRESULT_FROM_WIN32(ERROR_CANCELLED) - The dialog was dismissed.
//   HRESULT_FROM_WIN32(RPC_S_CALL_FAILED) - OpenWith.exe died.
typedef base::Callback<void(HRESULT)> OpenWithDialogCallback;

// Calls SHOpenWithDialog on a dedicated thread, returning the result to the
// caller via |callback| on the current thread.  The Windows SHOpenWithDialog
// function blocks until the user makes a choice or dismisses the dialog (there
// is no natural timeout nor a means by which it can be cancelled).  Note that
// the dedicated thread will be leaked if the calling thread's message loop goes
// away before the interaction completes.
void OpenWithDialogAsync(HWND parent_window,
                         const string16& file_name,
                         const string16& file_type_class,
                         int open_as_info_flags,
                         const OpenWithDialogCallback& callback);

}  // namespace win8

#endif  // WIN8_TEST_OPEN_WITH_DIALOG_ASYNC_H_
