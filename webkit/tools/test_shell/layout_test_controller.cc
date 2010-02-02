// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the definition for LayoutTestController.

#include <vector>

#include "webkit/tools/test_shell/layout_test_controller.h"

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "third_party/WebKit/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScriptSource.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSecurityPolicy.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"
#include "webkit/glue/dom_operations.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/tools/test_shell/simple_database_system.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_devtools_agent.h"

using std::string;
using std::wstring;

using WebKit::WebConsoleMessage;
using WebKit::WebScriptSource;
using WebKit::WebSecurityPolicy;
using WebKit::WebString;

TestShell* LayoutTestController::shell_ = NULL;
// Most of these flags need to be cleared in Reset() so that they get turned
// off between each test run.
bool LayoutTestController::dump_as_text_ = false;
bool LayoutTestController::dump_editing_callbacks_ = false;
bool LayoutTestController::dump_frame_load_callbacks_ = false;
bool LayoutTestController::dump_resource_load_callbacks_ = false;
bool LayoutTestController::dump_back_forward_list_ = false;
bool LayoutTestController::dump_child_frame_scroll_positions_ = false;
bool LayoutTestController::dump_child_frames_as_text_ = false;
bool LayoutTestController::dump_window_status_changes_ = false;
bool LayoutTestController::dump_title_changes_ = false;
bool LayoutTestController::accepts_editing_ = true;
bool LayoutTestController::wait_until_done_ = false;
bool LayoutTestController::can_open_windows_ = false;
bool LayoutTestController::close_remaining_windows_ = true;
bool LayoutTestController::test_repaint_ = false;
bool LayoutTestController::sweep_horizontally_ = false;
bool LayoutTestController::should_add_file_to_pasteboard_ = false;
bool LayoutTestController::stop_provisional_frame_loads_ = false;
LayoutTestController::WorkQueue LayoutTestController::work_queue_;
CppVariant LayoutTestController::globalFlag_;
CppVariant LayoutTestController::webHistoryItemCount_;

