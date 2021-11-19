// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/dump_accessibility_browsertest_base.h"

#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_command_line.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/accessibility/accessibility_event_recorder.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/public/browser/ax_inspect_factory.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/base/ui_base_features.h"

namespace content {

namespace {

// Searches recursively and returns true if an accessibility node is found
// that represents a fully loaded web document with the given url.
bool AccessibilityTreeContainsLoadedDocWithUrl(BrowserAccessibility* node,
                                               const std::string& url) {
  if (node->GetRole() == ax::mojom::Role::kRootWebArea &&
      node->GetStringAttribute(ax::mojom::StringAttribute::kUrl) == url) {
    // Ensure the doc has finished loading and has a non-zero size.
    return node->manager()->GetTreeData().loaded &&
           (node->GetData().relative_bounds.bounds.width() > 0 &&
            node->GetData().relative_bounds.bounds.height() > 0);
  }

  for (unsigned i = 0; i < node->PlatformChildCount(); i++) {
    if (AccessibilityTreeContainsLoadedDocWithUrl(node->PlatformGetChild(i),
                                                  url)) {
      return true;
    }
  }
  return false;
}

}  // namespace

using ui::AXPropertyFilter;
using ui::AXTreeFormatter;

// DumpAccessibilityTestBase
DumpAccessibilityTestBase::DumpAccessibilityTestBase()
    : enable_accessibility_after_navigating_(false), test_helper_(GetParam()) {}

DumpAccessibilityTestBase::~DumpAccessibilityTestBase() {}

void DumpAccessibilityTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  IsolateAllSitesForTesting(command_line);

  // Each test pass may require custom command-line setup.
  test_helper_.SetUpCommandLine(command_line);
}

void DumpAccessibilityTestBase::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");
  SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
}

void DumpAccessibilityTestBase::SetUp() {
  std::vector<base::Feature> enabled_features;
  std::vector<base::Feature> disabled_features;
  ChooseFeatures(&enabled_features, &disabled_features);

  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

  // The <input type="color"> popup tested in
  // AccessibilityInputColorWithPopupOpen requires the ability to read pixels
  // from a Canvas, so we need to be able to produce pixel output.
  EnablePixelOutput();

  ContentBrowserTest::SetUp();
}

void DumpAccessibilityTestBase::ChooseFeatures(
    std::vector<base::Feature>* enabled_features,
    std::vector<base::Feature>* disabled_features) {

  // Enable exposing "display: none" nodes to the browser process for testing.
  enabled_features->emplace_back(
      features::kEnableAccessibilityExposeDisplayNone);

  // For the best test coverage during development of this feature, enable the
  // code that expposes document markers on AXInlineTextBox objects and the
  // corresponding code in AXPosition on the browser that collects those
  // markers.
  enabled_features->emplace_back(features::kUseAXPositionForDocumentMarkers);

  enabled_features->emplace_back(blink::features::kPortals);
}

std::string
DumpAccessibilityTestBase::DumpUnfilteredAccessibilityTreeAsString() {
  std::unique_ptr<AXTreeFormatter> formatter(CreateFormatter());
  formatter->SetPropertyFilters({{"*", AXPropertyFilter::ALLOW}});
  formatter->set_show_ids(true);
  return formatter->Format(GetRootAccessibilityNode(GetWebContents()));
}

void DumpAccessibilityTestBase::RunTest(
    const base::FilePath file_path,
    const char* file_dir,
    const base::FilePath::StringType& expectations_qualifier) {
  RunTestForPlatform(file_path, file_dir, expectations_qualifier);
}

// TODO(accessibility) Consider renaming these things to
// WaitForAccessibiltiyClean(), Action::kRequestAccessibilityCleanNotification,
// Event::kAccessibilityClean, etc. because this can be used multiple times
// per test.
void DumpAccessibilityTestBase::WaitForEndOfTest() const {
  // To make sure we've handled all accessibility events, add a sentinel by
  // calling SignalEndOfTest and waiting for a kEndOfTest event in response.
  AccessibilityNotificationWaiter waiter(GetWebContents(), ui::kAXModeComplete,
                                         ax::mojom::Event::kEndOfTest);
  GetManager()->SignalEndOfTest();
  waiter.WaitForNotification();
}

