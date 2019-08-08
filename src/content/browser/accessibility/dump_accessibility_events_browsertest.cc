// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <set>
#include <string>
#include <vector>

#include "base/bind_helpers.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/accessibility/accessibility_event_recorder.h"
#include "content/browser/accessibility/accessibility_tree_formatter.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/dump_accessibility_browsertest_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"

namespace content {

typedef AccessibilityTreeFormatter::PropertyFilter PropertyFilter;

// See content/test/data/accessibility/readme.md for an overview.
//
// Tests that the right platform-specific accessibility events are fired
// in response to things that happen in a web document.
//
// Similar to DumpAccessibilityTree in that each test consists of a
// single HTML file, possibly with a few special directives in comments,
// and then expectation files in text format for each platform.
//
// While DumpAccessibilityTree just loads the document and then
// prints out a text representation of the accessibility tree,
// DumpAccessibilityEvents loads the document, then executes the
// JavaScript function "go()", then it records and dumps all accessibility
// events generated as a result of that "go" function executing.
//
// How each event is dumped is platform-specific, but should be of the
// form:
//
// <event> on <node>
//
// ...where <event> is the name of the event, and <node> is a description
// of the node the event fired on, such as the node's role and name.
//
// As with DumpAccessibilityTree, DumpAccessibilityEvents takes the events
// dumped from that particular html file and compares it to the expectation
// file in the same directory (for example, test-name-expected-win.txt)
// and the test fails if they don't agree.
//
// Currently it's not possible to test for accessibility events that
// don't fire immediately (i.e. within the call scope of the call to "go()");
// the test framework calls "go()" and then sends a sentinel event signaling
// the end of the test; anything received after that is too late.
class DumpAccessibilityEventsTest : public DumpAccessibilityTestBase {
 public:
  void AddDefaultFilters(
      std::vector<PropertyFilter>* property_filters) override {
    // Suppress spurious focus events on the document object.
    property_filters->push_back(
        PropertyFilter(base::ASCIIToUTF16("EVENT_OBJECT_FOCUS*DOCUMENT*"),
                       PropertyFilter::DENY));
    property_filters->push_back(
        PropertyFilter(base::ASCIIToUTF16("AutomationFocusChanged*document*"),
                       PropertyFilter::DENY));
  }

  std::vector<std::string> Dump(std::vector<std::string>& run_until) override;

  void OnDiffFailed() override;
  void RunEventTest(const base::FilePath::CharType* file_path);