LayoutTestController::LayoutTestController(TestShell* shell) :
    ALLOW_THIS_IN_INITIALIZER_LIST(timeout_factory_(this)) {
  // Set static shell_ variable since we can't do it in an initializer list.
  // We also need to be careful not to assign shell_ to new windows which are
  // temporary.
  if (NULL == shell_)
    shell_ = shell;

  // Initialize the map that associates methods of this class with the names
  // they will use when called by JavaScript.  The actual binding of those
  // names to their methods will be done by calling BindToJavaScript() (defined
  // by CppBoundClass, the parent to LayoutTestController).
  BindMethod("dumpAsText", &LayoutTestController::dumpAsText);
  BindMethod("dumpChildFrameScrollPositions", &LayoutTestController::dumpChildFrameScrollPositions);
  BindMethod("dumpChildFramesAsText", &LayoutTestController::dumpChildFramesAsText);
  BindMethod("dumpDatabaseCallbacks", &LayoutTestController::dumpDatabaseCallbacks);
  BindMethod("dumpEditingCallbacks", &LayoutTestController::dumpEditingCallbacks);
  BindMethod("dumpBackForwardList", &LayoutTestController::dumpBackForwardList);
  BindMethod("dumpFrameLoadCallbacks", &LayoutTestController::dumpFrameLoadCallbacks);
  BindMethod("dumpResourceLoadCallbacks", &LayoutTestController::dumpResourceLoadCallbacks);
  BindMethod("dumpStatusCallbacks", &LayoutTestController::dumpWindowStatusChanges);
  BindMethod("dumpTitleChanges", &LayoutTestController::dumpTitleChanges);
  BindMethod("setAcceptsEditing", &LayoutTestController::setAcceptsEditing);
  BindMethod("waitUntilDone", &LayoutTestController::waitUntilDone);
  BindMethod("notifyDone", &LayoutTestController::notifyDone);
  BindMethod("queueReload", &LayoutTestController::queueReload);
  BindMethod("queueLoadingScript", &LayoutTestController::queueLoadingScript);
  BindMethod("queueNonLoadingScript", &LayoutTestController::queueNonLoadingScript);
  BindMethod("queueLoad", &LayoutTestController::queueLoad);
  BindMethod("queueBackNavigation", &LayoutTestController::queueBackNavigation);
  BindMethod("queueForwardNavigation", &LayoutTestController::queueForwardNavigation);
  BindMethod("windowCount", &LayoutTestController::windowCount);
  BindMethod("setCanOpenWindows", &LayoutTestController::setCanOpenWindows);
  BindMethod("setCloseRemainingWindowsWhenComplete", &LayoutTestController::setCloseRemainingWindowsWhenComplete);
  BindMethod("objCIdentityIsEqual", &LayoutTestController::objCIdentityIsEqual);
  BindMethod("setAlwaysAcceptCookies", &LayoutTestController::setAlwaysAcceptCookies);
  BindMethod("setWindowIsKey", &LayoutTestController::setWindowIsKey);
  BindMethod("setTabKeyCyclesThroughElements", &LayoutTestController::setTabKeyCyclesThroughElements);
  BindMethod("setUserStyleSheetLocation", &LayoutTestController::setUserStyleSheetLocation);
  BindMethod("setUserStyleSheetEnabled", &LayoutTestController::setUserStyleSheetEnabled);
  BindMethod("pathToLocalResource", &LayoutTestController::pathToLocalResource);
  BindMethod("addFileToPasteboardOnDrag", &LayoutTestController::addFileToPasteboardOnDrag);
  BindMethod("execCommand", &LayoutTestController::execCommand);
  BindMethod("isCommandEnabled", &LayoutTestController::isCommandEnabled);
  BindMethod("setPopupBlockingEnabled", &LayoutTestController::setPopupBlockingEnabled);
  BindMethod("setStopProvisionalFrameLoads", &LayoutTestController::setStopProvisionalFrameLoads);
  BindMethod("setSmartInsertDeleteEnabled", &LayoutTestController::setSmartInsertDeleteEnabled);
  BindMethod("setSelectTrailingWhitespaceEnabled", &LayoutTestController::setSelectTrailingWhitespaceEnabled);
  BindMethod("pauseAnimationAtTimeOnElementWithId", &LayoutTestController::pauseAnimationAtTimeOnElementWithId);
  BindMethod("pauseTransitionAtTimeOnElementWithId", &LayoutTestController::pauseTransitionAtTimeOnElementWithId);
  BindMethod("elementDoesAutoCompleteForElementWithId", &LayoutTestController::elementDoesAutoCompleteForElementWithId);
  BindMethod("numberOfActiveAnimations", &LayoutTestController::numberOfActiveAnimations);
  BindMethod("disableImageLoading", &LayoutTestController::disableImageLoading);
  BindMethod("setIconDatabaseEnabled", &LayoutTestController::setIconDatabaseEnabled);
  BindMethod("setCustomPolicyDelegate", &LayoutTestController::setCustomPolicyDelegate);
  BindMethod("waitForPolicyDelegate", &LayoutTestController::waitForPolicyDelegate);
  BindMethod("setWillSendRequestReturnsNullOnRedirect", &LayoutTestController::setWillSendRequestReturnsNullOnRedirect);
  BindMethod("setWillSendRequestReturnsNull", &LayoutTestController::setWillSendRequestReturnsNull);
  BindMethod("whiteListAccessFromOrigin", &LayoutTestController::whiteListAccessFromOrigin);
  BindMethod("clearAllDatabases", &LayoutTestController::clearAllDatabases);
  BindMethod("setDatabaseQuota", &LayoutTestController::setDatabaseQuota);
  BindMethod("setPOSIXLocale", &LayoutTestController::setPOSIXLocale);
  BindMethod("counterValueForElementById", &LayoutTestController::counterValueForElementById);
  BindMethod("addUserScript", &LayoutTestController::addUserScript);

  // The following are stubs.
  BindMethod("dumpAsWebArchive", &LayoutTestController::dumpAsWebArchive);
  BindMethod("setMainFrameIsFirstResponder", &LayoutTestController::setMainFrameIsFirstResponder);
  BindMethod("dumpSelectionRect", &LayoutTestController::dumpSelectionRect);
  BindMethod("display", &LayoutTestController::display);
  BindMethod("testRepaint", &LayoutTestController::testRepaint);
  BindMethod("repaintSweepHorizontally", &LayoutTestController::repaintSweepHorizontally);
  BindMethod("clearBackForwardList", &LayoutTestController::clearBackForwardList);
  BindMethod("keepWebHistory", &LayoutTestController::keepWebHistory);
  BindMethod("storeWebScriptObject", &LayoutTestController::storeWebScriptObject);
  BindMethod("accessStoredWebScriptObject", &LayoutTestController::accessStoredWebScriptObject);
  BindMethod("objCClassNameOf", &LayoutTestController::objCClassNameOf);
  BindMethod("addDisallowedURL", &LayoutTestController::addDisallowedURL);
  BindMethod("setCallCloseOnWebViews", &LayoutTestController::setCallCloseOnWebViews);
  BindMethod("setPrivateBrowsingEnabled", &LayoutTestController::setPrivateBrowsingEnabled);
  BindMethod("setUseDashboardCompatibilityMode", &LayoutTestController::setUseDashboardCompatibilityMode);

  BindMethod("setXSSAuditorEnabled", &LayoutTestController::setXSSAuditorEnabled);
  BindMethod("evaluateScriptInIsolatedWorld", &LayoutTestController::evaluateScriptInIsolatedWorld);
  BindMethod("overridePreference", &LayoutTestController::overridePreference);
  BindMethod("setAllowUniversalAccessFromFileURLs", &LayoutTestController::setAllowUniversalAccessFromFileURLs);
  BindMethod("setTimelineProfilingEnabled", &LayoutTestController::setTimelineProfilingEnabled);
  BindMethod("evaluateInWebInspector", &LayoutTestController::evaluateInWebInspector);
  BindMethod("forceRedSelectionColors", &LayoutTestController::forceRedSelectionColors);

  // The fallback method is called when an unknown method is invoked.
  BindFallbackMethod(&LayoutTestController::fallbackMethod);

  // Shared properties.
  // globalFlag is used by a number of layout tests in
  // LayoutTests\http\tests\security\dataURL.
  BindProperty("globalFlag", &globalFlag_);
  // webHistoryItemCount is used by tests in LayoutTests\http\tests\history
  BindProperty("webHistoryItemCount", &webHistoryItemCount_);
}