void DumpAccessibilityTestBase::PerformAndWaitForDefaultActions() {
  // Only perform actions the first call, as they are only allowed once per
  // test, e.g. only perform the action once if this is  script is executed
  // multiple times.

  if (has_performed_default_actions_)
    return;

  has_performed_default_actions_ = true;

  // Perform default action on any elements specified by the test.
  for (const auto& str : scenario_.default_action_on) {
    // TODO(accessibility) Consider waiting for kEndOfTest instead (but change
    // the name to something more like kAccessibilityClean).
    AccessibilityNotificationWaiter waiter(
        GetWebContents(), ui::kAXModeComplete, ax::mojom::Event::kClicked);
    BrowserAccessibility* action_element;

    // TODO(accessibility) base/strings/string_split.h might be cleaner here.
    size_t parent_node_delimiter_index = str.find(",");
    if (parent_node_delimiter_index != std::string::npos) {
      auto node_name = str.substr(0, parent_node_delimiter_index);
      auto parent_node_name = str.substr(parent_node_delimiter_index + 1);

      BrowserAccessibility* parent_node = FindNode(parent_node_name);
      DCHECK(parent_node) << "Parent node name provided but not found";
      action_element = FindNode(node_name, parent_node);
    } else {
      action_element = FindNode(str);
    }

    ui::AXActionData action_data;
    action_data.action = ax::mojom::Action::kDoDefault;
    action_element->AccessibilityPerformAction(action_data);

    waiter.WaitForNotification();
  }
}

void DumpAccessibilityTestBase::WaitForExpectedText() {
  // If the original page has a @WAIT-FOR directive, don't break until
  // the text we're waiting for appears in the full text dump of the
  // accessibility tree, either.

  for (;;) {
    VLOG(1) << "Top of WaitForExpectedText() loop";
    // Check to see if the @WAIT-FOR text has appeared yet.
    bool all_wait_for_strings_found = true;
    std::string tree_dump = DumpUnfilteredAccessibilityTreeAsString();
    for (const auto& str : scenario_.wait_for) {
      if (tree_dump.find(str) == std::string::npos) {
        VLOG(1) << "Still waiting on this text to be found: " << str;
        all_wait_for_strings_found = false;
        break;
      }
    }

    // If the @WAIT-FOR text has appeared, we're done.
    if (all_wait_for_strings_found)
      break;

    // Block until the next accessibility notification in any frame.
    VLOG(1) << "Waiting until the next accessibility event";
    AccessibilityNotificationWaiter accessibility_waiter(
        GetWebContents(), ui::kAXModeComplete, ax::mojom::Event::kNone);
    accessibility_waiter.WaitForNotification();
  }
}

void DumpAccessibilityTestBase::WaitForFinalTreeContents() {
  // If @DEFAULT-ACTION-ON:[name] is used, perform the action and wait until it
  // is complete.
  PerformAndWaitForDefaultActions();

  // Wait for expected text from @WAIT-FOR.
  WaitForExpectedText();

  // Wait until all accessibility events and dirty objects have been processed.
  WaitForEndOfTest();
}

