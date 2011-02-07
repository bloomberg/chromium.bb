// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the definition for LayoutTestController.

#include <vector>

#include "webkit/tools/test_shell/layout_test_controller.h"

#include "base/base64.h"
#include "base/basictypes.h"
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
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSpeechInputControllerMock.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/dom_operations.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/tools/test_shell/notification_presenter.h"
#include "webkit/tools/test_shell/simple_database_system.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_devtools_agent.h"
#include "webkit/tools/test_shell/test_webview_delegate.h"

using std::string;
using std::wstring;

using WebKit::WebBindings;
using WebKit::WebConsoleMessage;
using WebKit::WebElement;
using WebKit::WebScriptSource;
using WebKit::WebSecurityPolicy;
using WebKit::WebSize;
using WebKit::WebString;
using WebKit::WebURL;

TestShell* LayoutTestController::shell_ = NULL;
// Most of these flags need to be cleared in Reset() so that they get turned
// off between each test run.
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
  BindMethod("showWebInspector", &LayoutTestController::showWebInspector);
  BindMethod("closeWebInspector", &LayoutTestController::closeWebInspector);
  BindMethod("setWindowIsKey", &LayoutTestController::setWindowIsKey);
  BindMethod("setTabKeyCyclesThroughElements", &LayoutTestController::setTabKeyCyclesThroughElements);
  BindMethod("setUserStyleSheetLocation", &LayoutTestController::setUserStyleSheetLocation);
  BindMethod("setUserStyleSheetEnabled", &LayoutTestController::setUserStyleSheetEnabled);
  BindMethod("setAuthorAndUserStylesEnabled", &LayoutTestController::setAuthorAndUserStylesEnabled);
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
  BindMethod("suspendAnimations", &LayoutTestController::suspendAnimations);
  BindMethod("resumeAnimations", &LayoutTestController::resumeAnimations);
  BindMethod("elementDoesAutoCompleteForElementWithId", &LayoutTestController::elementDoesAutoCompleteForElementWithId);
  BindMethod("numberOfActiveAnimations", &LayoutTestController::numberOfActiveAnimations);
  BindMethod("disableImageLoading", &LayoutTestController::disableImageLoading);
  BindMethod("setIconDatabaseEnabled", &LayoutTestController::setIconDatabaseEnabled);
  BindMethod("setCustomPolicyDelegate", &LayoutTestController::setCustomPolicyDelegate);
  BindMethod("setScrollbarPolicy", &LayoutTestController::setScrollbarPolicy);
  BindMethod("waitForPolicyDelegate", &LayoutTestController::waitForPolicyDelegate);
  BindMethod("setWillSendRequestClearHeader", &LayoutTestController::setWillSendRequestClearHeader);
  BindMethod("setWillSendRequestReturnsNullOnRedirect", &LayoutTestController::setWillSendRequestReturnsNullOnRedirect);
  BindMethod("setWillSendRequestReturnsNull", &LayoutTestController::setWillSendRequestReturnsNull);
  BindMethod("addOriginAccessWhitelistEntry",
      &LayoutTestController::addOriginAccessWhitelistEntry);
  BindMethod("removeOriginAccessWhitelistEntry",
      &LayoutTestController::removeOriginAccessWhitelistEntry);
  BindMethod("clearAllDatabases", &LayoutTestController::clearAllDatabases);
  BindMethod("setDatabaseQuota", &LayoutTestController::setDatabaseQuota);
  BindMethod("setPOSIXLocale", &LayoutTestController::setPOSIXLocale);
  BindMethod("counterValueForElementById", &LayoutTestController::counterValueForElementById);
  BindMethod("addUserScript", &LayoutTestController::addUserScript);
  BindMethod("addUserStyleSheet", &LayoutTestController::addUserStyleSheet);
  BindMethod("pageNumberForElementById", &LayoutTestController::pageNumberForElementById);
  BindMethod("numberOfPages", &LayoutTestController::numberOfPages);
  BindMethod("grantDesktopNotificationPermission", &LayoutTestController::grantDesktopNotificationPermission);
  BindMethod("setDomainRelaxationForbiddenForURLScheme", &LayoutTestController::setDomainRelaxationForbiddenForURLScheme);
  BindMethod("sampleSVGAnimationForElementAtTime", &LayoutTestController::sampleSVGAnimationForElementAtTime);
  BindMethod("hasSpellingMarker", &LayoutTestController::hasSpellingMarker);

  // The following are stubs.
  BindMethod("setMainFrameIsFirstResponder", &LayoutTestController::setMainFrameIsFirstResponder);
  BindMethod("display", &LayoutTestController::display);
  BindMethod("testRepaint", &LayoutTestController::testRepaint);
  BindMethod("repaintSweepHorizontally", &LayoutTestController::repaintSweepHorizontally);
  BindMethod("clearBackForwardList", &LayoutTestController::clearBackForwardList);
  BindMethod("keepWebHistory", &LayoutTestController::keepWebHistory);
  BindMethod("layerTreeAsText", &LayoutTestController::layerTreeAsText);
  BindMethod("storeWebScriptObject", &LayoutTestController::storeWebScriptObject);
  BindMethod("accessStoredWebScriptObject", &LayoutTestController::accessStoredWebScriptObject);
  BindMethod("objCClassNameOf", &LayoutTestController::objCClassNameOf);
  BindMethod("addDisallowedURL", &LayoutTestController::addDisallowedURL);
  BindMethod("callShouldCloseOnWebView", &LayoutTestController::callShouldCloseOnWebView);
  BindMethod("setCallCloseOnWebViews", &LayoutTestController::setCallCloseOnWebViews);
  BindMethod("setPrivateBrowsingEnabled", &LayoutTestController::setPrivateBrowsingEnabled);
  BindMethod("setUseDashboardCompatibilityMode", &LayoutTestController::setUseDashboardCompatibilityMode);

  BindMethod("setJavaScriptCanAccessClipboard", &LayoutTestController::setJavaScriptCanAccessClipboard);
  BindMethod("setXSSAuditorEnabled", &LayoutTestController::setXSSAuditorEnabled);
  BindMethod("evaluateScriptInIsolatedWorld", &LayoutTestController::evaluateScriptInIsolatedWorld);
  BindMethod("overridePreference", &LayoutTestController::overridePreference);
  BindMethod("setAllowUniversalAccessFromFileURLs", &LayoutTestController::setAllowUniversalAccessFromFileURLs);
  BindMethod("setAllowFileAccessFromFileURLs", &LayoutTestController::setAllowFileAccessFromFileURLs);
  BindMethod("setJavaScriptProfilingEnabled", &LayoutTestController::setJavaScriptProfilingEnabled);
  BindMethod("setTimelineProfilingEnabled", &LayoutTestController::setTimelineProfilingEnabled);
  BindMethod("evaluateInWebInspector", &LayoutTestController::evaluateInWebInspector);
  BindMethod("forceRedSelectionColors", &LayoutTestController::forceRedSelectionColors);
  BindMethod("setEditingBehavior", &LayoutTestController::setEditingBehavior);

  BindMethod("setGeolocationPermission", &LayoutTestController::setGeolocationPermission);
  BindMethod("setMockGeolocationPosition", &LayoutTestController::setMockGeolocationPosition);
  BindMethod("setMockGeolocationError", &LayoutTestController::setMockGeolocationError);

  BindMethod("markerTextForListItem", &LayoutTestController::markerTextForListItem);

  BindMethod("setMockDeviceOrientation", &LayoutTestController::setMockDeviceOrientation);
  BindMethod("addMockSpeechInputResult", &LayoutTestController::addMockSpeechInputResult);

  // The fallback method is called when an unknown method is invoked.
  BindFallbackMethod(&LayoutTestController::fallbackMethod);

  // Shared properties.
  // globalFlag is used by a number of layout tests in
  // LayoutTests\http\tests\security\dataURL.
  BindProperty("globalFlag", &globalFlag_);
  // webHistoryItemCount is used by tests in LayoutTests\http\tests\history
  BindProperty("webHistoryItemCount", &webHistoryItemCount_);
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
  test_repaint_ = false;
  sweep_horizontally_ = false;
  should_add_file_to_pasteboard_ = false;
  stop_provisional_frame_loads_ = false;
  globalFlag_.Set(false);
  webHistoryItemCount_.Set(0);

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