LayoutTestController::WorkQueue::~WorkQueue() {
  Reset();
}

void LayoutTestController::WorkQueue::ProcessWorkSoon() {
  if (shell_->delegate()->top_loading_frame())
    return;

  if (!queue_.empty()) {
    // We delay processing queued work to avoid recursion problems.
    timer_.Start(base::TimeDelta(), this, &WorkQueue::ProcessWork);
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

void LayoutTestController::dumpAsText(const CppArgumentList& args,
                                                   CppVariant* result) {
  dump_as_text_ = true;
  result->SetNull();
}

void LayoutTestController::dumpDatabaseCallbacks(
    const CppArgumentList& args, CppVariant* result) {
  // Do nothing; we don't use this flag anywhere for now
  result->SetNull();
}

void LayoutTestController::dumpEditingCallbacks(
    const CppArgumentList& args, CppVariant* result) {
  dump_editing_callbacks_ = true;
  result->SetNull();
}

void LayoutTestController::dumpBackForwardList(
    const CppArgumentList& args, CppVariant* result) {
  dump_back_forward_list_ = true;
  result->SetNull();
}

void LayoutTestController::dumpFrameLoadCallbacks(
    const CppArgumentList& args, CppVariant* result) {
  dump_frame_load_callbacks_ = true;
  result->SetNull();
}

void LayoutTestController::dumpResourceLoadCallbacks(
    const CppArgumentList& args, CppVariant* result) {
  dump_resource_load_callbacks_ = true;
  result->SetNull();
}

void LayoutTestController::dumpChildFrameScrollPositions(
    const CppArgumentList& args, CppVariant* result) {
  dump_child_frame_scroll_positions_ = true;
  result->SetNull();
}

void LayoutTestController::dumpChildFramesAsText(
    const CppArgumentList& args, CppVariant* result) {
  dump_child_frames_as_text_ = true;
  result->SetNull();
}

void LayoutTestController::dumpWindowStatusChanges(
    const CppArgumentList& args, CppVariant* result) {
  dump_window_status_changes_ = true;
  result->SetNull();
}

void LayoutTestController::dumpTitleChanges(
    const CppArgumentList& args, CppVariant* result) {
  dump_title_changes_ = true;
  result->SetNull();
}

void LayoutTestController::setAcceptsEditing(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isBool())
    accepts_editing_ = args[0].value.boolValue;
  result->SetNull();
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
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        timeout_factory_.NewRunnableMethod(
            &LayoutTestController::notifyDoneTimedOut),
        shell_->GetLayoutTestTimeout());
  }

  wait_until_done_ = true;
  result->SetNull();
}

void LayoutTestController::notifyDone(
    const CppArgumentList& args, CppVariant* result) {
  // Test didn't timeout. Kill the timeout timer.
  timeout_factory_.RevokeAll();

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

class WorkItemBackForward : public LayoutTestController::WorkItem {
 public:
  WorkItemBackForward(int distance) : distance_(distance) {}
  bool Run(TestShell* shell) {
    shell->GoBackOrForward(distance_);
    return true;  // TODO(darin): Did it really start a navigation?
  }
 private:
  int distance_;
};

void LayoutTestController::queueBackNavigation(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isNumber())
    work_queue_.AddWork(new WorkItemBackForward(-args[0].ToInt32()));
  result->SetNull();
}

