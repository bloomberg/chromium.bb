// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_TEST_UI_AUTOMATION_CLIENT_H_
#define WIN8_TEST_UI_AUTOMATION_CLIENT_H_

// This file contains UI automation implementation details for the
// OpenWithDialogController. See that class for consumable entrypoints.

#include <windows.h>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"

namespace win8 {
namespace internal {

// An asynchronous UI automation client that waits for the appearance of a
// Window of a particular class and then invokes one of its children identified
// by name.  An instance may be destroyed at any time after Begin() is invoked.
// Any pending operation is cancelled.
class UIAutomationClient {
 public:
  // If the HRESULT argument indicates success, the automation client has been
  // initialized, has installed its Window observer, and is ready to process
  // the named window.  Otherwise, initialization has failed and ResultCallback
  // will not be invoked.
  typedef base::Callback<void(HRESULT)> InitializedCallback;

  // If the HRESULT argument indicates success, the desired item in the window
  // was invoked.  Otherwise, the string vector (if not empty) contains the list
  // of possible items in the window.
  typedef base::Callback<void(HRESULT, std::vector<string16>)> ResultCallback;

  UIAutomationClient();
  ~UIAutomationClient();

  // Starts the client.  Invokes |callback| once the client is ready or upon
  // failure to start, in which case |result_callback| will never be called.
  // Otherwise, |result_callback| will be invoked once |item_name| has been
  // invoked.
  void Begin(const wchar_t* class_name,
             const string16& item_name,
             const InitializedCallback& init_callback,
             const ResultCallback& result_callback);

 private:
  class Context;

  base::ThreadChecker thread_checker_;

  // A thread in the COM MTA in which automation calls are made.
  base::Thread automation_thread_;

  // A pointer to the context object that lives on the automation thread.
  base::WeakPtr<Context> context_;

  DISALLOW_COPY_AND_ASSIGN(UIAutomationClient);
};

}  // namespace internal
}  // namespace win8

#endif  // WIN8_TEST_UI_AUTOMATION_CLIENT_H_