void LayoutTestController::showWebInspector(
    const CppArgumentList& args, CppVariant* result) {
  shell_->ShowDevTools();
  result->SetNull();
}

void LayoutTestController::closeWebInspector(
    const CppArgumentList& args, CppVariant* result) {
  shell_->CloseDevTools();
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
  result->SetNull();

  if (args.size() > 0 && args[0].isString()) {
    GURL location(TestShell::RewriteLocalUrl(args[0].ToString()));

    FilePath local_path;
    if (!net::FileURLToFilePath(location, &local_path))
      return;

    std::string contents;
    if (!file_util::ReadFileToString(local_path, &contents))
      return;

    std::string contents_base64;
    if (!base::Base64Encode(contents, &contents_base64))
      return;

    const char kDataUrlPrefix[] = "data:text/css;charset=utf-8;base64,";
    location = GURL(kDataUrlPrefix + contents_base64);

    shell_->delegate()->SetUserStyleSheetLocation(location);
  }
}

void LayoutTestController::setAuthorAndUserStylesEnabled(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isBool()) {
    shell_->delegate()->SetAuthorAndUserStylesEnabled(args[0].value.boolValue);
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


void LayoutTestController::setScrollbarPolicy(
    const CppArgumentList& args, CppVariant* result) {
  // FIXME: implement.
  // Currently only has a non-null implementation on QT.
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

void LayoutTestController::setWillSendRequestClearHeader(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isString()) {
    std::string header = args[0].ToString();
    if (!header.empty())
      shell_->delegate()->set_clear_header(header);
  }
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

void LayoutTestController::suspendAnimations(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  WebKit::WebFrame* web_frame = shell_->webView()->mainFrame();
  if (!web_frame)
    return;

  WebKit::WebAnimationController* controller = web_frame->animationController();
  if (!controller)
    return;
  controller->suspendAnimations();
}

void LayoutTestController::resumeAnimations(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  WebKit::WebFrame* web_frame = shell_->webView()->mainFrame();
  if (!web_frame)
    return;

  WebKit::WebAnimationController* controller = web_frame->animationController();
  if (!controller)
    return;
  controller->resumeAnimations();
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

void LayoutTestController::callShouldCloseOnWebView(
    const CppArgumentList& args, CppVariant* result) {
  bool rv = shell_->webView()->dispatchBeforeUnloadEvent();
  result->Set(rv);
}

void LayoutTestController::grantDesktopNotificationPermission(
  const CppArgumentList& args, CppVariant* result) {
  if (args.size() != 1 || !args[0].isString()) {
    result->Set(false);
    return;
  }
  std::string origin = args[0].ToString();
  shell_->notification_presenter()->grantPermission(origin);
  result->Set(true);
}

void LayoutTestController::setDomainRelaxationForbiddenForURLScheme(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() != 2 || !args[0].isBool() || !args[1].isString())
    return;

  shell_->webView()->setDomainRelaxationForbidden(
      CppVariantToBool(args[0]), WebString::fromUTF8(args[1].ToString()));
}

void LayoutTestController::sampleSVGAnimationForElementAtTime(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() != 3) {
    result->SetNull();
    return;
  }
  bool success = shell_->webView()->mainFrame()->pauseSVGAnimation(
      WebString::fromUTF8(args[0].ToString()),
      args[1].ToDouble(),
      WebString::fromUTF8(args[2].ToString()));
  result->Set(success);
}

//
// Unimplemented stubs
//

void LayoutTestController::setMainFrameIsFirstResponder(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void LayoutTestController::display(
    const CppArgumentList& args, CppVariant* result) {
  WebViewHost* view_host = shell_->webViewHost();
  const WebSize& size = view_host->webview()->size();
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

void LayoutTestController::layerTreeAsText(
    const CppArgumentList& args,  CppVariant* result) {
  WebString rv = shell_->webView()->mainFrame()->layerTreeAsText();
  result->Set(rv.utf8());
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

void LayoutTestController::setJavaScriptCanAccessClipboard(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isBool()) {
    WebPreferences* prefs = shell_->GetWebPreferences();
    prefs->javascript_can_access_clipboard = args[0].value.boolValue;
    prefs->Apply(shell_->webView());
  }
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

void LayoutTestController::setAllowFileAccessFromFileURLs(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0 && args[0].isBool()) {
    WebPreferences* prefs = shell_->GetWebPreferences();
    prefs->allow_file_access_from_file_urls = args[0].value.boolValue;
    prefs->Apply(shell_->webView());
  }
  result->SetNull();
}

void LayoutTestController::addMockSpeechInputResult(const CppArgumentList& args,
                                                    CppVariant* result) {
  if (args.size() > 0 && args[0].isString() && args[1].isNumber() &&
      args[2].isString()) {
    shell_->speech_input_controller_mock()->addMockRecognitionResult(
        WebString::fromUTF8(args[0].ToString()), args[1].ToDouble(),
        WebString::fromUTF8(args[2].ToString()));
  }
  result->SetNull();
}

// Need these conversions because the format of the value for booleans
// may vary - for example, on mac "1" and "0" are used for boolean.
bool LayoutTestController::CppVariantToBool(const CppVariant& value) {
  if (value.isBool())
    return value.ToBoolean();
  else if (value.isNumber())
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
  if (value.isNumber())
    return value.ToInt32();
  else if (value.isString()) {
    int number;
    if (base::StringToInt(value.ToString(), &number))
      return number;
  }
  LogErrorToConsole("Invalid value for preference. Expected integer value.");
  return 0;
}

string16 LayoutTestController::CppVariantToString16(
    const CppVariant& value) {
  if (value.isString())
    return UTF8ToUTF16(value.ToString());
  LogErrorToConsole("Invalid value for preference. Expected string value.");
  return string16();
}

void LayoutTestController::overridePreference(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() == 2 && args[0].isString()) {
    std::string key = args[0].ToString();
    CppVariant value = args[1];
    WebPreferences* preferences = shell_->GetWebPreferences();
    if (key == "WebKitStandardFont")
      preferences->standard_font_family = CppVariantToString16(value);
    else if (key == "WebKitFixedFont")
      preferences->fixed_font_family = CppVariantToString16(value);
    else if (key == "WebKitSerifFont")
      preferences->serif_font_family = CppVariantToString16(value);
    else if (key == "WebKitSansSerifFont")
      preferences->sans_serif_font_family = CppVariantToString16(value);
    else if (key == "WebKitCursiveFont")
      preferences->cursive_font_family = CppVariantToString16(value);
    else if (key == "WebKitFantasyFont")
      preferences->fantasy_font_family = CppVariantToString16(value);
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
    else if (key == "WebKitJavaScriptCanAccessClipboard")
      preferences->javascript_can_access_clipboard = CppVariantToBool(value);
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
    else if (key == "WebKitEnableCaretBrowsing")
      preferences->caret_browsing_enabled = CppVariantToBool(value);
    else if (key == "WebKitHyperlinkAuditingEnabled")
      preferences->hyperlink_auditing_enabled = CppVariantToBool(value);
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

void LayoutTestController::addOriginAccessWhitelistEntry(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  if (args.size() != 4 || !args[0].isString() || !args[1].isString() ||
      !args[2].isString() || !args[3].isBool())
    return;

  WebURL url(GURL(args[0].ToString()));
  if (!url.isValid())
    return;

  WebSecurityPolicy::addOriginAccessWhitelistEntry(url,
      WebString::fromUTF8(args[1].ToString()),
      WebString::fromUTF8(args[2].ToString()),
       args[3].ToBoolean());
}

void LayoutTestController::removeOriginAccessWhitelistEntry(
    const CppArgumentList& args, CppVariant* result)
{
  result->SetNull();

  if (args.size() != 4 || !args[0].isString() || !args[1].isString() ||
      !args[2].isString() || !args[3].isBool())
    return;

  WebURL url(GURL(args[0].ToString()));
  if (!url.isValid())
    return;

  WebSecurityPolicy::removeOriginAccessWhitelistEntry(url,
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
  if ((args.size() >= 1) && args[0].isNumber())
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
  string16 counterValue;
  if (!webkit_glue::CounterValueForElementById(shell_->webView()->mainFrame(),
                                               args[0].ToString(),
                                               &counterValue))
    return;
  result->Set(UTF16ToUTF8(counterValue));
}

static bool ParsePageSizeParameters(const CppArgumentList& args,
                                    int arg_offset,
                                    float* page_width_in_pixels,
                                    float* page_height_in_pixels) {
  // WebKit is using the window width/height of DumpRenderTree as the
  // default value of the page size.
  // TODO(hamaji): Once chromium DumpRenderTree is implemented,
  //               share these values with other ports.
  *page_width_in_pixels = 800;
  *page_height_in_pixels = 600;
  switch (args.size() - arg_offset) {
  case 2:
    if (!args[arg_offset].isNumber() || !args[1 + arg_offset].isNumber())
      return false;
    *page_width_in_pixels = static_cast<float>(args[arg_offset].ToInt32());
    *page_height_in_pixels = static_cast<float>(args[1 + arg_offset].ToInt32());
  // fall through.
  case 0:
    break;
  default:
    return false;
  }
  return true;
}

void LayoutTestController::pageNumberForElementById(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
  float page_width_in_pixels = 0;
  float page_height_in_pixels = 0;
  if (!ParsePageSizeParameters(args, 1,
                               &page_width_in_pixels, &page_height_in_pixels))
    return;
  if (!args[0].isString())
    return;

  int page_number = webkit_glue::PageNumberForElementById(
      shell_->webView()->mainFrame(),
      args[0].ToString(),
      page_width_in_pixels,
      page_height_in_pixels);
  result->Set(page_number);
}

void LayoutTestController::numberOfPages(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
  float page_width_in_pixels = 0;
  float page_height_in_pixels = 0;
  if (!ParsePageSizeParameters(args, 0,
                               &page_width_in_pixels, &page_height_in_pixels))
    return;

  int page_number = webkit_glue::NumberOfPages(shell_->webView()->mainFrame(),
                                               page_width_in_pixels,
                                               page_height_in_pixels);
  result->Set(page_number);
}

void LayoutTestController::LogErrorToConsole(const std::string& text) {
  shell_->delegate()->didAddMessageToConsole(
      WebConsoleMessage(WebConsoleMessage::LevelError,
                        WebString::fromUTF8(text)),
      WebString(), 0);
}

void LayoutTestController::setJavaScriptProfilingEnabled(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
  // Dummy method. JS profiling is always enabled in InspectorController.
  return;
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
  if (args.size() < 2 || !args[0].isNumber() || !args[1].isString())
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
  if (args.size() < 3 || !args[0].isString() || !args[1].isBool() ||
      !args[2].isBool())
    return;
  WebKit::WebView::addUserScript(
      WebString::fromUTF8(args[0].ToString()),
      WebKit::WebVector<WebString>(),
      args[1].ToBoolean() ?
          WebKit::WebView::UserScriptInjectAtDocumentStart :
          WebKit::WebView::UserScriptInjectAtDocumentEnd,
      args[2].ToBoolean() ?
          WebKit::WebView::UserContentInjectInAllFrames :
          WebKit::WebView::UserContentInjectInTopFrameOnly);
}

void LayoutTestController::addUserStyleSheet(const CppArgumentList& args,
                                             CppVariant* result) {
  result->SetNull();
  if (args.size() < 2 || !args[0].isString() || !args[1].isBool())
    return;
  WebKit::WebView::addUserStyleSheet(
      WebString::fromUTF8(args[0].ToString()),
      WebKit::WebVector<WebString>(),
      args[1].ToBoolean() ?
          WebKit::WebView::UserContentInjectInAllFrames :
          WebKit::WebView::UserContentInjectInTopFrameOnly,
      WebKit::WebView::UserStyleInjectInExistingDocuments);
}

void LayoutTestController::setEditingBehavior(const CppArgumentList& args,
                                              CppVariant* result) {
  result->SetNull();
  WebString key = WebString::fromUTF8(args[0].ToString());
  WebKit::WebSettings::EditingBehavior behavior;
  if (key == "mac") {
    behavior = WebKit::WebSettings::EditingBehaviorMac;
  } else if (key == "win") {
    behavior = WebKit::WebSettings::EditingBehaviorWin;
  } else if (key == "unix") {
    behavior = WebKit::WebSettings::EditingBehaviorUnix;
  } else {
    LogErrorToConsole("Passed invalid editing behavior. "
                      "Should be 'mac', 'win', or 'unix'.");
    return;
  }
  shell_->webView()->settings()->setEditingBehavior(behavior);
}

void LayoutTestController::setGeolocationPermission(const CppArgumentList& args,
                                                    CppVariant* result) {
  if (args.size() < 1 || !args[0].isBool())
    return;
  shell_->geolocation_client_mock()->setPermission(
      args[0].ToBoolean());
}

void LayoutTestController::setMockGeolocationPosition(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() < 3 ||
      !args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber())
    return;
  shell_->geolocation_client_mock()->setPosition(
      args[0].ToDouble(), args[1].ToDouble(), args[2].ToDouble());
}

void LayoutTestController::setMockGeolocationError(const CppArgumentList& args,
                                                   CppVariant* result) {
  if (args.size() < 2 || !args[0].isNumber() || !args[1].isString())
    return;
  shell_->geolocation_client_mock()->setError(
      args[0].ToInt32(), WebString::fromUTF8(args[1].ToString()));
}

void LayoutTestController::markerTextForListItem(const CppArgumentList& args,
                                                 CppVariant* result) {
  WebElement element;
  if (!WebBindings::getElement(args[0].value.objectValue, &element))
    result->SetNull();
  else
    result->Set(
        element.document().frame()->markerTextForListItem(element).utf8());
}

void LayoutTestController::setMockDeviceOrientation(const CppArgumentList& args,
                                                    CppVariant* result) {
  if (args.size() < 6 ||
      !args[0].isBool() || !args[1].isNumber() ||
      !args[2].isBool() || !args[3].isNumber() ||
      !args[4].isBool() || !args[5].isNumber())
    return;
  WebKit::WebDeviceOrientation orientation(args[0].ToBoolean(),
                                           args[1].ToDouble(),
                                           args[2].ToBoolean(),
                                           args[3].ToDouble(),
                                           args[4].ToBoolean(),
                                           args[5].ToDouble());

  shell_->device_orientation_client_mock()->setOrientation(orientation);
}

void LayoutTestController::hasSpellingMarker(const CppArgumentList& arguments,
                                             CppVariant* result) {
  if (arguments.size() < 2 || !arguments[0].isNumber() || !arguments[1].isNumber())
    return;
  result->Set(shell_->webView()->mainFrame()->selectionStartHasSpellingMarkerFor(
      arguments[0].ToInt32(), arguments[1].ToInt32()));
}