void LayoutTestController::queueForwardNavigation(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isNumber())
    work_queue_.AddWork(new WorkItemBackForward(args[0].ToInt32()));
  result->SetNull();
}

class WorkItemReload : public LayoutTestController::WorkItem {
 public:
  bool Run(TestShell* shell) {
    shell->Reload();
    return true;
  }
};

void LayoutTestController::queueReload(
    const CppArgumentList& args, CppVariant* result) {
  work_queue_.AddWork(new WorkItemReload);
  result->SetNull();
}

class WorkItemLoadingScript : public LayoutTestController::WorkItem {
 public:
  WorkItemLoadingScript(const string& script) : script_(script) {}
  bool Run(TestShell* shell) {
    shell->webView()->mainFrame()->executeScript(
        WebScriptSource(WebString::fromUTF8(script_)));
    return true;  // TODO(darin): Did it really start a navigation?
  }
 private:
  string script_;
};

class WorkItemNonLoadingScript : public LayoutTestController::WorkItem {
 public:
  WorkItemNonLoadingScript(const string& script) : script_(script) {}
  bool Run(TestShell* shell) {
    shell->webView()->mainFrame()->executeScript(
        WebScriptSource(WebString::fromUTF8(script_)));
    return false;
  }
 private:
  string script_;
};

void LayoutTestController::queueLoadingScript(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isString())
    work_queue_.AddWork(new WorkItemLoadingScript(args[0].ToString()));
  result->SetNull();
}

void LayoutTestController::queueNonLoadingScript(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isString())
    work_queue_.AddWork(new WorkItemNonLoadingScript(args[0].ToString()));
  result->SetNull();
}

class WorkItemLoad : public LayoutTestController::WorkItem {
 public:
  WorkItemLoad(const GURL& url, const string& target)
      : url_(url), target_(target) {}
  bool Run(TestShell* shell) {
    shell->LoadURLForFrame(url_, UTF8ToWide(target_).c_str());
    return true;  // TODO(darin): Did it really start a navigation?
  }
 private:
  GURL url_;
  string target_;
};

void LayoutTestController::queueLoad(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isString()) {
    GURL current_url = shell_->webView()->mainFrame()->url();
    GURL full_url = current_url.Resolve(args[0].ToString());

    string target = "";
    if (args.size() > 1 && args[1].isString())
      target = args[1].ToString();

    work_queue_.AddWork(new WorkItemLoad(full_url, target));
  }
  result->SetNull();
}

void LayoutTestController::objCIdentityIsEqual(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() < 2) {
    // This is the best we can do to return an error.
    result->SetNull();
    return;
  }
  result->Set(args[0].isEqual(args[1]));
}

void LayoutTestController::Reset() {
  if (shell_) {
    shell_->webView()->setZoomLevel(false, 0);
    shell_->webView()->setTabKeyCyclesThroughElements(true);
#if defined(OS_LINUX)
    // (Constants copied because we can't depend on the header that defined
    // them from this file.)
    shell_->webView()->setSelectionColors(
        0xff1e90ff, 0xff000000, 0xffc8c8c8, 0xff323232);
#endif  // defined(OS_LINUX)
    shell_->webView()->removeAllUserContent();
  }
  dump_as_text_ = false;
  dump_editing_callbacks_ = false;
  dump_frame_load_callbacks_ = false;
  dump_resource_load_callbacks_ = false;
  dump_back_forward_list_ = false;
  dump_child_frame_scroll_positions_ = false;
  dump_child_frames_as_text_ = false;
  dump_window_status_changes_ = false;
  dump_title_changes_ = false;
  accepts_editing_ = true;
  wait_until_done_ = false;
  can_open_windows_ = false;
  test_repaint_ = false;
  sweep_horizontally_ = false;
  should_add_file_to_pasteboard_ = false;
  stop_provisional_frame_loads_ = false;
  globalFlag_.Set(false);
  webHistoryItemCount_.Set(0);

  SimpleResourceLoaderBridge::SetAcceptAllCookies(false);
  WebSecurityPolicy::resetOriginAccessWhiteLists();

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
  webHistoryItemCount_.Set(shell_->navigation_controller()->GetEntryCount());

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

void LayoutTestController::setCanOpenWindows(
    const CppArgumentList& args, CppVariant* result) {
  can_open_windows_ = true;
  result->SetNull();
}

void LayoutTestController::setTabKeyCyclesThroughElements(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isBool()) {
    shell_->webView()->setTabKeyCyclesThroughElements(args[0].ToBoolean());
  }
  result->SetNull();
}