void DumpAccessibilityTestBase::RunTestForPlatform(
    const base::FilePath file_path,
    const char* file_dir,
    const base::FilePath::StringType& expectations_qualifier) {
  // Disable the "hot tracked" state (set when the mouse is hovering over
  // an object) because it makes test output change based on the mouse position.
  BrowserAccessibilityStateImpl::GetInstance()
      ->set_disable_hot_tracking_for_testing(true);

  // Normally some accessibility events that would be fired are suppressed or
  // delayed, depending on what has focus or the type of event. For testing,
  // we want all events to fire immediately to make tests predictable and not
  // flaky.
  BrowserAccessibilityManager::NeverSuppressOrDelayEventsForTesting();

  // Enable the behavior whereby all focused nodes will be exposed to the
  // platform accessibility layer. This behavior is currently disabled in
  // production code, but is enabled in tests so that it could be tested
  // thoroughly before it is turned on for all code.
  // TODO(nektar): Turn this on in a followup patch.
  // ui::AXTree::SetFocusedNodeShouldNeverBeIgnored();

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  absl::optional<ui::AXInspectScenario> scenario =
      test_helper_.ParseScenario(file_path, DefaultFilters());
  if (!scenario) {
    ADD_FAILURE()
        << "Failed to process a testing file. The file might not exist: "
        << file_path.LossyDisplayName();
    return;
  }
  scenario_ = std::move(*scenario);

  // Exit without running the test if we can't find an expectation file.
  // This is used to skip certain tests on certain platforms.
  // We have to check for this in advance in order to avoid waiting on a
  // WAIT-FOR directive in the source file that's looking for something not
  // supported on the current platform.
  base::FilePath expected_file =
      test_helper_.GetExpectationFilePath(file_path, expectations_qualifier);
  if (expected_file.empty()) {
    LOG(INFO) << "No expectation file present, ignoring test on this "
                 "platform.";
    return;
  }

  absl::optional<std::vector<std::string>> expected_lines =
      test_helper_.LoadExpectationFile(expected_file);
  if (!expected_lines) {
    LOG(INFO) << "Skipping this test on this platform.";
    return;
  }

  // Get the test URL.
  GURL url(embedded_test_server()->GetURL("/" + std::string(file_dir) + "/" +
                                          file_path.BaseName().MaybeAsASCII()));
  WebContentsImpl* web_contents = GetWebContents();

  if (enable_accessibility_after_navigating_ &&
      web_contents->GetAccessibilityMode().is_mode_off()) {
    // Load the url, then enable accessibility.
    EXPECT_TRUE(NavigateToURL(shell(), url));
    AccessibilityNotificationWaiter accessibility_waiter(
        web_contents, ui::kAXModeComplete, ax::mojom::Event::kNone);
    accessibility_waiter.WaitForNotification();
  } else {
    // Enable accessibility, then load the test html and wait for the
    // "load complete" AX event.
    AccessibilityNotificationWaiter accessibility_waiter(
        web_contents, ui::kAXModeComplete, ax::mojom::Event::kLoadComplete);
    EXPECT_TRUE(NavigateToURL(shell(), url));
    accessibility_waiter.WaitForNotification();
  }

  WaitForAllFramesLoaded();

  // Call the subclass to dump the output.
  std::vector<std::string> actual_lines = Dump();

  // Execute and wait for specified string
  for (const auto& function_name : scenario_.execute) {
    DLOG(INFO) << "executing: " << function_name;
    base::Value result =
        ExecuteScriptAndGetValue(web_contents->GetMainFrame(), function_name);
    const std::string& str = result.is_string() ? result.GetString() : "";
    // If no string is specified, do not wait.
    bool wait_for_string = str != "";
    while (wait_for_string) {
      // Loop until specified string is found.
      std::string tree_dump = DumpUnfilteredAccessibilityTreeAsString();
      if (tree_dump.find(str) != std::string::npos) {
        wait_for_string = false;
        // Append an additional dump if the specified string was found.
        std::vector<std::string> additional_dump = Dump();
        actual_lines.emplace_back("=== Start Continuation ===");
        actual_lines.insert(actual_lines.end(), additional_dump.begin(),
                            additional_dump.end());
        break;
      }
      // Block until the next accessibility notification in any frame.
      VLOG(1) << "Still waiting on this text to be found: " << str;
      VLOG(1) << "Waiting until the next accessibility event";
      // TODO(aleventhal) Try waiting for kEndOfTest to make sure all events
      // after code execution are captured.
      AccessibilityNotificationWaiter accessibility_waiter(
          web_contents, ui::AXMode(), ax::mojom::Event::kNone);
      accessibility_waiter.WaitForNotification();
    }
  }

  // Validate against the expectation file.
  bool matches_expectation = test_helper_.ValidateAgainstExpectation(
      file_path, expected_file, actual_lines, *expected_lines);
  EXPECT_TRUE(matches_expectation);
  if (!matches_expectation)
    OnDiffFailed();
}

