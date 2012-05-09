// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
  LayoutTestController class:
  Bound to a JavaScript window.layoutTestController object using the
  CppBoundClass::BindToJavascript(), this allows layout tests that are run in
  the test_shell (or, in principle, any web page loaded into a client app built
  with this class) to control various aspects of how the tests are run and what
  sort of output they produce.
*/

#ifndef WEBKIT_TOOLS_TEST_SHELL_LAYOUT_TEST_CONTROLLER_H_
#define WEBKIT_TOOLS_TEST_SHELL_LAYOUT_TEST_CONTROLLER_H_

#include <queue>

#include "base/memory/weak_ptr.h"
#include "base/timer.h"
#include "base/string16.h"
#include "webkit/glue/cpp_bound_class.h"

class TestShell;

class LayoutTestController : public webkit_glue::CppBoundClass {
 public:
  // Builds the property and method lists needed to bind this class to a JS
  // object.
  LayoutTestController(TestShell* shell);
  virtual ~LayoutTestController();

  // By default, tests end when page load is complete.  These methods are used
  // to delay the completion of the test until notifyDone is called.
  void waitUntilDone(const webkit_glue::CppArgumentList& args,
                     webkit_glue::CppVariant* result);
  void notifyDone(const webkit_glue::CppArgumentList& args,
                  webkit_glue::CppVariant* result);

  // The fallback method is called when a nonexistent method is called on
  // the layout test controller object.
  // It is usefull to catch typos in the JavaScript code (a few layout tests
  // do have typos in them) and it allows the script to continue running in
  // that case (as the Mac does).
  void fallbackMethod(const webkit_glue::CppArgumentList& args,
                      webkit_glue::CppVariant* result);

 public:
  // The following methods are not exposed to JavaScript.
  void SetWorkQueueFrozen(bool frozen) { work_queue_.set_frozen(frozen); }

  bool AcceptsEditing() { return accepts_editing_; }
  bool CanOpenWindows() { return can_open_windows_; }
  bool StopProvisionalFrameLoads() { return stop_provisional_frame_loads_; }

  // Called by the webview delegate when the toplevel frame load is done.
  void LocationChangeDone();

  // Called by the webview delegate when the policy delegate runs if the
  // waitForPolicyDelegate was called.
  void PolicyDelegateDone();

  // Reinitializes all static values.  The Reset() method should be called
  // before the start of each test (currently from
  // TestShell::RunFileTest).
  void Reset();

  // A single item in the work queue.
  class WorkItem {
   public:
    virtual ~WorkItem() {}

    // Returns true if this started a load.
    virtual bool Run(TestShell* shell) = 0;
  };

  // Used to clear the value of shell_ from test_shell_tests.
  static void ClearShell() { shell_ = NULL; }

 private:
  friend class WorkItem;

  // Helper class for managing events queued by methods like queueLoad or
  // queueScript.
  class WorkQueue {
   public:
    WorkQueue();
    virtual ~WorkQueue();
    void ProcessWorkSoon();

    // Reset the state of the class between tests.
    void Reset();

    void AddWork(WorkItem* work);

    void set_frozen(bool frozen) { frozen_ = frozen; }
    bool empty() { return queue_.empty(); }

   private:
    void ProcessWork();

    base::OneShotTimer<WorkQueue> timer_;
    std::queue<WorkItem*> queue_;
    bool frozen_;
  };

  void notifyDoneTimedOut();

  void LogErrorToConsole(const std::string& text);

  void completeNotifyDone(bool is_timeout);

  // Used for test timeouts.
  // TODO(ojan): Use base::OneShotTimer.
  base::WeakPtrFactory<LayoutTestController> weak_factory_;

  // Non-owning pointer.  The LayoutTestController is owned by the host.
  static TestShell* shell_;

  // If true, the element will be treated as editable.  This value is returned
  // from various editing callbacks that are called just before edit operations
  // are allowed.
  static bool accepts_editing_;

  // If true, new windows can be opened via javascript or by plugins.  By
  // default, set to false and can be toggled to true using
  // setCanOpenWindows().
  static bool can_open_windows_;

  // When reset is called, go through and close all but the main test shell
  // window.  By default, set to true but toggled to false using
  // setCloseRemainingWindowsWhenComplete().
  static bool close_remaining_windows_;

  // If true, stops provisional frame loads during the
  // DidStartProvisionalLoadForFrame callback.
  static bool stop_provisional_frame_loads_;

  // If true, don't dump output until notifyDone is called.
  static bool wait_until_done_;

  static WorkQueue work_queue_;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_LAYOUT_TEST_CONTROLLER_H_