void LayoutTestController::windowCount(
    const CppArgumentList& args, CppVariant* result) {
  int num_windows = static_cast<int>(TestShell::windowList()->size());
  result->Set(num_windows);
}

void LayoutTestController::setCloseRemainingWindowsWhenComplete(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isBool()) {
    close_remaining_windows_ = args[0].value.boolValue;
  }
  result->SetNull();
}

void LayoutTestController::setAlwaysAcceptCookies(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0) {
    SimpleResourceLoaderBridge::SetAcceptAllCookies(CppVariantToBool(args[0]));
  }
  result->SetNull();
}

void LayoutTestController::setWindowIsKey(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isBool()) {
    shell_->SetFocus(shell_->webViewHost(), args[0].value.boolValue);
  }
  result->SetNull();
}

void LayoutTestController::setUserStyleSheetEnabled(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isBool()) {
    shell_->delegate()->SetUserStyleSheetEnabled(args[0].value.boolValue);
  }

  result->SetNull();
}

void LayoutTestController::setUserStyleSheetLocation(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isString()) {
    GURL location(TestShell::RewriteLocalUrl(args[0].ToString()));
    shell_->delegate()->SetUserStyleSheetLocation(location);
  }

  result->SetNull();
}

void LayoutTestController::execCommand(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isString()) {
    std::string command = args[0].ToString();
    std::string value("");

    // Ignore the second parameter (which is userInterface)
    // since this command emulates a manual action.
    if (args.size() >= 3 && args[2].isString())
      value = args[2].ToString();

    // Note: webkit's version does not return the boolean, so neither do we.
    shell_->webView()->focusedFrame()->executeCommand(
        WebString::fromUTF8(command), WebString::fromUTF8(value));
  }
  result->SetNull();
}

void LayoutTestController::isCommandEnabled(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() <= 0 || !args[0].isString()) {
    result->SetNull();
    return;
  }

  std::string command = args[0].ToString();
  bool rv = shell_->webView()->focusedFrame()->isCommandEnabled(
      WebString::fromUTF8(command));
  result->Set(rv);
}

void LayoutTestController::setPopupBlockingEnabled(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isBool()) {
    bool block_popups = args[0].ToBoolean();
    WebPreferences* prefs = shell_->GetWebPreferences();
    prefs->javascript_can_open_windows_automatically = !block_popups;
    prefs->Apply(shell_->webView());
  }
  result->SetNull();
}

void LayoutTestController::setUseDashboardCompatibilityMode(
    const CppArgumentList& args, CppVariant* result) {
  // We have no need to support Dashboard Compatibility Mode (mac-only)
  result->SetNull();
}

void LayoutTestController::setCustomPolicyDelegate(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isBool()) {
    bool enable = args[0].value.boolValue;
    bool permissive = false;
    if (args.size() > 1 && args[1].isBool())
      permissive = args[1].value.boolValue;
    shell_->delegate()->SetCustomPolicyDelegate(enable, permissive);
  }

  result->SetNull();
}

void LayoutTestController::waitForPolicyDelegate(
    const CppArgumentList& args, CppVariant* result) {
  shell_->delegate()->WaitForPolicyDelegate();
  wait_until_done_ = true;
  result->SetNull();
}

void LayoutTestController::setWillSendRequestReturnsNullOnRedirect(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isBool())
    shell_->delegate()->set_block_redirects(args[0].value.boolValue);

  result->SetNull();
}

void LayoutTestController::setWillSendRequestReturnsNull(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isBool())
    shell_->delegate()->set_request_return_null(args[0].value.boolValue);

  result->SetNull();
}

void LayoutTestController::pathToLocalResource(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
  if (args.size() <= 0 || !args[0].isString())
    return;

  std::string url = args[0].ToString();
  if (StartsWithASCII(url, "/tmp/", true)) {
    // We want a temp file.
    FilePath path;
    PathService::Get(base::DIR_TEMP, &path);
    path = path.AppendASCII(url.substr(5));
    result->Set(WideToUTF8(path.ToWStringHack()));
    return;
  }

  // Some layout tests use file://// which we resolve as a UNC path.  Normalize
  // them to just file:///.
  while (StartsWithASCII(url, "file:////", false)) {
    url = url.substr(0, 8) + url.substr(9);
  }
  GURL location(TestShell::RewriteLocalUrl(url));
  result->Set(location.spec());
}

void LayoutTestController::addFileToPasteboardOnDrag(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
  should_add_file_to_pasteboard_ = true;
}

