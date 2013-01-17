// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_TEST_OPEN_WITH_DIALOG_CONTROLLER_H_
#define WIN8_TEST_OPEN_WITH_DIALOG_CONTROLLER_H_

#include <windows.h>

#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"

namespace win8 {

// Asynchronously drives Windows 8's OpenWithDialog into making a given program
// the default handler for an URL protocol.
class OpenWithDialogController {
 public:
  // A callback that is invoked upon completion of the OpenWithDialog
  // interaction. If the HRESULT indicates success, the interaction completed
  // successfully. Otherwise, the vector of strings may contain the list of
  // possible choices if the desired program could not be selected.
  typedef base::Callback<void(HRESULT,
                              std::vector<string16>)> SetDefaultCallback;

  OpenWithDialogController();
  ~OpenWithDialogController();

  // Starts the process of making |program| the default handler for
  // |url_protocol|. |parent_window| may be NULL.  |callback| will be invoked
  // upon completion. This instance may be deleted prior to |callback| being
  // invoked to cancel the operation.
  void Begin(HWND parent_window,
             const string16& url_protocol,
             const string16& program,
             const SetDefaultCallback& callback);

  // Sychronously drives the dialog by running a message loop. Do not by any
  // means call this on a thread that already has a message loop. Returns S_OK
  // on success. Otherwise, |choices| may contain the list of possible choices
  // if the desired program could not be selected.
  HRESULT RunSynchronously(HWND parent_window,
                           const string16& url_protocol,
                           const string16& program,
                           std::vector<string16>* choices);

 private:
  class Context;

  base::WeakPtr<Context> context_;

  DISALLOW_COPY_AND_ASSIGN(OpenWithDialogController);
};

}  // namespace win8

#endif // WIN8_TEST_OPEN_WITH_DIALOG_CONTROLLER_H_
