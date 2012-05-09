// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the definition for LayoutTestController.

#include <vector>

#include "webkit/tools/test_shell/layout_test_controller.h"

#include "base/base64.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAnimationController.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDeviceOrientation.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDeviceOrientationClientMock.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGeolocationClientMock.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityPolicy.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSettings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/dom_operations.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/support/simple_database_system.h"
#include "webkit/tools/test_shell/notification_presenter.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_devtools_agent.h"
#include "webkit/tools/test_shell/test_webview_delegate.h"

using std::string;

using WebKit::WebBindings;
using WebKit::WebConsoleMessage;
using WebKit::WebElement;
using WebKit::WebScriptSource;
using WebKit::WebSecurityPolicy;
using WebKit::WebSize;
using WebKit::WebString;
using WebKit::WebURL;
using webkit_glue::CppArgumentList;
using webkit_glue::CppVariant;

TestShell* LayoutTestController::shell_ = NULL;
// Most of these flags need to be cleared in Reset() so that they get turned
// off between each test run.
bool LayoutTestController::accepts_editing_ = true;
bool LayoutTestController::wait_until_done_ = false;
bool LayoutTestController::can_open_windows_ = false;
bool LayoutTestController::close_remaining_windows_ = true;
bool LayoutTestController::stop_provisional_frame_loads_ = false;
LayoutTestController::WorkQueue LayoutTestController::work_queue_;

LayoutTestController::LayoutTestController(TestShell* shell) :
    ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  // Set static shell_ variable since we can't do it in an initializer list.
  // We also need to be careful not to assign shell_ to new windows which are
  // temporary.
  if (NULL == shell_)
    shell_ = shell;

  // Initialize the map that associates methods of this class with the names
  // they will use when called by JavaScript.  The actual binding of those
  // names to their methods will be done by calling BindToJavaScript() (defined
  // by CppBoundClass, the parent to LayoutTestController).
  BindCallback("waitUntilDone",
               base::Bind(&LayoutTestController::waitUntilDone,
                          base::Unretained(this)));
  BindCallback("notifyDone",
               base::Bind(&LayoutTestController::notifyDone,
                          base::Unretained(this)));

  // The fallback method is called when an unknown method is invoked.
  BindFallbackCallback(base::Bind(&LayoutTestController::fallbackMethod,
                                  base::Unretained(this)));
}

LayoutTestController::~LayoutTestController() {
}

LayoutTestController::WorkQueue::WorkQueue() {
}

LayoutTestController::WorkQueue::~WorkQueue() {
  Reset();
}

void LayoutTestController::WorkQueue::ProcessWorkSoon() {
  if (shell_->delegate()->top_loading_frame())
    return;

  if (!queue_.empty()) {
    // We delay processing queued work to avoid recursion problems.
    timer_.Start(FROM_HERE, base::TimeDelta(), this, &WorkQueue::ProcessWork);
  } else if (!wait_until_done_) {
    shell_->TestFinished();
  }
}

void LayoutTestController::WorkQueue::ProcessWork() {
  // Quit doing work once a load is in progress.
  while (!queue_.empty()) {
    bool started_load = queue_.front()->Run(shell_);
    delete queue_.front();
    queue_.pop();

    if (started_load)
      return;
  }

  if (!wait_until_done_ && !shell_->delegate()->top_loading_frame())
    shell_->TestFinished();
}

void LayoutTestController::WorkQueue::Reset() {
  frozen_ = false;
  while (!queue_.empty()) {
    delete queue_.front();
    queue_.pop();
  }
}

void LayoutTestController::WorkQueue::AddWork(WorkItem* work) {
  if (frozen_) {
    delete work;
    return;
  }
  queue_.push(work);
}

void LayoutTestController::waitUntilDone(
    const CppArgumentList& args, CppVariant* result) {
  bool is_debugger_present = false;
#if defined(OS_WIN)
  // TODO(ojan): Make cross-platform.
  is_debugger_present = ::IsDebuggerPresent();
#endif

  if (!is_debugger_present) {
    // TODO(ojan): Use base::OneShotTimer. For some reason, using OneShotTimer
    // seems to cause layout test failures on the try bots.
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&LayoutTestController::notifyDoneTimedOut,
                   weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(shell_->GetLayoutTestTimeout()));
  }

  wait_until_done_ = true;
  result->SetNull();
}