void LayoutTestController::setStopProvisionalFrameLoads(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
  stop_provisional_frame_loads_ = true;
}

void LayoutTestController::setSmartInsertDeleteEnabled(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isBool()) {
    shell_->delegate()->SetSmartInsertDeleteEnabled(args[0].value.boolValue);
  }

  result->SetNull();
}

void LayoutTestController::setSelectTrailingWhitespaceEnabled(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isBool()) {
    shell_->delegate()->SetSelectTrailingWhitespaceEnabled(
        args[0].value.boolValue);
  }

  result->SetNull();
}

void LayoutTestController::pauseAnimationAtTimeOnElementWithId(
    const CppArgumentList& args,
    CppVariant* result) {
  if (args.size() > 2 && args[0].isString() && args[1].isNumber() &&
      args[2].isString()) {
    std::string animation_name = args[0].ToString();
    double time = args[1].ToDouble();
    std::string element_id = args[2].ToString();

    result->Set(
        webkit_glue::PauseAnimationAtTimeOnElementWithId(
            shell_->webView(), animation_name, time, element_id));
  } else {
    result->Set(false);
  }
}

void LayoutTestController::pauseTransitionAtTimeOnElementWithId(
    const CppArgumentList& args,
    CppVariant* result) {
  if (args.size() > 2 && args[0].isString() && args[1].isNumber() &&
      args[2].isString()) {
    std::string property_name = args[0].ToString();
    double time = args[1].ToDouble();
    std::string element_id = args[2].ToString();

    result->Set(webkit_glue::PauseTransitionAtTimeOnElementWithId(
        shell_->webView(), property_name, time, element_id));
  } else {
    result->Set(false);
  }
}

void LayoutTestController::elementDoesAutoCompleteForElementWithId(
    const CppArgumentList& args,
    CppVariant* result) {
  if (args.size() != 1 || !args[0].isString()) {
    result->Set(false);
    return;
  }
  std::string element_id = args[0].ToString();
  result->Set(webkit_glue::ElementDoesAutoCompleteForElementWithId(
      shell_->webView(), element_id));
}

void LayoutTestController::numberOfActiveAnimations(const CppArgumentList& args,
                                                    CppVariant* result) {
  result->Set(webkit_glue::NumberOfActiveAnimations(shell_->webView()));
}

void LayoutTestController::disableImageLoading(const CppArgumentList& args,
                                               CppVariant* result) {
  WebPreferences* prefs = shell_->GetWebPreferences();
  prefs->loads_images_automatically = false;
  prefs->Apply(shell_->webView());
  result->SetNull();
}

void LayoutTestController::setIconDatabaseEnabled(const CppArgumentList& args,
                                                  CppVariant* result) {
  // We don't use the WebKit icon database.
  result->SetNull();
}

//
// Unimplemented stubs
//

