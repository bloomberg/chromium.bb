// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win8/test/open_with_dialog_controller.h"

#include <shlobj.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "base/win/windows_version.h"
#include "win8/test/open_with_dialog_async.h"
#include "win8/test/ui_automation_client.h"

namespace win8 {

namespace {

const int kControllerTimeoutSeconds = 5;
const wchar_t kShellFlyoutClassName[] = L"Shell_Flyout";

// A callback invoked with the OpenWithDialogController's results. Said results
// are copied to |result_out| and |choices_out| and then |closure| is invoked.
// This function is in support of OpenWithDialogController::RunSynchronously.
void OnMakeDefaultComplete(
    const base::Closure& closure,
    HRESULT* result_out,
    std::vector<string16>* choices_out,
    HRESULT hr,
    std::vector<string16> choices) {
  *result_out = hr;
  *choices_out = choices;
  closure.Run();
}

}  // namespace

// Lives on the main thread and is owned by a controller. May outlive the
// controller (see Orphan).
class OpenWithDialogController::Context {
 public:
  Context();
  ~Context();

  base::WeakPtr<Context> AsWeakPtr();

  void Orphan();

  void Begin(HWND parent_window,
             const string16& url_protocol,
             const string16& program_name,
             const OpenWithDialogController::SetDefaultCallback& callback);

 private:
  enum State {
    // The Context has been constructed.
    CONTEXT_INITIALIZED,
    // The UI automation event handler is ready.
    CONTEXT_AUTOMATION_READY,
    // The automation results came back before the call to SHOpenWithDialog.
    CONTEXT_WAITING_FOR_DIALOG,
    // The call to SHOpenWithDialog returned before automation results.
    CONTEXT_WAITING_FOR_RESULTS,
    CONTEXT_FINISHED,
  };

  // Invokes the client's callback and destroys this instance.
  void NotifyClientAndDie();

  void OnTimeout();
  void OnInitialized(HRESULT result);
  void OnAutomationResult(HRESULT result, std::vector<string16> choices);
  void OnOpenWithComplete(HRESULT result);

  base::ThreadChecker thread_checker_;
  State state_;
  internal::UIAutomationClient automation_client_;
  HWND parent_window_;
  string16 file_name_;
  string16 file_type_class_;
  int open_as_info_flags_;
  OpenWithDialogController::SetDefaultCallback callback_;
  HRESULT open_with_result_;
  HRESULT automation_result_;
  std::vector<string16> automation_choices_;
  base::WeakPtrFactory<Context> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(OpenWithDialogController::Context);
};

OpenWithDialogController::Context::Context()
    : state_(CONTEXT_INITIALIZED),
      parent_window_(),
      open_as_info_flags_(),
      open_with_result_(E_FAIL),
      automation_result_(E_FAIL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {}

OpenWithDialogController::Context::~Context() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

base::WeakPtr<OpenWithDialogController::Context>
    OpenWithDialogController::Context::AsWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_ptr_factory_.GetWeakPtr();
}

void OpenWithDialogController::Context::Orphan() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // The controller is being destroyed. Its client is no longer interested in
  // having the interaction continue.
  DLOG_IF(WARNING, (state_ == CONTEXT_AUTOMATION_READY ||
                    state_ == CONTEXT_WAITING_FOR_DIALOG))
      << "Abandoning the OpenWithDialog.";
  delete this;
}

void OpenWithDialogController::Context::Begin(
    HWND parent_window,
    const string16& url_protocol,
    const string16& program_name,
    const OpenWithDialogController::SetDefaultCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  parent_window_ = parent_window;
  file_name_ = url_protocol;
  file_type_class_.clear();
  open_as_info_flags_ = (OAIF_URL_PROTOCOL | OAIF_FORCE_REGISTRATION |
                         OAIF_REGISTER_EXT);
  callback_ = callback;

  // Post a delayed callback to abort the operation if it takes too long.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&OpenWithDialogController::Context::OnTimeout, AsWeakPtr()),
      base::TimeDelta::FromSeconds(kControllerTimeoutSeconds));

  automation_client_.Begin(
      kShellFlyoutClassName,
      program_name,
      base::Bind(&OpenWithDialogController::Context::OnInitialized,
                 AsWeakPtr()),
      base::Bind(&OpenWithDialogController::Context::OnAutomationResult,
                 AsWeakPtr()));
}