void LayoutTestController::notifyDone(
    const CppArgumentList& args, CppVariant* result) {
  // Test didn't timeout. Kill the timeout timer.
  weak_factory_.InvalidateWeakPtrs();

  completeNotifyDone(false);
  result->SetNull();
}

void LayoutTestController::notifyDoneTimedOut() {
  completeNotifyDone(true);
}

void LayoutTestController::completeNotifyDone(bool is_timeout) {
  if (shell_->layout_test_mode() && wait_until_done_ &&
      !shell_->delegate()->top_loading_frame() && work_queue_.empty()) {
    if (is_timeout)
      shell_->TestTimedOut();
    else
      shell_->TestFinished();
  }
  wait_until_done_ = false;
}

void LayoutTestController::Reset() {
  if (shell_) {
    shell_->webView()->setZoomLevel(false, 0);
    shell_->webView()->setTabKeyCyclesThroughElements(true);
#if defined(TOOLKIT_GTK)
    // (Constants copied because we can't depend on the header that defined
    // them from this file.)
    shell_->webView()->setSelectionColors(
        0xff1e90ff, 0xff000000, 0xffc8c8c8, 0xff323232);
#endif  // defined(TOOLKIT_GTK)
    shell_->webView()->removeAllUserContent();

    // Reset editingBehavior to a reasonable default between tests
    // see http://trac.webkit.org/changeset/60158
#if defined(OS_MACOSX)
    shell_->webView()->settings()->setEditingBehavior(
        WebKit::WebSettings::EditingBehaviorMac);
#else
    shell_->webView()->settings()->setEditingBehavior(
        WebKit::WebSettings::EditingBehaviorWin);
#endif
  }
  accepts_editing_ = true;
  wait_until_done_ = false;
  can_open_windows_ = false;
  stop_provisional_frame_loads_ = false;

  SimpleResourceLoaderBridge::SetAcceptAllCookies(false);
  WebSecurityPolicy::resetOriginAccessWhitelists();

  // Reset the default quota for each origin to 5MB
  SimpleDatabaseSystem::GetInstance()->SetDatabaseQuota(5 * 1024 * 1024);

  setlocale(LC_ALL, "");

  if (close_remaining_windows_) {
    // Iterate through the window list and close everything except the original
    // shell.  We don't want to delete elements as we're iterating, so we copy
    // to a temp vector first.
    WindowList* windows = TestShell::windowList();
    std::vector<gfx::NativeWindow> windows_to_delete;
    for (WindowList::iterator i = windows->begin(); i != windows->end(); ++i) {
      if (*i != shell_->mainWnd())
        windows_to_delete.push_back(*i);
    }
    DCHECK(windows_to_delete.size() + 1 == windows->size());
    for (size_t i = 0; i < windows_to_delete.size(); ++i) {
      TestShell::DestroyWindow(windows_to_delete[i]);
    }
    DCHECK(windows->size() == 1);
  } else {
    // Reset the value
    close_remaining_windows_ = true;
  }
  work_queue_.Reset();
}

void LayoutTestController::LocationChangeDone() {
  // no more new work after the first complete load.
  work_queue_.set_frozen(true);

  if (!wait_until_done_)
    work_queue_.ProcessWorkSoon();
}

void LayoutTestController::PolicyDelegateDone() {
  if (!shell_->layout_test_mode())
    return;

  DCHECK(wait_until_done_);
  shell_->TestFinished();
  wait_until_done_ = false;
}

void LayoutTestController::fallbackMethod(
    const CppArgumentList& args, CppVariant* result) {
  std::string message(
      "JavaScript ERROR: unknown method called on LayoutTestController");
  if (!shell_->layout_test_mode()) {
    logging::LogMessage("CONSOLE:", 0).stream() << message;
  } else {
    printf("CONSOLE MESSAGE: %s\n", message.c_str());
  }
  result->SetNull();
}

void LayoutTestController::LogErrorToConsole(const std::string& text) {
  shell_->delegate()->didAddMessageToConsole(
      WebConsoleMessage(WebConsoleMessage::LevelError,
                        WebString::fromUTF8(text)),
      WebString(), 0);
}