void LayoutTestController::dumpAsWebArchive(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void LayoutTestController::setMainFrameIsFirstResponder(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void LayoutTestController::dumpSelectionRect(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void LayoutTestController::display(
    const CppArgumentList& args, CppVariant* result) {
  WebViewHost* view_host = shell_->webViewHost();
  const WebKit::WebSize& size = view_host->webview()->size();
  gfx::Rect rect(size.width, size.height);
  view_host->UpdatePaintRect(rect);
  view_host->Paint();
  view_host->DisplayRepaintMask();
  result->SetNull();
}

void LayoutTestController::testRepaint(
    const CppArgumentList& args, CppVariant* result) {
  test_repaint_ = true;
  result->SetNull();
}

void LayoutTestController::repaintSweepHorizontally(
    const CppArgumentList& args, CppVariant* result) {
  sweep_horizontally_ = true;
  result->SetNull();
}

void LayoutTestController::clearBackForwardList(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void LayoutTestController::keepWebHistory(
    const CppArgumentList& args,  CppVariant* result) {
  result->SetNull();
}

void LayoutTestController::storeWebScriptObject(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void LayoutTestController::accessStoredWebScriptObject(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void LayoutTestController::objCClassNameOf(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}
void LayoutTestController::addDisallowedURL(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}
void LayoutTestController::setCallCloseOnWebViews(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}
void LayoutTestController::setPrivateBrowsingEnabled(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void LayoutTestController::setXSSAuditorEnabled(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isBool()) {
    WebPreferences* prefs = shell_->GetWebPreferences();
    prefs->xss_auditor_enabled = args[0].value.boolValue;
    prefs->Apply(shell_->webView());
  }
  result->SetNull();
}

void LayoutTestController::evaluateScriptInIsolatedWorld(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() >= 2 && args[0].isNumber() && args[1].isString()) {
    WebScriptSource source(WebString::fromUTF8(args[1].ToString()));
    // This relies on the iframe focusing itself when it loads. This is a bit
    // sketchy, but it seems to be what other tests do.
    shell_->webView()->focusedFrame()->executeScriptInIsolatedWorld(
        args[0].ToInt32(), &source, 1, 1);
  }
  result->SetNull();
}

void LayoutTestController::setAllowUniversalAccessFromFileURLs(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isBool()) {
    WebPreferences* prefs = shell_->GetWebPreferences();
    prefs->allow_universal_access_from_file_urls = args[0].value.boolValue;
    prefs->Apply(shell_->webView());
  }
  result->SetNull();
}

// Need these conversions because the format of the value for booleans
// may vary - for example, on mac "1" and "0" are used for boolean.
bool LayoutTestController::CppVariantToBool(const CppVariant& value) {
  if (value.isBool())
    return value.ToBoolean();
  else if (value.isInt32())
    return 0 != value.ToInt32();
  else if (value.isString()) {
    std::string valueString = value.ToString();
    if (valueString == "true" || valueString == "1")
      return true;
    if (valueString == "false" || valueString == "0")
      return false;
  }
  LogErrorToConsole("Invalid value. Expected boolean value.");
  return false;
}

int32 LayoutTestController::CppVariantToInt32(const CppVariant& value) {
  if (value.isInt32())
    return value.ToInt32();
  else if (value.isString()) {
    int number;
    if (StringToInt(value.ToString(), &number))
      return number;
  }
  LogErrorToConsole("Invalid value for preference. Expected integer value.");
  return 0;
}

std::wstring LayoutTestController::CppVariantToWstring(
    const CppVariant& value) {
  if (value.isString())
    return UTF8ToWide(value.ToString());
  LogErrorToConsole("Invalid value for preference. Expected string value.");
  return std::wstring();
}

void LayoutTestController::overridePreference(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() == 2 && args[0].isString()) {
    std::string key = args[0].ToString();
    CppVariant value = args[1];
    WebPreferences* preferences = shell_->GetWebPreferences();
    if (key == "WebKitStandardFont")
      preferences->standard_font_family = CppVariantToWstring(value);
    else if (key == "WebKitFixedFont")
      preferences->fixed_font_family = CppVariantToWstring(value);
    else if (key == "WebKitSerifFont")
      preferences->serif_font_family = CppVariantToWstring(value);
    else if (key == "WebKitSansSerifFont")
      preferences->sans_serif_font_family = CppVariantToWstring(value);
    else if (key == "WebKitCursiveFont")
      preferences->cursive_font_family = CppVariantToWstring(value);
    else if (key == "WebKitFantasyFont")
      preferences->fantasy_font_family = CppVariantToWstring(value);
    else if (key == "WebKitDefaultFontSize")
      preferences->default_font_size = CppVariantToInt32(value);
    else if (key == "WebKitDefaultFixedFontSize")
      preferences->default_fixed_font_size = CppVariantToInt32(value);
    else if (key == "WebKitMinimumFontSize")
      preferences->minimum_font_size = CppVariantToInt32(value);
    else if (key == "WebKitMinimumLogicalFontSize")
      preferences->minimum_logical_font_size = CppVariantToInt32(value);
    else if (key == "WebKitDefaultTextEncodingName")
      preferences->default_encoding = value.ToString();
    else if (key == "WebKitJavaScriptEnabled")
      preferences->javascript_enabled = CppVariantToBool(value);
    else if (key == "WebKitWebSecurityEnabled")
      preferences->web_security_enabled = CppVariantToBool(value);
    else if (key == "WebKitJavaScriptCanOpenWindowsAutomatically")
      preferences->javascript_can_open_windows_automatically =
          CppVariantToBool(value);
    else if (key == "WebKitDisplayImagesKey")
      preferences->loads_images_automatically = CppVariantToBool(value);
    else if (key == "WebKitPluginsEnabled")
      preferences->plugins_enabled = CppVariantToBool(value);
    else if (key == "WebKitDOMPasteAllowedPreferenceKey")
      preferences->dom_paste_enabled = CppVariantToBool(value);
    else if (key == "WebKitDeveloperExtrasEnabledPreferenceKey")
      preferences->developer_extras_enabled = CppVariantToBool(value);
    else if (key == "WebKitShrinksStandaloneImagesToFit")
      preferences->shrinks_standalone_images_to_fit = CppVariantToBool(value);
    else if (key == "WebKitTextAreasAreResizable")
      preferences->text_areas_are_resizable = CppVariantToBool(value);
    else if (key == "WebKitJavaEnabled")
      preferences->java_enabled = CppVariantToBool(value);
    else if (key == "WebKitUsesPageCachePreferenceKey")
      preferences->uses_page_cache = CppVariantToBool(value);
    else if (key == "WebKitXSSAuditorEnabled")
      preferences->xss_auditor_enabled = CppVariantToBool(value);
    else if (key == "WebKitLocalStorageEnabledPreferenceKey")
      preferences->local_storage_enabled = CppVariantToBool(value);
    else if (key == "WebKitOfflineWebApplicationCacheEnabled")
      preferences->application_cache_enabled = CppVariantToBool(value);
    else if (key == "WebKitTabToLinksPreferenceKey")
      preferences->tabs_to_links = CppVariantToBool(value);
    else if (key == "WebKitWebGLEnabled")
      preferences->experimental_webgl_enabled = CppVariantToBool(value);
    else {
      std::string message("Invalid name for preference: ");
      message.append(key);
      LogErrorToConsole(message);
    }
    preferences->Apply(shell_->webView());
  }
  result->SetNull();
}

void LayoutTestController::fallbackMethod(
    const CppArgumentList& args, CppVariant* result) {
  std::wstring message(L"JavaScript ERROR: unknown method called on LayoutTestController");
  if (!shell_->layout_test_mode()) {
    logging::LogMessage("CONSOLE:", 0).stream() << message;
  } else {
    printf("CONSOLE MESSAGE: %S\n", message.c_str());
  }
  result->SetNull();
}

void LayoutTestController::whiteListAccessFromOrigin(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  if (args.size() != 4 || !args[0].isString() || !args[1].isString() ||
      !args[2].isString() || !args[3].isBool())
    return;

  WebKit::WebURL url(GURL(args[0].ToString()));
  if (!url.isValid())
    return;

  WebSecurityPolicy::whiteListAccessFromOrigin(url,
      WebString::fromUTF8(args[1].ToString()),
      WebString::fromUTF8(args[2].ToString()),
       args[3].ToBoolean());
}

void LayoutTestController::clearAllDatabases(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
  SimpleDatabaseSystem::GetInstance()->ClearAllDatabases();
}

void LayoutTestController::setDatabaseQuota(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
  if ((args.size() >= 1) && args[0].isInt32())
    SimpleDatabaseSystem::GetInstance()->SetDatabaseQuota(args[0].ToInt32());
}

void LayoutTestController::setPOSIXLocale(const CppArgumentList& args,
                                          CppVariant* result) {
  result->SetNull();
  if (args.size() == 1 && args[0].isString()) {
    std::string new_locale = args[0].ToString();
    setlocale(LC_ALL, new_locale.c_str());
  }
}

void LayoutTestController::counterValueForElementById(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
  if (args.size() < 1 || !args[0].isString())
    return;
  std::wstring counterValue;
  if (!webkit_glue::CounterValueForElementById(shell_->webView()->mainFrame(),
                                               args[0].ToString(),
                                               &counterValue))
    return;
  result->Set(WideToUTF8(counterValue));
}

void LayoutTestController::LogErrorToConsole(const std::string& text) {
  shell_->delegate()->didAddMessageToConsole(
      WebConsoleMessage(WebConsoleMessage::LevelError,
                        WebString::fromUTF8(text)),
      WebString(), 0);
}

void LayoutTestController::setTimelineProfilingEnabled(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
  if (args.size() < 1 || !args[0].isBool())
    return;
  shell_->dev_tools_agent()->setTimelineProfilingEnabled(args[0].ToBoolean());
}

void LayoutTestController::evaluateInWebInspector(const CppArgumentList& args,
                                                  CppVariant* result) {
  result->SetNull();
  if (args.size() < 2 || !args[0].isInt32() || !args[1].isString())
    return;
  shell_->dev_tools_agent()->evaluateInWebInspector(args[0].ToInt32(),
                                                    args[1].ToString());
}

void LayoutTestController::forceRedSelectionColors(const CppArgumentList& args,
                                                   CppVariant* result) {
  result->SetNull();
  shell_->webView()->setSelectionColors(0xffee0000, 0xff00ee00, 0xff000000,
                                        0xffc0c0c0);
}

void LayoutTestController::addUserScript(const CppArgumentList& args,
                                         CppVariant* result) {
  result->SetNull();
  if (args.size() < 1 || !args[0].isString() || !args[1].isBool())
    return;
  shell_->webView()->addUserScript(WebString::fromUTF8(args[0].ToString()),
                                   args[1].ToBoolean());
}