std::vector<std::string> DumpAccessibilityTestBase::CollectAllFrameUrls(
    const std::vector<std::string>& skip_urls) {
  std::vector<std::string> all_frame_urls;
  // Get the url of every frame in the frame tree.
  for (FrameTreeNode* node : GetWebContents()->GetPrimaryFrameTree().Nodes()) {
    // Ignore about:blank urls because of the case where a parent frame A
    // has a child iframe B and it writes to the document using
    // contentDocument.open() on the child frame B.
    //
    // In this scenario, B's contentWindow.location.href matches A's url,
    // but B's url in the browser frame tree is still "about:blank".
    //
    // We also ignore frame tree nodes created for portals in the outer
    // WebContents as the node doesn't have a url set.

    std::string url = node->current_url().spec();

    // sometimes we expect a url to never load, in these cases, don't wait.
    bool skip_url = false;
    for (std::string no_load_url : skip_urls) {
      if (url.find(no_load_url) != std::string::npos) {
        skip_url = true;
        break;
      }
    }
    if (!skip_url && url != url::kAboutBlankURL && !url.empty() &&
        node->frame_owner_element_type() !=
            blink::FrameOwnerElementType::kPortal) {
      all_frame_urls.push_back(url);
    }
  }
  return all_frame_urls;
}

void DumpAccessibilityTestBase::WaitForAllFramesLoaded() {
  // Wait for the accessibility tree to fully load for all frames,
  // by searching for the WEB_AREA node in the accessibility tree
  // with the url of each frame in our frame tree. Note that this
  // doesn't support cases where there are two iframes with the
  // exact same url. If all frames haven't loaded yet, set up a
  // listener for accessibility events on any frame and block
  // until the next one is received.
  WebContentsImpl* web_contents = GetWebContents();
  for (;;) {
    VLOG(1) << "Top of WaitForAllFramesLoaded() loop";
    RenderFrameHostImpl* main_frame =
        static_cast<RenderFrameHostImpl*>(web_contents->GetMainFrame());
    BrowserAccessibilityManager* manager =
        main_frame->browser_accessibility_manager();
    if (manager) {
      BrowserAccessibility* accessibility_root = manager->GetRoot();

      // Check to see if all frames have loaded.
      bool all_frames_loaded = true;
      // A test may change the url for a frame, for example by setting
      // window.location.href, so collect the current list of urls.
      const std::vector<std::string> all_frame_urls =
          CollectAllFrameUrls(scenario_.no_load_expected);
      for (const auto& url : all_frame_urls) {
        if (!AccessibilityTreeContainsLoadedDocWithUrl(accessibility_root,
                                                       url)) {
          VLOG(1) << "Still waiting on this frame to load: " << url;
          all_frames_loaded = false;
          break;
        }
      }
      // If all frames have loaded, we're done.
      if (all_frames_loaded)
        break;
    }

    // Block until the next accessibility notification in any frame.
    VLOG(1) << "Waiting until the next accessibility event";
    AccessibilityNotificationWaiter accessibility_waiter(
        web_contents, ui::kAXModeComplete, ax::mojom::Event::kNone);
    accessibility_waiter.WaitForNotification();
  }
}

BrowserAccessibility* DumpAccessibilityTestBase::FindNode(
    const std::string& name,
    BrowserAccessibility* search_root) const {
  if (!search_root)
    search_root = GetManager()->GetRoot();

  CHECK(search_root);
  BrowserAccessibility* node = FindNodeInSubtree(*search_root, name);
  return node;
}

BrowserAccessibilityManager* DumpAccessibilityTestBase::GetManager() const {
  return GetWebContents()->GetRootBrowserAccessibilityManager();
}

WebContentsImpl* DumpAccessibilityTestBase::GetWebContents() const {
  return static_cast<WebContentsImpl*>(shell()->web_contents());
}

std::unique_ptr<AXTreeFormatter> DumpAccessibilityTestBase::CreateFormatter()
    const {
  return AXInspectFactory::CreateFormatter(GetParam());
}