 private:
  base::string16 initial_tree_;
  base::string16 final_tree_;
};

bool IsRecordingComplete(AccessibilityEventRecorder& event_recorder,
                         std::vector<std::string>& run_until) {
  // If no @RUN-UNTIL-EVENT directives, then having any events is enough.
  LOG(ERROR) << "=== IsRecordingComplete#1 run_until size=" << run_until.size();
  if (run_until.empty())
    return true;

  std::vector<std::string> event_logs = event_recorder.event_logs();
  LOG(ERROR) << "=== IsRecordingComplete#2 Logs size=" << event_logs.size();

  for (size_t i = 0; i < event_logs.size(); ++i)
    for (size_t j = 0; j < run_until.size(); ++j)
      if (event_logs[i].find(run_until[j]) != std::string::npos)
        return true;

  return false;
}

std::vector<std::string> DumpAccessibilityEventsTest::Dump(
    std::vector<std::string>& run_until) {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  base::ProcessId pid = base::GetCurrentProcId();

  // Save a copy of the accessibility tree (as a text dump); we'll
  // log this for the user later if the test fails.
  initial_tree_ = DumpUnfilteredAccessibilityTreeAsString();

  // Create a waiter that waits for any one accessibility event.
  // This will ensure that after calling the go() function, we
  // block until we've received an accessibility event generated as
  // a result of this function.
  std::unique_ptr<AccessibilityNotificationWaiter> waiter;

  final_tree_.clear();
  bool run_go_again = false;
  std::vector<std::string> result;
  do {
    // Create a new Event Recorder for the run
    std::unique_ptr<AccessibilityEventRecorder> event_recorder =
        event_recorder_factory_(
            web_contents->GetRootBrowserAccessibilityManager(), pid,
            base::StringPiece{});
    event_recorder->set_only_web_events(true);

    waiter.reset(new AccessibilityNotificationWaiter(
        shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kNone));

    base::Value go_results =
        ExecuteScriptAndGetValue(web_contents->GetMainFrame(), "go()");
    run_go_again = go_results.is_bool() && go_results.GetBool();

    for (;;) {
      waiter->WaitForNotification();  // Run at least once.
      if (IsRecordingComplete(*event_recorder, run_until))
        break;
    }

    // More than one accessibility event could have been generated.
    // To make sure we've received all accessibility events, add a
    // sentinel by calling SignalEndOfTest and waiting for a kEndOfTest
    // event in response.
    waiter.reset(new AccessibilityNotificationWaiter(
        shell()->web_contents(), ui::kAXModeComplete,
        ax::mojom::Event::kEndOfTest));
    BrowserAccessibilityManager* manager =
        web_contents->GetRootBrowserAccessibilityManager();
    manager->SignalEndOfTest();
    waiter->WaitForNotification();

    // Save a copy of the final accessibility tree (as a text dump); we'll
    // log this for the user later if the test fails.
    final_tree_.append(DumpUnfilteredAccessibilityTreeAsString());

    // Dump the event logs, running them through any filters specified
    // in the HTML file.
    event_recorder->FlushAsyncEvents();
    std::vector<std::string> event_logs = event_recorder->event_logs();

    // Sort the logs so that results are predictable. There are too many
    // nondeterministic things that affect the exact order of events fired,
    // so these tests shouldn't be used to make assertions about event order.
    std::sort(event_logs.begin(), event_logs.end());

    for (size_t i = 0; i < event_logs.size(); ++i) {
      if (AccessibilityTreeFormatter::MatchesPropertyFilters(
              property_filters_, base::UTF8ToUTF16(event_logs[i]), true)) {
        result.push_back(event_logs[i]);
      }
    }

    if (run_go_again) {
      final_tree_.append(base::ASCIIToUTF16("=== Start Continuation ===\n"));
      result.emplace_back("=== Start Continuation ===");
    }
  } while (run_go_again);

  return result;
}

void DumpAccessibilityEventsTest::OnDiffFailed() {
  printf("\n");
  printf("Initial accessibility tree (after load complete):\n");
  printf("%s\n", base::UTF16ToUTF8(initial_tree_).c_str());
  printf("\n");
  printf("Final accessibility tree after events fired:\n");
  printf("%s\n", base::UTF16ToUTF8(final_tree_).c_str());
  printf("\n");
}

void DumpAccessibilityEventsTest::RunEventTest(
    const base::FilePath::CharType* file_path) {
  base::FilePath test_path = GetTestFilePath("accessibility", "event");

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
  }

  base::FilePath event_file = test_path.Append(base::FilePath(file_path));
  RunTest(event_file, "accessibility/event");
}