void OpenWithDialogController::Context::NotifyClientAndDie() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, CONTEXT_FINISHED);
  DLOG_IF(WARNING, SUCCEEDED(automation_result_) && FAILED(open_with_result_))
      << "Automation succeeded, yet SHOpenWithDialog failed.";

  // Ignore any future callbacks (such as the timeout) or calls to Orphan.
  weak_ptr_factory_.InvalidateWeakPtrs();
  callback_.Run(automation_result_, automation_choices_);
  delete this;
}

void OpenWithDialogController::Context::OnTimeout() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // This is a LOG rather than a DLOG since it represents something that needs
  // to be investigated and fixed.
  LOG(ERROR) << __FUNCTION__ " state: " << state_;

  state_ = CONTEXT_FINISHED;
  NotifyClientAndDie();
}

void OpenWithDialogController::Context::OnInitialized(HRESULT result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, CONTEXT_INITIALIZED);
  if (FAILED(result)) {
    automation_result_ = result;
    state_ = CONTEXT_FINISHED;
    NotifyClientAndDie();
    return;
  }
  state_ = CONTEXT_AUTOMATION_READY;
  OpenWithDialogAsync(
      parent_window_, file_name_, file_type_class_, open_as_info_flags_,
      base::Bind(&OpenWithDialogController::Context::OnOpenWithComplete,
                 weak_ptr_factory_.GetWeakPtr()));
}

void OpenWithDialogController::Context::OnAutomationResult(
    HRESULT result,
    std::vector<string16> choices) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(automation_result_, E_FAIL);

  automation_result_ = result;
  automation_choices_ = choices;
  switch (state_) {
    case CONTEXT_AUTOMATION_READY:
      // The results of automation are in and we're waiting for
      // SHOpenWithDialog to return.
      state_ = CONTEXT_WAITING_FOR_DIALOG;
      break;
    case CONTEXT_WAITING_FOR_RESULTS:
      state_ = CONTEXT_FINISHED;
      NotifyClientAndDie();
      break;
    default:
      NOTREACHED() << state_;
  }
}

void OpenWithDialogController::Context::OnOpenWithComplete(HRESULT result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(open_with_result_, E_FAIL);

  open_with_result_ = result;
  switch (state_) {
    case CONTEXT_AUTOMATION_READY:
      // The interaction completed and we're waiting for the results from the
      // automation side to come in.
      state_ = CONTEXT_WAITING_FOR_RESULTS;
      break;
    case CONTEXT_WAITING_FOR_DIALOG:
      // All results are in.  Invoke the caller's callback.
      state_ = CONTEXT_FINISHED;
      NotifyClientAndDie();
      break;
    default:
      NOTREACHED() << state_;
  }
}

OpenWithDialogController::OpenWithDialogController() {}

OpenWithDialogController::~OpenWithDialogController() {
  // Orphan the context if this instance is being destroyed before the context
  // finishes its work.
  if (context_)
    context_->Orphan();
}

void OpenWithDialogController::Begin(
    HWND parent_window,
    const string16& url_protocol,
    const string16& program,
    const SetDefaultCallback& callback) {
  DCHECK_EQ(context_, static_cast<Context*>(NULL));
  if (base::win::GetVersion() < base::win::VERSION_WIN8) {
    NOTREACHED() << "Windows 8 is required.";
    // The callback may not properly handle being run from Begin, so post a task
    // to this thread's task runner to call it.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, E_FAIL, std::vector<string16>()));
    return;
  }

  context_ = (new Context())->AsWeakPtr();
  context_->Begin(parent_window, url_protocol, program, callback);
}

HRESULT OpenWithDialogController::RunSynchronously(
    HWND parent_window,
    const string16& protocol,
    const string16& program,
    std::vector<string16>* choices) {
  DCHECK_EQ(MessageLoop::current(), static_cast<MessageLoop*>(NULL));
  if (base::win::GetVersion() < base::win::VERSION_WIN8) {
    NOTREACHED() << "Windows 8 is required.";
    return E_FAIL;
  }

  HRESULT result = S_OK;
  MessageLoop message_loop;
  base::RunLoop run_loop;

  message_loop.PostTask(
      FROM_HERE,
      base::Bind(&OpenWithDialogController::Begin, base::Unretained(this),
                 parent_window, protocol, program,
                 Bind(&OnMakeDefaultComplete, run_loop.QuitClosure(),
                      &result, choices)));

  run_loop.Run();
  return result;
}

}  // namespace win8