std::pair<base::Value, std::vector<std::string>>
DumpAccessibilityTestBase::CaptureEvents(InvokeAction invoke_action) {
  // Create a new Event Recorder for the run.
  std::unique_ptr<ui::AXEventRecorder> event_recorder =
      AXInspectFactory::CreateRecorder(GetParam(), GetManager(),
                                       base::GetCurrentProcId(), {});
  event_recorder->SetOnlyWebEvents(true);

  event_recorder->ListenToEvents(base::BindRepeating(
      &DumpAccessibilityTestBase::OnEventRecorded, base::Unretained(this)));

  LOG(INFO) << "-------------- Start listening to events --------------";

  // If @DEFAULT-ACTION-ON:[name] is used, perform the action and wait until
  // it is complete.
  PerformAndWaitForDefaultActions();

  // Create a waiter that waits for any one accessibility event.
  // This will ensure that after calling the go() function, we
  // block until we've received an accessibility event generated as
  // a result of this function.
  AccessibilityNotificationWaiter waiter(GetWebContents(), ui::kAXModeComplete,
                                         ax::mojom::Event::kNone);

  // Run any script, e.g. go().
  // If an action was performed, we already waited for the kClicked event in
  // PerformAndWaitForDefaultActions(), which means the action is already
  // completed.
  base::Value action_result = std::move(invoke_action).Run();

  // Wait for at least one event. This may unblock either when |waiter|
  // observes either an ax::mojom::Event or ui::AXEventGenerator::Event, or
  // when |event_recorder| records a platform event.
  waiter.WaitForNotification();

  // More than one accessibility event could have been generated.
  // To make sure we've received all accessibility events, add a
  // sentinel by calling SignalEndOfTest and waiting for a kEndOfTest
  // event in response.
  WaitForEndOfTest();
  event_recorder->WaitForDoneRecording();

  LOG(INFO) << "-------------- Stop listening to events --------------";

  // Dump the event logs, running them through any filters specified
  // in the HTML file.
  std::vector<std::string> event_logs = event_recorder->GetEventLogs();

  // Sort the logs so that results are predictable. There are too many
  // nondeterministic things that affect the exact order of events fired,
  // so these tests shouldn't be used to make assertions about event order.
  std::sort(event_logs.begin(), event_logs.end());

  return std::make_pair(std::move(action_result), std::move(event_logs));
}

BrowserAccessibility* DumpAccessibilityTestBase::FindNodeInSubtree(
    BrowserAccessibility& node,
    const std::string& name) const {
  if (node.GetStringAttribute(ax::mojom::StringAttribute::kName) == name)
    return &node;

  for (unsigned int i = 0; i < node.PlatformChildCount(); ++i) {
    BrowserAccessibility* result =
        FindNodeInSubtree(*node.PlatformGetChild(i), name);
    if (result)
      return result;
  }
  return nullptr;
}

bool DumpAccessibilityTestBase::HasHtmlAttribute(
    BrowserAccessibility& node,
    const char* attr,
    const std::string& value) const {
  std::string result;
  if (node.GetHtmlAttribute(attr, &result))
    return result == value;

  if (base::LowerCaseEqualsASCII(attr, "class"))
    return node.GetStringAttribute(ax::mojom::StringAttribute::kClassName) ==
           value;

  return false;
}

BrowserAccessibility* DumpAccessibilityTestBase::FindNodeByHTMLAttribute(
    const char* attr,
    const std::string& value) const {
  BrowserAccessibility* root = GetManager()->GetRoot();

  CHECK(root);
  return FindNodeByHTMLAttributeInSubtree(*root, attr, value);
}

BrowserAccessibility*
DumpAccessibilityTestBase::FindNodeByHTMLAttributeInSubtree(
    BrowserAccessibility& node,
    const char* attr,
    const std::string& value) const {
  if (HasHtmlAttribute(node, attr, value))
    return &node;

  for (unsigned int i = 0; i < node.PlatformChildCount(); ++i) {
    if (BrowserAccessibility* result = FindNodeByHTMLAttributeInSubtree(
            *node.PlatformGetChild(i), attr, value))
      return result;
  }
  return nullptr;
}

}  // namespace content