// Parameterize the tests so that each test-pass is run independently.
struct DumpAccessibilityEventsTestPassToString {
  std::string operator()(const ::testing::TestParamInfo<size_t>& i) const {
    auto passes = AccessibilityEventRecorder::GetTestPasses();
    CHECK_LT(i.param, passes.size());
    return passes[i.param].name;
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    DumpAccessibilityEventsTest,
    ::testing::Range(size_t{0},
                     AccessibilityEventRecorder::GetTestPasses().size()),
    DumpAccessibilityEventsTestPassToString());

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaBusyChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-busy-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaComboBoxCollapse) {
  RunEventTest(FILE_PATH_LITERAL("aria-combo-box-collapse.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaComboBoxExpand) {
  RunEventTest(FILE_PATH_LITERAL("aria-combo-box-expand.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaControlsChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-controls-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaTreeCollapse) {
  RunEventTest(FILE_PATH_LITERAL("aria-tree-collapse.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaTreeExpand) {
  RunEventTest(FILE_PATH_LITERAL("aria-tree-expand.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaTreeItemFocus) {
  RunEventTest(FILE_PATH_LITERAL("aria-treeitem-focus.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaComboBoxFocus) {
  RunEventTest(FILE_PATH_LITERAL("aria-combo-box-focus.html"));
}

// TODO(aboxhall): Fix flaky test
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       DISABLED_AccessibilityEventsAriaComboBoxDelayAddList) {
  RunEventTest(FILE_PATH_LITERAL("aria-combo-box-delay-add-list.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaComboBoxDelayShowList) {
  RunEventTest(FILE_PATH_LITERAL("aria-combo-box-delay-show-list.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaComboBoxNext) {
  RunEventTest(FILE_PATH_LITERAL("aria-combo-box-next.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaSliderValueBothChange) {
  RunEventTest(FILE_PATH_LITERAL("aria-slider-value-both-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaSliderValueChange) {
  RunEventTest(FILE_PATH_LITERAL("aria-slider-value-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaSliderValueTextChange) {
  RunEventTest(FILE_PATH_LITERAL("aria-slider-valuetext-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaSpinButtonValueBothChange) {
  RunEventTest(FILE_PATH_LITERAL("aria-spinbutton-value-both-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaSpinButtonValueChange) {
  RunEventTest(FILE_PATH_LITERAL("aria-spinbutton-value-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaSpinButtonValueTextChange) {
  RunEventTest(FILE_PATH_LITERAL("aria-spinbutton-valuetext-change.html"));
}

// https://crbug.com/941919
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       DISABLED_AccessibilityEventsAddAlert) {
  RunEventTest(FILE_PATH_LITERAL("add-alert.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAddChildOfBody) {
  RunEventTest(FILE_PATH_LITERAL("add-child-of-body.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAddHiddenAttribute) {
  RunEventTest(FILE_PATH_LITERAL("add-hidden-attribute.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAddHiddenAttributeSubtree) {
  RunEventTest(FILE_PATH_LITERAL("add-hidden-attribute-subtree.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAddSubtree) {
  RunEventTest(FILE_PATH_LITERAL("add-subtree.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsCheckedStateChanged) {
  RunEventTest(FILE_PATH_LITERAL("checked-state-changed.html"));
}

// http:/crbug.com/889013
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       DISABLED_AccessibilityEventsCaretHide) {
  RunEventTest(FILE_PATH_LITERAL("caret-hide.html"));
}

// http:/crbug.com/889013
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       DISABLED_AccessibilityEventsCaretMove) {
  RunEventTest(FILE_PATH_LITERAL("caret-move.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsCheckboxValidity) {
  RunEventTest(FILE_PATH_LITERAL("checkbox-validity.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsCSSDisplay) {
  RunEventTest(FILE_PATH_LITERAL("css-display.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsCSSVisibility) {
  RunEventTest(FILE_PATH_LITERAL("css-visibility.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsDescriptionChange) {
  RunEventTest(FILE_PATH_LITERAL("description-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsDescriptionChangeIndirect) {
  RunEventTest(FILE_PATH_LITERAL("description-change-indirect.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsDisabledStateChanged) {
  RunEventTest(FILE_PATH_LITERAL("disabled-state-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsExpandedChange) {
  RunEventTest(FILE_PATH_LITERAL("expanded-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsFormRequiredChanged) {
  RunEventTest(FILE_PATH_LITERAL("form-required-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsInnerHtmlChange) {
  RunEventTest(FILE_PATH_LITERAL("inner-html-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsInputTypeTextValueChanged) {
  RunEventTest(FILE_PATH_LITERAL("input-type-text-value-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsInvalidStatusChange) {
  RunEventTest(FILE_PATH_LITERAL("invalid-status-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsListboxFocus) {
  RunEventTest(FILE_PATH_LITERAL("listbox-focus.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsListboxNext) {
  RunEventTest(FILE_PATH_LITERAL("listbox-next.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsLiveRegionAdd) {
  RunEventTest(FILE_PATH_LITERAL("live-region-add.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsLiveRegionAddLiveAttribute) {
  RunEventTest(FILE_PATH_LITERAL("live-region-add-live-attribute.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsLiveRegionChange) {
  RunEventTest(FILE_PATH_LITERAL("live-region-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsLiveRegionCreate) {
  RunEventTest(FILE_PATH_LITERAL("live-region-create.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsLiveRegionOff) {
  RunEventTest(FILE_PATH_LITERAL("live-region-off.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsLiveRegionElemReparent) {
  RunEventTest(FILE_PATH_LITERAL("live-region-elem-reparent.html"));
}

// TODO(aboxhall): Fix flakiness.
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       DISABLED_AccessibilityEventsLiveRegionIgnoresClick) {
  RunEventTest(FILE_PATH_LITERAL("live-region-ignores-click.html"));
}

// http:/crbug.com/786848
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       DISABLED_AccessibilityEventsLiveRegionRemove) {
  RunEventTest(FILE_PATH_LITERAL("live-region-remove.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsMenuListCollapse) {
  RunEventTest(FILE_PATH_LITERAL("menulist-collapse.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsMenuListCollapseNext) {
  RunEventTest(FILE_PATH_LITERAL("menulist-collapse-next.html"));
}

// https://crbug.com/719030
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       DISABLED_AccessibilityEventsMenuListExpand) {
  RunEventTest(FILE_PATH_LITERAL("menulist-expand.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsMenuListFocus) {
  RunEventTest(FILE_PATH_LITERAL("menulist-focus.html"));
}

// https://crbug.com/719030
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       DISABLED_AccessibilityEventsMenuListNext) {
  RunEventTest(FILE_PATH_LITERAL("menulist-next.html"));
}

// http://crbug.com/719030
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       DISABLED_AccessibilityEventsMenuListPopup) {
  RunEventTest(FILE_PATH_LITERAL("menulist-popup.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsNameChange) {
  RunEventTest(FILE_PATH_LITERAL("name-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsNameChangeIndirect) {
  RunEventTest(FILE_PATH_LITERAL("name-change-indirect.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsDocumentTitleChange) {
  RunEventTest(FILE_PATH_LITERAL("document-title-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsRemoveChild) {
  RunEventTest(FILE_PATH_LITERAL("remove-child.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsRemoveHiddenAttribute) {
  RunEventTest(FILE_PATH_LITERAL("remove-hidden-attribute.html"));
}

// TODO(aboxhall): Fix flakiness on Windows and Mac
#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_AccessibilityEventsReportValidityInvalidField \
  DISABLED_AccessibilityEventsReportValidityInvalidField
#else
#define MAYBE_AccessibilityEventsReportValidityInvalidField \
  AccessibilityEventsReportValidityInvalidField
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       MAYBE_AccessibilityEventsReportValidityInvalidField) {
  RunEventTest(FILE_PATH_LITERAL("report-validity-invalid-field.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsRemoveHiddenAttributeSubtree) {
  RunEventTest(FILE_PATH_LITERAL("remove-hidden-attribute-subtree.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsScrollHorizontalScrollPercentChange) {
  RunEventTest(
      FILE_PATH_LITERAL("scroll-horizontal-scroll-percent-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsScrollVerticalScrollPercentChange) {
  RunEventTest(FILE_PATH_LITERAL("scroll-vertical-scroll-percent-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTabindexAddedOnPlainDiv) {
  RunEventTest(FILE_PATH_LITERAL("tabindex-added-on-plain-div.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTabindexAddedOnAriaHidden) {
  RunEventTest(FILE_PATH_LITERAL("tabindex-added-on-aria-hidden.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTabindexRemovedOnPlainDiv) {
  RunEventTest(FILE_PATH_LITERAL("tabindex-removed-on-plain-div.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTabindexRemovedOnAriaHidden) {
  RunEventTest(FILE_PATH_LITERAL("tabindex-removed-on-aria-hidden.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTableColumnHidden) {
  RunEventTest(FILE_PATH_LITERAL("table-column-hidden.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTextChanged) {
  RunEventTest(FILE_PATH_LITERAL("text-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTextChangedContentEditable) {
  RunEventTest(FILE_PATH_LITERAL("text-changed-contenteditable.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTextSelectionChanged) {
  RunEventTest(FILE_PATH_LITERAL("text-selection-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaCheckedChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-checked-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaMultiselectableChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-multiselectable-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaRequiredChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-required-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaPressedChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-pressed-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTheadFocus) {
  RunEventTest(FILE_PATH_LITERAL("thead-focus.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTfootFocus) {
  RunEventTest(FILE_PATH_LITERAL("tfoot-focus.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTbodyFocus) {
  RunEventTest(FILE_PATH_LITERAL("tbody-focus.html"));
}

// Even with the deflaking in WaitForAccessibilityTreeToContainNodeWithName,
// this test is still flaky on Windows.
// TODO(aboxhall, dmazzoni, meredithl): re-enable with better fix for above.
#if defined(OS_WIN)
#define MAYBE_AccessibilityEventsAriaSelectedChanged \
  DISABLED_AccessibilityEventsAriaSelectedChanged
#else
#define MAYBE_AccessibilityEventsAriaSelectedChanged \
  AccessibilityEventsAriaSelectedChanged
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       MAYBE_AccessibilityEventsAriaSelectedChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-selected-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsButtonClick) {
  RunEventTest(FILE_PATH_LITERAL("button-click.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       RangeValueIsReadonlyChanged) {
  RunEventTest(FILE_PATH_LITERAL("range-value-is-readonly-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest, RangeValueMaximumChanged) {
  RunEventTest(FILE_PATH_LITERAL("range-value-maximum-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest, RangeValueMinimumChanged) {
  RunEventTest(FILE_PATH_LITERAL("range-value-minimum-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest, RangeValueStepChanged) {
  RunEventTest(FILE_PATH_LITERAL("range-value-step-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest, RangeValueValueChanged) {
  RunEventTest(FILE_PATH_LITERAL("range-value-value-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest, ValueIsReadOnlyChanged) {
  RunEventTest(FILE_PATH_LITERAL("value-is-readonly-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest, ValueValueChanged) {
  RunEventTest(FILE_PATH_LITERAL("value-value-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsMenuOpenedClosed) {
  RunEventTest(FILE_PATH_LITERAL("menu-opened-closed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaFlowToChange) {
  RunEventTest(FILE_PATH_LITERAL("aria-flow-to.html"));
}

}  // namespace content
