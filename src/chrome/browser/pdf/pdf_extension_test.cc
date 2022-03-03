// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/ignore_result.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/test_timeouts.h"
#include "base/test/with_feature_override.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/pdf/pdf_extension_util.h"
#include "chrome/browser/pdf/pdf_frame_util.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/plugins/plugin_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/download/public/common/download_item.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "components/lens/lens_features.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/viz/common/features.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/test/zoom_test_utils.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/ax_inspect_factory.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/dump_accessibility_test_helper.h"
#include "content/public/test/focus_changed_observer.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/scoped_time_zone.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/text_input_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/file_system/file_system_api.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "pdf/buildflags.h"
#include "pdf/pdf_features.h"
#include "printing/buildflags/buildflags.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/context_menu_data/untrustworthy_context_menu_params.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/platform/ax_platform_node_delegate_base.h"
#include "ui/accessibility/platform/inspect/ax_api_type.h"
#include "ui/accessibility/platform/inspect/ax_inspect_scenario.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/point.h"
#include "url/gurl.h"

#if defined(TOOLKIT_VIEWS) && !defined(OS_MAC)
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/printing/print_view_manager_base.h"
#include "chrome/browser/ui/browser_commands.h"
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_view_manager.h"
#include "ui/base/ui_base_types.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif
#endif  // BUILDFLAG(ENABLE_PRINTING)

namespace {

using ::content::AXInspectFactory;
using ::content::WebContents;
using ::extensions::ExtensionsAPIClient;
using ::guest_view::GuestViewManager;
using ::guest_view::TestGuestViewManager;
using ::guest_view::TestGuestViewManagerFactory;
using ::pdf_extension_test_util::ConvertPageCoordToScreenCoord;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::StartsWith;
using ::ui::AXTreeFormatter;

const int kNumberLoadTestParts = 10;

#if defined(OS_MAC)
const int kDefaultKeyModifier = blink::WebInputEvent::kMetaKey;
#else
const int kDefaultKeyModifier = blink::WebInputEvent::kControlKey;
#endif

// Calling PluginService::GetPlugins ensures that LoadPlugins is called
// internally. This is an asynchronous task and this method uses a run loop to
// wait for the loading task to complete.
void WaitForPluginServiceToLoad() {
  base::RunLoop run_loop;
  content::PluginService::GetPluginsCallback callback = base::BindOnce(
      [](base::RepeatingClosure quit,
         const std::vector<content::WebPluginInfo>& unused) { quit.Run(); },
      run_loop.QuitClosure());
  content::PluginService::GetInstance()->GetPlugins(std::move(callback));
  run_loop.Run();
}

void WaitForLoadStart(WebContents* web_contents) {
  while (!web_contents->IsLoading() &&
         !web_contents->GetController().GetLastCommittedEntry()) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }
}
}  // namespace

// Using ASSERT_TRUE deliberately instead of ASSERT_EQ or ASSERT_STREQ
// in order to print a more readable message if the strings differ.
#define ASSERT_MULTILINE_STREQ(expected, actual) \
    ASSERT_TRUE(expected == actual) \
        << "Expected:\n" << expected \
        << "\n\nActual:\n" << actual

class PDFExtensionTestWithoutUnseasonedOverride
    : public extensions::ExtensionApiTest {
 public:
  ~PDFExtensionTestWithoutUnseasonedOverride() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);

    feature_list_.InitWithFeatures(GetEnabledFeatures(), GetDisabledFeatures());
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    content::SetupCrossSiteRedirector(embedded_test_server());
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    extensions::ExtensionApiTest::TearDownOnMainThread();
  }

  // Serve paths prefixed with _test_resources/ from chrome/test/data.
  base::FilePath GetTestResourcesParentDir() override {
    base::FilePath test_root_path;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_root_path);
    return test_root_path;
  }

  bool PdfIsExpectedToLoad(const std::string& pdf_file) {
    const char* const kFailingPdfs[] = {
        // clang-format off
        "pdf/test-ranges.pdf",
        "pdf_private/accessibility_crash_1.pdf",
        "pdf_private/cfuzz5.pdf",
        "pdf_private/js.pdf",
        "pdf_private/segv-ecx.pdf",
        "pdf_private/tests.pdf",
        // clang-format on
    };
    for (const char* failing_pdf : kFailingPdfs) {
      if (failing_pdf == pdf_file)
        return false;
    }
    return true;
  }

  // Load the PDF at the given URL and ensure it has finished loading. Return
  // true if it loads successfully or false if it fails. If it doesn't finish
  // loading the test will hang. This is done from outside of the BrowserPlugin
  // guest to ensure sending messages to/from the plugin works correctly from
  // there, since the PDFScriptingAPI relies on doing this as well.
  testing::AssertionResult LoadPdf(const GURL& url) {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    WebContents* web_contents = GetActiveWebContents();
    return pdf_extension_test_util::EnsurePDFHasLoaded(web_contents);
  }

  // Same as LoadPDF(), but loads into a new tab.
  testing::AssertionResult LoadPdfInNewTab(const GURL& url) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    WebContents* web_contents = GetActiveWebContents();
    return pdf_extension_test_util::EnsurePDFHasLoaded(web_contents);
  }

  // Same as LoadPdf(), but also returns a pointer to the guest WebContents for
  // the loaded PDF. Returns nullptr if the load fails.
  WebContents* LoadPdfGetGuestContents(const GURL& url) {
    if (!LoadPdf(url))
      return nullptr;
    return GetOnlyGuestContents(GetActiveWebContents());
  }

  // Same as LoadPdf(), but also returns a pointer to the guest WebContents for
  // the loaded PDF in a new tab. Returns nullptr if the load fails.
  WebContents* LoadPdfInNewTabGetGuestContents(const GURL& url) {
    if (!LoadPdfInNewTab(url))
      return nullptr;
    return GetOnlyGuestContents(GetActiveWebContents());
  }

  void TestGetSelectedTextReply(const GURL& url, bool expect_success) {
    WebContents* guest_contents = LoadPdfGetGuestContents(url);
    ASSERT_TRUE(guest_contents);

    // Reach into the guest and hook into it such that it posts back a 'flush'
    // message after every getSelectedTextReply message sent.
    ASSERT_TRUE(content::ExecuteScript(
        guest_contents, "viewer.overrideSendScriptingMessageForTest();"));

    // Add an event listener for flush messages and request the selected text.
    // If we get a flush message without receiving getSelectedText we know that
    // the message didn't come through.
    bool success = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        GetActiveWebContents(),
        "window.addEventListener('message', function(event) {"
        "  if (event.data == 'flush')"
        "    window.domAutomationController.send(false);"
        "  if (event.data.type == 'getSelectedTextReply')"
        "    window.domAutomationController.send(true);"
        "});"
        "document.getElementsByTagName('embed')[0].postMessage("
        "    {type: 'getSelectedText'});",
        &success));
    ASSERT_EQ(expect_success, success);
  }

  WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Synchronously sets the input focus on the plugin frame by clicking on the
  // top left corner of a PDF document.
  void SetInputFocusOnPlugin(WebContents* guest_contents) {
    content::FocusChangedObserver focus_observer(guest_contents);
    content::SimulateMouseClickAt(
        guest_contents, blink::WebInputEvent::kNoModifiers,
        blink::WebMouseEvent::Button::kLeft,
        ConvertPageCoordToScreenCoord(guest_contents, {1, 1}));
    focus_observer.Wait();
  }

 protected:
  WebContents* GetOnlyGuestContents(WebContents* embedder_contents) const {
    content::BrowserPluginGuestManager* guest_manager =
        embedder_contents->GetBrowserContext()->GetGuestManager();

    WebContents* guest_contents = nullptr;
    guest_manager->ForEachGuest(
        embedder_contents,
        base::BindLambdaForTesting([&guest_contents](WebContents* guest) {
          // Assume exactly one guest contents.
          EXPECT_FALSE(guest_contents);
          guest_contents = guest;
          return false;
        }));

    return guest_contents;
  }

  content::RenderFrameHost* GetPluginFrame(WebContents* guest_contents) const {
    content::RenderFrameHost* main_frame = guest_contents->GetMainFrame();
    return IsUnseasoned() ? pdf_frame_util::FindPdfChildFrame(main_frame)
                          : main_frame;
  }

  // Finds the `RenderFrameHost`s of Unseasoned PDF plugins within a given
  // `WebContents`. This method should only be used in Unseasoned PDF tests.
  std::vector<content::RenderFrameHost*> GetUnseasonedPdfFrames(
      WebContents* contents) {
    EXPECT_TRUE(IsUnseasoned());

    std::vector<content::RenderFrameHost*> pdf_frames;
    contents->ForEachRenderFrameHost(base::BindLambdaForTesting(
        [&pdf_frames](content::RenderFrameHost* frame) {
          if (IsUnseasonedPdfFrame(*frame))
            pdf_frames.push_back(frame);
        }));
    return pdf_frames;
  }

  int CountPDFProcesses() {
    return IsUnseasoned() ? CountUnseasonedPDFProcesses()
                          : CountPepperPDFProcesses();
  }

  ui::AXTreeUpdate GetAccessibilityTreeSnapshotForPdf(
      WebContents* web_contents) {
    ui::AXTreeUpdate ax_tree = GetAccessibilityTreeSnapshot(web_contents);
    if (!IsUnseasoned())
      return ax_tree;

    // In the Unseasoned viewer, the PDF contents are located in a child tree.
    ui::AXTreeID child_tree_id;
    for (const ui::AXNodeData& node : ax_tree.nodes) {
      if (node.HasStringAttribute(ax::mojom::StringAttribute::kChildTreeId)) {
        child_tree_id = ui::AXTreeID::FromString(
            node.GetStringAttribute(ax::mojom::StringAttribute::kChildTreeId));
        break;
      }
    }

    EXPECT_NE(child_tree_id, ui::AXTreeIDUnknown());
    return content::GetAccessibilityTreeSnapshotFromId(child_tree_id);
  }

  bool IsUnseasoned() const {
    return base::FeatureList::IsEnabled(chrome_pdf::features::kPdfUnseasoned);
  }

  // Hooks to set up feature flags.
  virtual std::vector<base::Feature> GetEnabledFeatures() const { return {}; }

  virtual std::vector<base::Feature> GetDisabledFeatures() const { return {}; }

 private:
  int CountPepperPDFProcesses() {
    auto* service = content::PluginService::GetInstance();
    return service->CountPpapiPluginProcessesForProfile(
        base::FilePath(ChromeContentClient::kPDFPluginPath),
        browser()->profile()->GetPath());
  }

  int CountUnseasonedPDFProcesses() {
    base::flat_set<content::RenderProcessHost*> pdf_processes;

    const TabStripModel* tab_strip = browser()->tab_strip_model();
    for (int tab = 0; tab < tab_strip->count(); ++tab) {
      for (content::RenderFrameHost* pdf_frame :
           GetUnseasonedPdfFrames(tab_strip->GetWebContentsAt(tab))) {
        pdf_processes.insert(pdf_frame->GetProcess());
      }
    }

    return pdf_processes.size();
  }

  static bool IsUnseasonedPdfFrame(content::RenderFrameHost& frame) {
    if (!frame.GetProcess()->IsPdf())
      return false;

    EXPECT_TRUE(frame.IsCrossProcessSubframe());
    return true;
  }

  base::test::ScopedFeatureList feature_list_;
};

class PDFExtensionTest : public base::test::WithFeatureOverride,
                         public PDFExtensionTestWithoutUnseasonedOverride {
 public:
  PDFExtensionTest()
      : base::test::WithFeatureOverride(chrome_pdf::features::kPdfUnseasoned) {}
};

class PDFExtensionTestWithPartialLoading : public PDFExtensionTest {
 protected:
  std::vector<base::Feature> GetEnabledFeatures() const override {
    auto enabled = PDFExtensionTest::GetEnabledFeatures();
    enabled.push_back(chrome_pdf::features::kPdfIncrementalLoading);
    enabled.push_back(chrome_pdf::features::kPdfPartialLoading);
    return enabled;
  }
};

class PDFExtensionTestWithTestGuestViewManager : public PDFExtensionTest {
 public:
  PDFExtensionTestWithTestGuestViewManager() {
    GuestViewManager::set_factory_for_testing(&factory_);
  }

 protected:
  TestGuestViewManager* GetGuestViewManager() {
    // TODO(wjmaclean): Re-implement FromBrowserContext in the
    // TestGuestViewManager class to avoid all callers needing this cast.
    auto* manager = static_cast<TestGuestViewManager*>(
        TestGuestViewManager::FromBrowserContext(browser()->profile()));
    // TestGuestViewManager::WaitForSingleGuestCreated can and will get called
    // before a guest is created. Since GuestViewManager is usually not created
    // until the first guest is created, this means that |manager| will be
    // nullptr if trying to use the manager to wait for the first guest. Because
    // of this, the manager must be created here if it does not already exist.
    if (!manager) {
      manager = static_cast<TestGuestViewManager*>(
          GuestViewManager::CreateWithDelegate(
              browser()->profile(),
              ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate(
                  browser()->profile())));
    }
    return manager;
  }

 private:
  TestGuestViewManagerFactory factory_;
};

// This test is a re-implementation of
// WebPluginContainerTest.PluginDocumentPluginIsFocused, which was introduced
// for https://crbug.com/536637. The original implementation checked that the
// BrowserPlugin hosting the pdf extension was focused; in this re-write, we
// make sure the guest view's WebContents has focus.
IN_PROC_BROWSER_TEST_P(PDFExtensionTestWithTestGuestViewManager,
                       PdfInMainFrameHasFocus) {
  // Load test HTML, and verify the text area has focus.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/pdf/test.pdf")));
  auto* embedder_web_contents = GetActiveWebContents();

  // Verify the pdf has loaded.
  auto* guest_web_contents = GetGuestViewManager()->WaitForSingleGuestCreated();
  ASSERT_TRUE(guest_web_contents);
  EXPECT_NE(embedder_web_contents, guest_web_contents);
  WaitForLoadStart(guest_web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(guest_web_contents));

  // Make sure the guest WebContents has focus.
  EXPECT_EQ(guest_web_contents,
            content::GetFocusedWebContents(embedder_web_contents));
}

// TODO(crbug.com/1278357): Flaky on lacros.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_PdfExtensionLoadedInGuest DISABLED_PdfExtensionLoadedInGuest
#else
#define MAYBE_PdfExtensionLoadedInGuest PdfExtensionLoadedInGuest
#endif
// This test verifies that when a PDF is loaded, that (i) the embedder
// WebContents' html consists of a single <embed> tag with appropriate
// properties, and (ii) that the guest WebContents finishes loading and
// has the correct URL for the PDF extension.
// TODO(wjmaclean): Are there any attributes we can/should test with respect to
// the extension's loaded html?
IN_PROC_BROWSER_TEST_P(PDFExtensionTestWithTestGuestViewManager,
                       MAYBE_PdfExtensionLoadedInGuest) {
  // Load test HTML, and verify the text area has focus.
  const GURL main_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  auto* embedder_web_contents = GetActiveWebContents();

  // Verify the pdf has loaded.
  auto* guest_web_contents = GetGuestViewManager()->WaitForSingleGuestCreated();
  ASSERT_TRUE(guest_web_contents);
  EXPECT_NE(embedder_web_contents, guest_web_contents);
  WaitForLoadStart(guest_web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(guest_web_contents));

  // Verify we loaded the extension.
  const GURL extension_url(
      "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html");
  EXPECT_EQ(extension_url, guest_web_contents->GetLastCommittedURL());
  EXPECT_EQ(main_url, embedder_web_contents->GetLastCommittedURL());

  // Make sure the embedder has the correct html boilerplate.
  EXPECT_EQ(1, content::EvalJs(embedder_web_contents,
                               "document.body.children.length;")
                   .ExtractInt());
  EXPECT_EQ("EMBED", content::EvalJs(embedder_web_contents,
                                     "document.body.firstChild.tagName;")
                         .ExtractString());
  EXPECT_EQ("application/pdf", content::EvalJs(embedder_web_contents,
                                               "document.body.firstChild.type;")
                                   .ExtractString());
  EXPECT_EQ("about:blank", content::EvalJs(embedder_web_contents,
                                           "document.body.firstChild.src;")
                               .ExtractString());
  EXPECT_TRUE(
      content::EvalJs(embedder_web_contents,
                      "document.body.firstChild.hasAttribute('internalid');")
          .ExtractBool());
}

// This test verifies that when a PDF is served with a restrictive
// Content-Security-Policy, the embed tag is still sized correctly.
// Regression test for https://crbug.com/271452.
IN_PROC_BROWSER_TEST_P(PDFExtensionTestWithTestGuestViewManager,
                       CSPDoesNotBlockEmbedStyles) {
  const GURL main_url(embedded_test_server()->GetURL("/pdf/test-csp.pdf"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  auto* embedder_web_contents = GetActiveWebContents();
  ASSERT_TRUE(embedder_web_contents);

  // Verify the pdf has loaded.
  auto* guest_web_contents = GetGuestViewManager()->WaitForSingleGuestCreated();
  ASSERT_TRUE(guest_web_contents);
  EXPECT_NE(embedder_web_contents, guest_web_contents);
  WaitForLoadStart(guest_web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(guest_web_contents));

  // Verify the extension was loaded.
  const GURL extension_url(
      "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html");
  EXPECT_EQ(extension_url, guest_web_contents->GetLastCommittedURL());
  EXPECT_EQ(main_url, embedder_web_contents->GetLastCommittedURL());

  // Verify that the plugin occupies all of the page area.
  const gfx::Rect embedder_rect = embedder_web_contents->GetContainerBounds();
  const gfx::Rect guest_rect = guest_web_contents->GetContainerBounds();
  EXPECT_EQ(embedder_rect, guest_rect);
}

// This test verifies that when a PDF is served with
// Content-Security-Policy: sandbox, this is ignored and the PDF is displayed.
// Regression test for https://crbug.com/1187122.
IN_PROC_BROWSER_TEST_P(PDFExtensionTestWithTestGuestViewManager,
                       CSPWithSandboxDoesNotBlockPDF) {
  const GURL main_url(
      embedded_test_server()->GetURL("/pdf/test-csp-sandbox.pdf"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  auto* embedder_web_contents = GetActiveWebContents();
  ASSERT_TRUE(embedder_web_contents);

  // Verify the pdf has loaded.
  auto* guest_web_contents = GetGuestViewManager()->WaitForSingleGuestCreated();
  ASSERT_TRUE(guest_web_contents);
  EXPECT_NE(embedder_web_contents, guest_web_contents);
  WaitForLoadStart(guest_web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(guest_web_contents));

  // Verify the extension was loaded.
  const GURL extension_url(
      "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html");
  EXPECT_EQ(extension_url, guest_web_contents->GetLastCommittedURL());
  EXPECT_EQ(main_url, embedder_web_contents->GetLastCommittedURL());
}

// This test verifies that Content-Security-Policy's frame-ancestors 'none'
// directive is effective on a PDF response.
// Regression test for https://crbug.com/1107535.
IN_PROC_BROWSER_TEST_P(PDFExtensionTestWithTestGuestViewManager,
                       CSPFrameAncestorsCanBlockEmbedding) {
  WebContents* web_contents = GetActiveWebContents();
  content::WebContentsConsoleObserver console_observer(web_contents);
  console_observer.SetPattern(
      "*because an ancestor violates the following Content Security Policy "
      "directive: \"frame-ancestors 'none'*");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/pdf/frame-test-csp-frame-ancestors-none.html")));

  console_observer.Wait();

  EXPECT_EQ(0, CountPDFProcesses());
}

// This test verifies that Content-Security-Policy's frame-ancestors directive
// overrides an X-Frame-Options header on a PDF response.
// Regression test for https://crbug.com/1107535.
IN_PROC_BROWSER_TEST_P(PDFExtensionTestWithTestGuestViewManager,
                       CSPFrameAncestorsOverridesXFrameOptions) {
  const GURL main_url(
      embedded_test_server()->GetURL("/pdf/frame-test-csp-and-xfo.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  auto* embedder_web_contents = GetActiveWebContents();
  ASSERT_TRUE(embedder_web_contents);

  // Verify the pdf has loaded.
  auto* guest_web_contents = GetGuestViewManager()->WaitForSingleGuestCreated();
  ASSERT_TRUE(guest_web_contents);
  EXPECT_NE(embedder_web_contents, guest_web_contents);
  WaitForLoadStart(guest_web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(guest_web_contents));

  // Verify the extension was loaded.
  const GURL extension_url(
      "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html");
  EXPECT_EQ(extension_url, guest_web_contents->GetLastCommittedURL());
  EXPECT_EQ(main_url, embedder_web_contents->GetLastCommittedURL());
}

class PDFExtensionLoadTest
    : public PDFExtensionTestWithoutUnseasonedOverride,
      public testing::WithParamInterface<std::tuple<int, bool>> {
 protected:
  int part() const { return std::get<0>(GetParam()); }
  bool should_enable_unseasoned() const { return std::get<1>(GetParam()); }

  std::vector<base::Feature> GetEnabledFeatures() const override {
    auto enabled =
        PDFExtensionTestWithoutUnseasonedOverride::GetEnabledFeatures();
    if (should_enable_unseasoned())
      enabled.push_back(chrome_pdf::features::kPdfUnseasoned);
    return enabled;
  }

  std::vector<base::Feature> GetDisabledFeatures() const override {
    auto disabled =
        PDFExtensionTestWithoutUnseasonedOverride::GetDisabledFeatures();
    if (!should_enable_unseasoned())
      disabled.push_back(chrome_pdf::features::kPdfUnseasoned);
    return disabled;
  }

  // Load all the PDFs contained in chrome/test/data/<dir_name>. The files are
  // sharded across kNumberLoadTestParts using base::PersistentHash(filename).
  void LoadAllPdfsTest(const std::string& dir_name) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    base::FileEnumerator file_enumerator(test_data_dir.AppendASCII(dir_name),
                                         false, base::FileEnumerator::FILES,
                                         FILE_PATH_LITERAL("*.pdf"));

    size_t count = 0;
    for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
         file_path = file_enumerator.Next()) {
      std::string filename = file_path.BaseName().MaybeAsASCII();
      ASSERT_FALSE(filename.empty());

      std::string pdf_file = dir_name + "/" + filename;
      SCOPED_TRACE(pdf_file);
      if (base::PersistentHash(filename) % kNumberLoadTestParts == part()) {
        LOG(INFO) << "Loading: " << pdf_file;
        testing::AssertionResult success =
            LoadPdf(embedded_test_server()->GetURL("/" + pdf_file));
        if (pdf_file == "pdf_private/cfuzz5.pdf")
          continue;
        EXPECT_EQ(PdfIsExpectedToLoad(pdf_file), success) << pdf_file;
      }
      ++count;
    }
    // Assume that there is at least 1 pdf in the directory to guard against
    // someone deleting the directory and silently making the test pass.
    ASSERT_GE(count, 1u);
  }
};

IN_PROC_BROWSER_TEST_P(PDFExtensionLoadTest, Load) {
  LoadAllPdfsTest("pdf");
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_P(PDFExtensionLoadTest, LoadPrivate) {
  LoadAllPdfsTest("pdf_private");
}
#endif

// We break PDFExtensionLoadTest up into kNumberLoadTestParts.
INSTANTIATE_TEST_SUITE_P(
    PDFTestFiles,
    PDFExtensionLoadTest,
    testing::Combine(testing::Range(0, kNumberLoadTestParts), testing::Bool()));

using PDFExtensionBlobNavigationTest = PDFExtensionTest;

IN_PROC_BROWSER_TEST_P(PDFExtensionBlobNavigationTest, NewTab) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/pdf/blob_navigation_new_tab.html")));

  // Calling `window.open` without a user gesture will be blocked by the popup
  // blocker. `ExecJs()` emulates a user gesture which bypasses the restriction.
  content::TestNavigationObserver navigation_observer(nullptr);
  navigation_observer.StartWatchingNewWebContents();
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), "openBlobPdfInNewTab()"));
  navigation_observer.Wait();

  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
  WebContents* new_tab_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(new_tab_contents));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionBlobNavigationTest, SameTab) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL("/pdf/blob_navigation_same_tab.html"),
      /*number_of_navigations=*/2));
  EXPECT_TRUE(
      pdf_extension_test_util::EnsurePDFHasLoaded(GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionTest, LoadInPlatformApp) {
  extensions::TestExtensionDir dir;
  dir.WriteManifest(R"(
    {
      "name": "PDFExtensionTest App",
      "version": "1.0",
      "manifest_version": 2,
      "app": {
        "background": {
          "scripts": ["background_script.js"]
        }
      }
    }
  )");

  dir.WriteFile(FILE_PATH_LITERAL("background_script.js"), R"(
    chrome.app.runtime.onLaunched.addListener(() => {
      chrome.app.window.create('test.pdf', () => {
        chrome.test.notifyPass();
      });
    });
  )");

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string pdf_contents;
    ASSERT_TRUE(base::ReadFileToString(
        GetTestResourcesParentDir().AppendASCII("pdf").AppendASCII("test.pdf"),
        &pdf_contents));
    dir.WriteFile(FILE_PATH_LITERAL("test.pdf"), pdf_contents);
  }

  extensions::ResultCatcher result_catcher;
  ASSERT_TRUE(LoadAndLaunchApp(dir.UnpackedPath()));
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();

  auto* app_registry = extensions::AppWindowRegistry::Get(browser()->profile());
  ASSERT_TRUE(app_registry);
  const extensions::AppWindowRegistry::AppWindowList& app_windows =
      app_registry->app_windows();
  ASSERT_EQ(app_windows.size(), 1u);
  extensions::AppWindow* window = app_windows.front();
  ASSERT_TRUE(window);
  content::WebContents* app_contents = window->web_contents();

  ASSERT_TRUE(content::WaitForLoadStop(app_contents));
  EXPECT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(app_contents));
}

class DownloadAwaiter : public content::DownloadManager::Observer {
 public:
  DownloadAwaiter() {}
  ~DownloadAwaiter() override {}

  const GURL& GetLastUrl() {
    // Wait until the download has been created.
    download_run_loop_.Run();
    return last_url_;
  }

  // content::DownloadManager::Observer implementation.
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override {
    last_url_ = item->GetURL();
    download_run_loop_.Quit();
  }

 private:
  base::RunLoop download_run_loop_;
  GURL last_url_;
};

// Tests behavior when the PDF plugin is disabled in preferences.
class PDFPluginDisabledTest : public PDFExtensionTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PDFExtensionTest::SetUpCommandLine(command_line);

    command_line->AppendSwitch(switches::kEnablePluginPlaceholderTesting);
  }

  void SetUpOnMainThread() override {
    PDFExtensionTest::SetUpOnMainThread();

    content::BrowserContext* browser_context =
        GetActiveWebContents()->GetBrowserContext();
    Profile* profile = Profile::FromBrowserContext(browser_context);
    profile->GetPrefs()->SetBoolean(prefs::kPluginsAlwaysOpenPdfExternally,
                                    true);

    content::DownloadManager* download_manager =
        browser_context->GetDownloadManager();
    download_awaiter_ = std::make_unique<DownloadAwaiter>();
    download_manager->AddObserver(download_awaiter_.get());
  }

  void TearDownOnMainThread() override {
    content::BrowserContext* browser_context =
        GetActiveWebContents()->GetBrowserContext();
    content::DownloadManager* download_manager =
        browser_context->GetDownloadManager();
    download_manager->RemoveObserver(download_awaiter_.get());

    // Cancel all downloads to shut down cleanly.
    std::vector<download::DownloadItem*> downloads;
    download_manager->GetAllDownloads(&downloads);
    for (auto* item : downloads) {
      item->Cancel(false);
    }

    PDFExtensionTest::TearDownOnMainThread();
  }

  void ClickOpenButtonInIframe() {
    content::RenderFrameHost* iframe_render_frame_host =
        ChildFrameAt(GetActiveWebContents(), 0);
    ASSERT_TRUE(iframe_render_frame_host);
    ASSERT_TRUE(
        content::ExecJs(iframe_render_frame_host,
                        "document.getElementById('open-button').click();"));
  }

  void ValidateSingleSuccessfulDownloadAndNoPDFPluginLaunch() {
    // Validate that we downloaded a single PDF and didn't launch the PDF
    // plugin.
    EXPECT_EQ(embedded_test_server()->GetURL("/pdf/test.pdf"),
              AwaitAndGetLastDownloadedUrl());
    EXPECT_EQ(1u, GetNumberOfDownloads());
    EXPECT_EQ(0, CountPDFProcesses());
  }

 private:
  size_t GetNumberOfDownloads() {
    content::BrowserContext* browser_context =
        GetActiveWebContents()->GetBrowserContext();
    content::DownloadManager* download_manager =
        browser_context->GetDownloadManager();

    std::vector<download::DownloadItem*> downloads;
    download_manager->GetAllDownloads(&downloads);
    return downloads.size();
  }

  const GURL& AwaitAndGetLastDownloadedUrl() {
    return download_awaiter_->GetLastUrl();
  }

  std::unique_ptr<DownloadAwaiter> download_awaiter_;
};

IN_PROC_BROWSER_TEST_P(PDFPluginDisabledTest, DirectNavigationToPDF) {
  // Navigate to a PDF and test that it is downloaded.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/pdf/test.pdf")));

  ValidateSingleSuccessfulDownloadAndNoPDFPluginLaunch();
}

// TODO(crbug.com/1201401): fix flakiness and reenable
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC)
#define MAYBE_EmbedPdfPlaceholderWithCSP DISABLED_EmbedPdfPlaceholderWithCSP
#else
#define MAYBE_EmbedPdfPlaceholderWithCSP EmbedPdfPlaceholderWithCSP
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC)
IN_PROC_BROWSER_TEST_P(PDFPluginDisabledTest,
                       MAYBE_EmbedPdfPlaceholderWithCSP) {
  // Navigate to a page with CSP that uses <embed> to embed a PDF as a plugin.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/pdf/pdf_embed_csp.html")));
  PluginTestUtils::WaitForPlaceholderReady(GetActiveWebContents(), "pdf_embed");

  // Fake a click on the <embed>, then press Enter to trigger the download.
  gfx::Point point_in_pdf(100, 100);
  content::SimulateMouseClickAt(GetActiveWebContents(), kDefaultKeyModifier,
                                blink::WebMouseEvent::Button::kLeft,
                                point_in_pdf);
  content::SimulateKeyPress(GetActiveWebContents(), ui::DomKey::ENTER,
                            ui::DomCode::ENTER, ui::VKEY_RETURN, false, false,
                            false, false);

  ValidateSingleSuccessfulDownloadAndNoPDFPluginLaunch();
}

IN_PROC_BROWSER_TEST_P(PDFPluginDisabledTest, IframePdfPlaceholderWithCSP) {
  // Navigate to a page that uses <iframe> to embed a PDF as a plugin.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/pdf/pdf_iframe_csp.html")));

  ClickOpenButtonInIframe();
  ValidateSingleSuccessfulDownloadAndNoPDFPluginLaunch();
}

IN_PROC_BROWSER_TEST_P(PDFPluginDisabledTest,
                       IframePlaceholderInjectedIntoNewWindow) {
  // This is an unusual test to verify crbug.com/924823. We are injecting the
  // HTML for a PDF IFRAME into a newly created popup with an undefined URL.
  ASSERT_TRUE(
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace(
              "new Promise((resolve) => {"
              "  var popup = window.open();"
              "  popup.document.writeln("
              "      '<iframe id=\"pdf_iframe\" src=\"' + $1 + '\"></iframe>');"
              "  var iframe = popup.document.getElementById('pdf_iframe');"
              "  iframe.onload = () => resolve(true);"
              "});",
              embedded_test_server()->GetURL("/pdf/test.pdf").spec()))
          .ExtractBool());

  ClickOpenButtonInIframe();
  ValidateSingleSuccessfulDownloadAndNoPDFPluginLaunch();
}

class PDFExtensionJSTest : public PDFExtensionTest {
 protected:
  void RunTestsInJsModule(const std::string& filename,
                          const std::string& pdf_filename) {
    RunTestsInJsModuleHelper(filename, pdf_filename, /*new_tab=*/false);
  }

  void RunTestsInJsModuleNewTab(const std::string& filename,
                                const std::string& pdf_filename) {
    RunTestsInJsModuleHelper(filename, pdf_filename, /*new_tab=*/true);
  }

 private:
  // Runs the extensions test at chrome/test/data/pdf/<filename> on the PDF file
  // at chrome/test/data/pdf/<pdf_filename>, where |filename| is loaded as a JS
  // module.
  void RunTestsInJsModuleHelper(const std::string& filename,
                                const std::string& pdf_filename,
                                bool new_tab) {
    extensions::ResultCatcher catcher;

    GURL url(embedded_test_server()->GetURL("/pdf/" + pdf_filename));

    // It should be good enough to just navigate to the URL. But loading up the
    // BrowserPluginGuest seems to happen asynchronously as there was flakiness
    // being seen due to the BrowserPluginGuest not being available yet (see
    // crbug.com/498077). So instead use LoadPdf() which ensures that the PDF is
    // loaded before continuing.
    WebContents* guest_contents = new_tab ? LoadPdfInNewTabGetGuestContents(url)
                                          : LoadPdfGetGuestContents(url);
    ASSERT_TRUE(guest_contents);

    constexpr char kModuleLoaderTemplate[] =
        R"(var s = document.createElement('script');
           s.type = 'module';
           s.src = '_test_resources/pdf/%s';
           document.body.appendChild(s);)";

    ASSERT_TRUE(content::ExecuteScript(
        guest_contents,
        base::StringPrintf(kModuleLoaderTemplate, filename.c_str())));

    if (!catcher.GetNextResult())
      FAIL() << catcher.message();
  }
};

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Basic) {
  RunTestsInJsModule("basic_test.js", "test.pdf");
  EXPECT_EQ(1, CountPDFProcesses());
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, BasicPlugin) {
  RunTestsInJsModule("basic_plugin_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Viewport) {
  RunTestsInJsModule("viewport_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Layout3) {
  RunTestsInJsModule("layout_test.js", "test-layout3.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Layout4) {
  RunTestsInJsModule("layout_test.js", "test-layout4.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Bookmark) {
  RunTestsInJsModule("bookmarks_test.js", "test-bookmarks-with-zoom.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Navigator) {
  RunTestsInJsModule("navigator_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ParamsParser) {
  RunTestsInJsModule("params_parser_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ZoomManager) {
  RunTestsInJsModule("zoom_manager_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, GestureDetector) {
  RunTestsInJsModule("gesture_detector_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, TouchHandling) {
  RunTestsInJsModule("touch_handling_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Elements) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("material_elements_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, DownloadControls) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("download_controls_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Title) {
  RunTestsInJsModule("title_test.js", "test-title.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, WhitespaceTitle) {
  RunTestsInJsModule("whitespace_title_test.js", "test-whitespace-title.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, PageChange) {
  RunTestsInJsModule("page_change_test.js", "test-bookmarks.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ScrollWithFormFieldFocusedTest) {
  RunTestsInJsModule("scroll_with_form_field_focused_test.js",
                     "test-bookmarks.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Metrics) {
  RunTestsInJsModule("metrics_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewerPasswordDialog) {
  RunTestsInJsModule("viewer_password_dialog_test.js", "encrypted.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ArrayBufferAllocator) {
  // Run several times to see if there are issues with unloading.
  RunTestsInJsModule("beep_test.js", "array_buffer.pdf");
  RunTestsInJsModule("beep_test.js", "array_buffer.pdf");
  RunTestsInJsModule("beep_test.js", "array_buffer.pdf");
}

// Test that if the plugin tries to load a URL that redirects then it will fail
// to load. This is to avoid the source origin of the document changing during
// the redirect, which can have security implications. https://crbug.com/653749.
//
// Note that this can happen only during partial loading, as the initial URL
// load is handled by MimeHandlerView, and the plugin only gets the response.
IN_PROC_BROWSER_TEST_P(PDFExtensionTestWithPartialLoading,
                       PartialRedirectsFailInPlugin) {
  // Should match values used by `chrome_pdf::DocumentLoaderImpl`.
  constexpr size_t kDefaultRequestSize = 65536;
  constexpr size_t kChunkCloseDistance = 10;

  std::string pdf_contents;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    ASSERT_TRUE(
        base::ReadFileToString(test_data_dir.AppendASCII("pdf").AppendASCII(
                                   "test-ranges-linearized.pdf"),
                               &pdf_contents));
  }
  ASSERT_GT(pdf_contents.size(),
            kDefaultRequestSize * (kChunkCloseDistance + 2));

  // Use an additional test server, to allow customizing the response handling.
  net::test_server::EmbeddedTestServer test_server;

  constexpr char kActualPdf[] = "/pdf/test-ranges-linearized.pdf";
  constexpr char kSimulatedPdf[] = "/simulated/test-ranges-linearized.pdf";
  net::test_server::ControllableHttpResponse initial_response(&test_server,
                                                              kSimulatedPdf);
  net::test_server::ControllableHttpResponse followup_response(&test_server,
                                                               kSimulatedPdf);
  auto handle = test_server.StartAndReturnHandle();

  WebContents* contents = GetActiveWebContents();
  content::TestNavigationObserver navigation_observer(contents);
  contents->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(
          test_server.GetURL(kSimulatedPdf)));

  {
    SCOPED_TRACE("Waiting for initial request");
    initial_response.WaitForRequest();
  }
  initial_response.Send("HTTP/1.1 200 OK\r\n");
  initial_response.Send("Accept-Ranges: bytes\r\n");
  initial_response.Send(
      base::StringPrintf("Content-Length: %zu\r\n", pdf_contents.size()));
  initial_response.Send("Content-Type: application/pdf\r\n");
  initial_response.Send("\r\n");
  initial_response.Send(pdf_contents.substr(0, kDefaultRequestSize));

  navigation_observer.Wait();
  ASSERT_TRUE(navigation_observer.last_navigation_succeeded());

  {
    SCOPED_TRACE("Waiting for follow-up request");
    followup_response.WaitForRequest();
  }
  followup_response.Send("HTTP/1.1 301 Moved Permanently\r\n");
  followup_response.Send(base::StringPrintf(
      "Location: %s\r\n",
      embedded_test_server()->GetURL(kActualPdf).spec().c_str()));
  followup_response.Send("\r\n");
  followup_response.Done();

  // TODO(crbug.com/1228987): Load success or failure is non-deterministic
  // currently, due to races between viewport messages and loading. For this
  // test, we only care that loading terminated, not about success or failure.
  ignore_result(pdf_extension_test_util::EnsurePDFHasLoaded(contents));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewerToolbar) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_toolbar_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewerPdfSidenav) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_pdf_sidenav_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewerThumbnailBar) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_thumbnail_bar_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewerThumbnail) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_thumbnail_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Fullscreen) {
  // Use a PDF document with multiple pages, to exercise navigating between
  // pages.
  RunTestsInJsModule("fullscreen_test.js", "test-bookmarks.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewerPropertiesDialog) {
  // The properties dialog formats some values based on locale.
  base::test::ScopedRestoreICUDefaultLocale scoped_locale{"en_US"};
  // This will apply to the new processes spawned within RunTestsInJsModule(),
  // thus consistently running the test in a well known time zone.
  content::ScopedTimeZone scoped_time_zone{"America/Los_Angeles"};
  RunTestsInJsModule("viewer_properties_dialog_test.js", "document_info.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, PostMessageProxy) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("post_message_proxy_test.js", "test.pdf");
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Printing) {
  RunTestsInJsModule("printing_icon_test.js", "test.pdf");
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_INK)
// TODO(https://crbug.com/920684): Test times out under sanitizers.
#if defined(MEMORY_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(ADDRESS_SANITIZER) || defined(_DEBUG)
#define MAYBE_AnnotationsFeatureEnabled DISABLED_AnnotationsFeatureEnabled
#else
#define MAYBE_AnnotationsFeatureEnabled AnnotationsFeatureEnabled
#endif
IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, MAYBE_AnnotationsFeatureEnabled) {
  RunTestsInJsModule("annotations_feature_enabled_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, AnnotationsToolbar) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("annotations_toolbar_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewerToolbarDropdown) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_toolbar_dropdown_test.js", "test.pdf");
}
#endif  // BUILDFLAG(ENABLE_INK)

class PDFExtensionContentSettingJSTest : public PDFExtensionJSTest {
 protected:
  // When blocking JavaScript, block the exact query from pdf/main.js while
  // still allowing enough JavaScript to run in the extension for the test
  // harness to complete its work.
  void SetPdfJavaScript(bool enabled) {
    auto* map =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());
    map->SetContentSettingCustomScope(
        ContentSettingsPattern::Wildcard(),
        ContentSettingsPattern::FromString(
            "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai"),
        ContentSettingsType::JAVASCRIPT,
        enabled ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
  }
};

IN_PROC_BROWSER_TEST_P(PDFExtensionContentSettingJSTest, Beep) {
  RunTestsInJsModule("beep_test.js", "test-beep.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionContentSettingJSTest, NoBeep) {
  SetPdfJavaScript(/*enabled=*/false);
  RunTestsInJsModule("nobeep_test.js", "test-beep.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionContentSettingJSTest, BeepThenNoBeep) {
  RunTestsInJsModule("beep_test.js", "test-beep.pdf");
  SetPdfJavaScript(/*enabled=*/false);
  RunTestsInJsModuleNewTab("nobeep_test.js", "test-beep.pdf");

  // Make sure there are two PDFs in the same process.
  const int tab_count = browser()->tab_strip_model()->count();
  EXPECT_EQ(2, tab_count);
  EXPECT_EQ(1, CountPDFProcesses());
}

IN_PROC_BROWSER_TEST_P(PDFExtensionContentSettingJSTest, NoBeepThenBeep) {
  SetPdfJavaScript(/*enabled=*/false);
  RunTestsInJsModule("nobeep_test.js", "test-beep.pdf");
  SetPdfJavaScript(/*enabled=*/true);
  RunTestsInJsModuleNewTab("beep_test.js", "test-beep.pdf");

  // Make sure there are two PDFs in the same process.
  const int tab_count = browser()->tab_strip_model()->count();
  EXPECT_EQ(2, tab_count);
  EXPECT_EQ(1, CountPDFProcesses());
}

IN_PROC_BROWSER_TEST_P(PDFExtensionContentSettingJSTest, BeepCsp) {
  // The script-source * directive in the mock headers file should
  // allow the JavaScript to execute the beep().
  RunTestsInJsModule("beep_test.js", "test-beep-csp.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionContentSettingJSTest, DISABLED_NoBeepCsp) {
  // The script-source none directive in the mock headers file should
  // prevent the JavaScript from executing the beep().
  // TODO(https://crbug.com/1032511) functionality not implemented.
  RunTestsInJsModule("nobeep_test.js", "test-nobeep-csp.pdf");
}

class PDFExtensionWebUICodeCacheJSTest : public PDFExtensionJSTest {
 protected:
  std::vector<base::Feature> GetEnabledFeatures() const override {
    auto enabled = PDFExtensionJSTest::GetEnabledFeatures();
    enabled.push_back(features::kWebUICodeCache);
    return enabled;
  }
};

// Regression test for https://crbug.com/1239148.
IN_PROC_BROWSER_TEST_P(PDFExtensionWebUICodeCacheJSTest, Basic) {
  RunTestsInJsModule("basic_test.js", "test.pdf");
}

// Service worker tests are regression tests for
// https://crbug.com/916514.
class PDFExtensionServiceWorkerJSTest : public PDFExtensionJSTest {
 public:
  ~PDFExtensionServiceWorkerJSTest() override = default;

 protected:
  // Installs the specified service worker and tests navigating to a PDF in its
  // scope.
  void RunServiceWorkerTest(const std::string& worker_path) {
    // Install the service worker.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(
                       "/service_worker/create_service_worker.html")));
    EXPECT_EQ("DONE", EvalJs(GetActiveWebContents(),
                             "register('" + worker_path + "', '/pdf');"));

    // Navigate to a PDF in the service worker's scope. It should load.
    RunTestsInJsModule("basic_test.js", "test.pdf");
    EXPECT_EQ(1, CountPDFProcesses());
  }
};

// Test navigating to a PDF in the scope of a service worker with no fetch event
// handler.
IN_PROC_BROWSER_TEST_P(PDFExtensionServiceWorkerJSTest, NoFetchHandler) {
  RunServiceWorkerTest("empty.js");
}

// Test navigating to a PDF when a service worker intercepts the request and
// then falls back to network by not calling FetchEvent.respondWith().
IN_PROC_BROWSER_TEST_P(PDFExtensionServiceWorkerJSTest, NetworkFallback) {
  RunServiceWorkerTest("network_fallback_worker.js");
}

// Test navigating to a PDF when a service worker intercepts the request and
// provides a response.
IN_PROC_BROWSER_TEST_P(PDFExtensionServiceWorkerJSTest, Interception) {
  RunServiceWorkerTest("respond_with_fetch_worker.js");
}

// Ensure that the internal PDF plugin application/x-google-chrome-pdf won't be
// loaded if it's not loaded in the chrome extension page.
IN_PROC_BROWSER_TEST_P(PDFExtensionTest, EnsureInternalPluginDisabled) {
  std::string url = embedded_test_server()->GetURL("/pdf/test.pdf").spec();
  std::string data_url =
      "data:text/html,"
      "<html><body>"
      "<embed type=\"application/x-google-chrome-pdf\" src=\"" +
      url +
      "\">"
      "</body></html>";
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(data_url)));
  WebContents* web_contents = GetActiveWebContents();
  bool plugin_loaded = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "var plugin_loaded = "
      "    document.getElementsByTagName('embed')[0].postMessage !== undefined;"
      "window.domAutomationController.send(plugin_loaded);",
      &plugin_loaded));
  ASSERT_FALSE(plugin_loaded);
}

// Ensure cross-origin replies won't work for getSelectedText.
IN_PROC_BROWSER_TEST_P(PDFExtensionTest, EnsureCrossOriginRepliesBlocked) {
  std::string url = embedded_test_server()->GetURL("/pdf/test.pdf").spec();
  std::string data_url =
      "data:text/html,"
      "<html><body>"
      "<embed type=\"application/pdf\" src=\"" +
      url +
      "\">"
      "</body></html>";
  TestGetSelectedTextReply(GURL(data_url), false);
}

// Ensure same-origin replies do work for getSelectedText.
IN_PROC_BROWSER_TEST_P(PDFExtensionTest, EnsureSameOriginRepliesAllowed) {
  TestGetSelectedTextReply(embedded_test_server()->GetURL("/pdf/test.pdf"),
                           true);
}

// TODO(crbug.com/1004425): Should be allowed?
IN_PROC_BROWSER_TEST_P(PDFExtensionTest, EnsureOpaqueOriginRepliesBlocked) {
  TestGetSelectedTextReply(
      embedded_test_server()->GetURL("/pdf/data_url_rectangles.html"), false);
}

// Ensure that the PDF component extension cannot be loaded directly.
IN_PROC_BROWSER_TEST_P(PDFExtensionTest, BlockDirectAccess) {
  WebContents* web_contents = GetActiveWebContents();

  content::WebContentsConsoleObserver console_observer(web_contents);
  console_observer.SetPattern(
      "*Streams are only available from a mime handler view guest.*");
  GURL forbidden_url(
      "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html?"
      "https://example.com/notrequested.pdf");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), forbidden_url));

  console_observer.Wait();

  EXPECT_EQ(0, CountPDFProcesses());
}

// This test ensures that PDF can be loaded from local file
IN_PROC_BROWSER_TEST_P(PDFExtensionTest, EnsurePDFFromLocalFileLoads) {
  GURL test_pdf_url;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    test_data_dir = test_data_dir.Append(FILE_PATH_LITERAL("pdf"));
    base::FilePath test_data_file = test_data_dir.AppendASCII("test.pdf");
    ASSERT_TRUE(PathExists(test_data_file));
    test_pdf_url = GURL("file://" + test_data_file.MaybeAsASCII());
  }
  WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);

  EXPECT_EQ(1, CountPDFProcesses());
}

// Tests that PDF with no filename extension can be loaded from local file.
IN_PROC_BROWSER_TEST_P(PDFExtensionTest, ExtensionlessPDFLocalFileLoads) {
  GURL test_pdf_url;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    test_data_dir = test_data_dir.AppendASCII("pdf");
    base::FilePath test_data_file = test_data_dir.AppendASCII("imgpdf");
    ASSERT_TRUE(PathExists(test_data_file));
    test_pdf_url = GURL("file://" + test_data_file.MaybeAsASCII());
  }
  WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);

  EXPECT_EQ(1, CountPDFProcesses());
}

// This test ensures that link permissions are enforced properly in PDFs.
IN_PROC_BROWSER_TEST_P(PDFExtensionTest, LinkPermissions) {
  WebContents* guest_contents =
      LoadPdfGetGuestContents(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest_contents);

  // chrome://favicon links should be allowed for PDFs, while chrome://settings
  // links should not.
  GURL valid_link_url(std::string(chrome::kChromeUIFaviconURL) +
                      "https://www.google.ca/");
  GURL invalid_link_url(chrome::kChromeUISettingsURL);

  GURL unfiltered_valid_link_url(valid_link_url);
  content::RenderProcessHost* rph =
      guest_contents->GetMainFrame()->GetProcess();
  rph->FilterURL(true, &valid_link_url);
  rph->FilterURL(true, &invalid_link_url);

  // Invalid link URLs should be changed to "about:blank#blocked" when filtered.
  EXPECT_EQ(unfiltered_valid_link_url, valid_link_url);
  EXPECT_EQ(GURL(content::kBlockedURL), invalid_link_url);
}

// This test ensures that titles are set properly for PDFs without /Title.
IN_PROC_BROWSER_TEST_P(PDFExtensionTest, TabTitleWithNoTitle) {
  WebContents* guest_contents =
      LoadPdfGetGuestContents(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest_contents);
  EXPECT_EQ(u"test.pdf", guest_contents->GetTitle());
  EXPECT_EQ(u"test.pdf", GetActiveWebContents()->GetTitle());
}

// This test ensures that titles are set properly for PDFs with /Title.
IN_PROC_BROWSER_TEST_P(PDFExtensionTest, TabTitleWithTitle) {
  WebContents* guest_contents = LoadPdfGetGuestContents(
      embedded_test_server()->GetURL("/pdf/test-title.pdf"));
  ASSERT_TRUE(guest_contents);
  EXPECT_EQ(u"PDF title test", guest_contents->GetTitle());
  EXPECT_EQ(u"PDF title test", GetActiveWebContents()->GetTitle());
}

// This test ensures that titles are set properly for embedded PDFs with /Title.
IN_PROC_BROWSER_TEST_P(PDFExtensionTest, TabTitleWithEmbeddedPdf) {
  std::string url =
      embedded_test_server()->GetURL("/pdf/test-title.pdf").spec();
  std::string data_url =
      "data:text/html,"
      "<html><head><title>TabTitleWithEmbeddedPdf</title></head><body>"
      "<embed type=\"application/pdf\" src=\"" +
      url +
      "\"></body></html>";
  ASSERT_TRUE(LoadPdf(GURL(data_url)));
  EXPECT_EQ(u"TabTitleWithEmbeddedPdf", GetActiveWebContents()->GetTitle());
}

// Flaky, http://crbug.com/767427
#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
#define MAYBE_PdfZoomWithoutBubble DISABLED_PdfZoomWithoutBubble
#else
#define MAYBE_PdfZoomWithoutBubble PdfZoomWithoutBubble
#endif
IN_PROC_BROWSER_TEST_P(PDFExtensionTest, MAYBE_PdfZoomWithoutBubble) {
  WebContents* guest_contents =
      LoadPdfGetGuestContents(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest_contents);
  WebContents* web_contents = GetActiveWebContents();

  // Here we look at the presets to find the next zoom level above 0. Ideally
  // we should look at the zoom levels from the PDF viewer javascript, but we
  // assume they'll always match the browser presets, which are easier to
  // access. In the script below, we zoom to 100% (0), then wait for this to be
  // picked up by the browser zoom, then zoom to the next zoom level. This
  // ensures the test passes regardless of the initial default zoom level.
  std::vector<double> preset_zoom_levels = zoom::PageZoom::PresetZoomLevels(0);
  auto it = std::find(preset_zoom_levels.begin(), preset_zoom_levels.end(), 0);
  ASSERT_NE(it, preset_zoom_levels.end());
  it++;
  ASSERT_NE(it, preset_zoom_levels.end());
  double new_zoom_level = *it;

  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents);
  // We expect a ZoomChangedEvent with can_show_bubble == false if the PDF
  // extension behaviour is properly picked up. The test times out otherwise.
  zoom::ZoomChangedWatcher watcher(
      zoom_controller, zoom::ZoomController::ZoomChangedEventData(
                           web_contents, 0, new_zoom_level,
                           zoom::ZoomController::ZOOM_MODE_MANUAL, false));

  // Zoom PDF via script.
#if defined(TOOLKIT_VIEWS) && !defined(OS_MAC)
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());
#endif
  ASSERT_TRUE(content::ExecuteScript(guest_contents,
                                     "while (viewer.viewport.getZoom() < 1) {"
                                     "  viewer.viewport.zoomIn();"
                                     "}"
                                     "setTimeout(() => {"
                                     "  viewer.viewport.zoomIn();"
                                     "}, 1);"));

  watcher.Wait();
#if defined(TOOLKIT_VIEWS) && !defined(OS_MAC)
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());
#endif
}

class PDFExtensionKeyEventTest : public PDFExtensionTest {
 protected:
  class ScrollEventWaiter {
   public:
    explicit ScrollEventWaiter(WebContents* guest_contents)
        : message_queue_(guest_contents) {
      content::ExecuteScriptAsync(
          guest_contents,
          R"(viewer.shadowRoot.querySelector('#scroller').onscroll = () => {
            window.domAutomationController.send('dispatchedScrollEvent');
          })");
    }

    void Reset() { message_queue_.ClearQueue(); }

    void Wait() {
      std::string message;
      ASSERT_TRUE(message_queue_.WaitForMessage(&message));
      EXPECT_EQ("\"dispatchedScrollEvent\"", message);
    }

   private:
    content::DOMMessageQueue message_queue_;
  };

  // Scroll increment in CSS pixels. Should match `SCROLL_INCREMENT` in
  // //chrome/browser/resources/pdf/viewport.js.
  static constexpr int kScrollIncrement = 40;

  static int GetViewportHeight(WebContents* guest_contents) {
    int viewport_height = 0;
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        guest_contents,
        "window.domAutomationController.send(viewer.viewport.size.height);",
        &viewport_height));
    return viewport_height;
  }

  static int GetViewportScrollPositionX(WebContents* guest_contents) {
    int position_x = 0;
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        guest_contents,
        "window.domAutomationController.send(viewer.viewport.position.x);",
        &position_x));
    return position_x;
  }

  static int GetViewportScrollPositionY(WebContents* guest_contents) {
    int position_y = 0;
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        guest_contents,
        "window.domAutomationController.send(viewer.viewport.position.y);",
        &position_y));
    return position_y;
  }
};

// static
constexpr int PDFExtensionKeyEventTest::kScrollIncrement;

// crbug.com/1281749
#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
#define MAYBE_ScrollWithSpace DISABLED_ScrollWithSpace
#else
#define MAYBE_ScrollWithSpace ScrollWithSpace
#endif
IN_PROC_BROWSER_TEST_P(PDFExtensionKeyEventTest, MAYBE_ScrollWithSpace) {
  WebContents* guest_contents = LoadPdfGetGuestContents(
      embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf"));
  SetInputFocusOnPlugin(guest_contents);
  ASSERT_EQ(0, GetViewportScrollPositionY(guest_contents));

  // Get the viewport height, since the scroll distance is based on it.
  const int viewport_height = GetViewportHeight(guest_contents);
  ASSERT_GT(viewport_height, 0);

  // Press Space to scroll down.
  ScrollEventWaiter scroll_waiter(guest_contents);
  content::SimulateKeyPress(guest_contents, ui::DomKey::FromCharacter(' '),
                            ui::DomCode::SPACE, ui::VKEY_SPACE,
                            /*control=*/false, /*shift=*/false, /*alt=*/false,
                            /*command=*/false);
  ASSERT_NO_FATAL_FAILURE(scroll_waiter.Wait());
  EXPECT_EQ(viewport_height, GetViewportScrollPositionY(guest_contents));

  // Press Space to scroll down again.
  scroll_waiter.Reset();
  content::SimulateKeyPress(guest_contents, ui::DomKey::FromCharacter(' '),
                            ui::DomCode::SPACE, ui::VKEY_SPACE,
                            /*control=*/false, /*shift=*/false, /*alt=*/false,
                            /*command=*/false);
  ASSERT_NO_FATAL_FAILURE(scroll_waiter.Wait());
  EXPECT_EQ(viewport_height * 2, GetViewportScrollPositionY(guest_contents));

  // Press Shift+Space to scroll up.
  scroll_waiter.Reset();
  content::SimulateKeyPress(guest_contents, ui::DomKey::FromCharacter(' '),
                            ui::DomCode::SPACE, ui::VKEY_SPACE,
                            /*control=*/false, /*shift=*/true, /*alt=*/false,
                            /*command=*/false);
  ASSERT_NO_FATAL_FAILURE(scroll_waiter.Wait());
  EXPECT_EQ(viewport_height, GetViewportScrollPositionY(guest_contents));
}

// crbug.com/1281749
#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
#define MAYBE_ScrollWithPageDownUp DISABLED_ScrollWithPageDownUp
#else
#define MAYBE_ScrollWithPageDownUp ScrollWithPageDownUp
#endif
IN_PROC_BROWSER_TEST_P(PDFExtensionKeyEventTest, MAYBE_ScrollWithPageDownUp) {
  WebContents* guest_contents = LoadPdfGetGuestContents(
      embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf"));
  SetInputFocusOnPlugin(guest_contents);
  ASSERT_EQ(0, GetViewportScrollPositionY(guest_contents));

  // Get the viewport height, since the scroll distance is based on it.
  const int viewport_height = GetViewportHeight(guest_contents);
  ASSERT_GT(viewport_height, 0);

  // Press PageDown to scroll down.
  ScrollEventWaiter scroll_waiter(guest_contents);
  content::SimulateKeyPressWithoutChar(guest_contents, ui::DomKey::PAGE_DOWN,
                                       ui::DomCode::PAGE_DOWN, ui::VKEY_NEXT,
                                       /*control=*/false, /*shift=*/false,
                                       /*alt=*/false,
                                       /*command=*/false);
  ASSERT_NO_FATAL_FAILURE(scroll_waiter.Wait());
  EXPECT_EQ(viewport_height, GetViewportScrollPositionY(guest_contents));

  // Press PageDown to scroll down again.
  scroll_waiter.Reset();
  content::SimulateKeyPressWithoutChar(guest_contents, ui::DomKey::PAGE_DOWN,
                                       ui::DomCode::PAGE_DOWN, ui::VKEY_NEXT,
                                       /*control=*/false, /*shift=*/false,
                                       /*alt=*/false,
                                       /*command=*/false);
  ASSERT_NO_FATAL_FAILURE(scroll_waiter.Wait());
  EXPECT_EQ(viewport_height * 2, GetViewportScrollPositionY(guest_contents));

  // Press PageUp to scroll up.
  scroll_waiter.Reset();
  content::SimulateKeyPressWithoutChar(
      guest_contents, ui::DomKey::PAGE_UP, ui::DomCode::PAGE_UP, ui::VKEY_PRIOR,
      /*control=*/false, /*shift=*/false, /*alt=*/false,
      /*command=*/false);
  ASSERT_NO_FATAL_FAILURE(scroll_waiter.Wait());
  EXPECT_EQ(viewport_height, GetViewportScrollPositionY(guest_contents));
}

// crbug.com/1281749
#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
#define MAYBE_ScrollWithArrowLeftRight DISABLED_ScrollWithArrowLeftRight
#else
#define MAYBE_ScrollWithArrowLeftRight ScrollWithArrowLeftRight
#endif
IN_PROC_BROWSER_TEST_P(PDFExtensionKeyEventTest,
                       MAYBE_ScrollWithArrowLeftRight) {
  WebContents* guest_contents = LoadPdfGetGuestContents(
      embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf#zoom=200"));
  SetInputFocusOnPlugin(guest_contents);
  ASSERT_EQ(0, GetViewportScrollPositionY(guest_contents));

  // Press ArrowRight to scroll right.
  ScrollEventWaiter scroll_waiter(guest_contents);
  content::SimulateKeyPressWithoutChar(guest_contents, ui::DomKey::ARROW_RIGHT,
                                       ui::DomCode::ARROW_RIGHT, ui::VKEY_RIGHT,
                                       /*control=*/false, /*shift=*/false,
                                       /*alt=*/false,
                                       /*command=*/false);
  ASSERT_NO_FATAL_FAILURE(scroll_waiter.Wait());
  EXPECT_EQ(kScrollIncrement, GetViewportScrollPositionX(guest_contents));

  // Press ArrowRight to scroll right again.
  scroll_waiter.Reset();
  content::SimulateKeyPressWithoutChar(guest_contents, ui::DomKey::ARROW_RIGHT,
                                       ui::DomCode::ARROW_RIGHT, ui::VKEY_RIGHT,
                                       /*control=*/false, /*shift=*/false,
                                       /*alt=*/false,
                                       /*command=*/false);
  ASSERT_NO_FATAL_FAILURE(scroll_waiter.Wait());
  EXPECT_EQ(kScrollIncrement * 2, GetViewportScrollPositionX(guest_contents));

  // Press ArrowLeft to scroll left.
  scroll_waiter.Reset();
  content::SimulateKeyPressWithoutChar(guest_contents, ui::DomKey::ARROW_LEFT,
                                       ui::DomCode::ARROW_LEFT, ui::VKEY_LEFT,
                                       /*control=*/false, /*shift=*/false,
                                       /*alt=*/false,
                                       /*command=*/false);
  ASSERT_NO_FATAL_FAILURE(scroll_waiter.Wait());
  EXPECT_EQ(kScrollIncrement, GetViewportScrollPositionX(guest_contents));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionKeyEventTest, ScrollWithArrowDownUp) {
  WebContents* guest_contents = LoadPdfGetGuestContents(
      embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf"));
  SetInputFocusOnPlugin(guest_contents);
  ASSERT_EQ(0, GetViewportScrollPositionY(guest_contents));

  // Press ArrowDown to scroll down.
  ScrollEventWaiter scroll_waiter(guest_contents);
  content::SimulateKeyPressWithoutChar(guest_contents, ui::DomKey::ARROW_DOWN,
                                       ui::DomCode::ARROW_DOWN, ui::VKEY_DOWN,
                                       /*control=*/false, /*shift=*/false,
                                       /*alt=*/false,
                                       /*command=*/false);
  ASSERT_NO_FATAL_FAILURE(scroll_waiter.Wait());
  EXPECT_EQ(kScrollIncrement, GetViewportScrollPositionY(guest_contents));

  // Press ArrowDown to scroll down again.
  scroll_waiter.Reset();
  content::SimulateKeyPressWithoutChar(guest_contents, ui::DomKey::ARROW_DOWN,
                                       ui::DomCode::ARROW_DOWN, ui::VKEY_DOWN,
                                       /*control=*/false, /*shift=*/false,
                                       /*alt=*/false,
                                       /*command=*/false);
  ASSERT_NO_FATAL_FAILURE(scroll_waiter.Wait());
  EXPECT_EQ(kScrollIncrement * 2, GetViewportScrollPositionY(guest_contents));

  // Press ArrowUp to scroll up.
  scroll_waiter.Reset();
  content::SimulateKeyPressWithoutChar(guest_contents, ui::DomKey::ARROW_UP,
                                       ui::DomCode::ARROW_UP, ui::VKEY_UP,
                                       /*control=*/false, /*shift=*/false,
                                       /*alt=*/false,
                                       /*command=*/false);
  ASSERT_NO_FATAL_FAILURE(scroll_waiter.Wait());
  EXPECT_EQ(kScrollIncrement, GetViewportScrollPositionY(guest_contents));
}

INSTANTIATE_TEST_SUITE_P(All, PDFExtensionKeyEventTest, testing::Values(true));

IN_PROC_BROWSER_TEST_P(PDFExtensionTest, SelectAllShortcut) {
  content::WebContents* guest_contents =
      LoadPdfGetGuestContents(embedded_test_server()->GetURL("/pdf/test.pdf"));
  SetInputFocusOnPlugin(guest_contents);

  content::RenderFrameHost* frame = GetPluginFrame(guest_contents);
  ASSERT_TRUE(frame);
  content::RenderWidgetHostView* view = frame->GetView();
  EXPECT_THAT(view->GetSelectedText(), IsEmpty());

  base::RunLoop run_loop;
  content::TextInputManagerTester input_tester(guest_contents);
  input_tester.SetOnTextSelectionChangedCallback(run_loop.QuitClosure());

  bool control = false;
  bool command = false;
#if defined(OS_MAC)
  command = true;
#else
  control = true;
#endif
  content::SimulateKeyPress(guest_contents, ui::DomKey::FromCharacter('a'),
                            ui::DomCode::US_A, ui::VKEY_A, control,
                            /*shift=*/false,
                            /*alt=*/false, command);
  run_loop.Run();

#if defined(OS_WIN)
  constexpr char kExpectedText[] = "this is some text\r\nsome more text";
#else
  constexpr char kExpectedText[] = "this is some text\nsome more text";
#endif
  EXPECT_EQ(base::UTF16ToUTF8(view->GetSelectedText()), kExpectedText);
}

// TODO(crbug.com/1253714): Add tests for using space and shift+space shortcuts
// for scrolling PDFs.

#if BUILDFLAG(ENABLE_PRINTING)
namespace {

class PrintObserver : public printing::PrintViewManagerBase::Observer {
 public:
  PrintObserver(content::WebContents* contents,
                const content::RenderFrameHost* rfh)
      : print_view_manager_(PrintViewManagerImpl::FromWebContents(contents)),
        rfh_(rfh) {
    print_view_manager_->AddObserver(*this);
  }

  ~PrintObserver() override { print_view_manager_->RemoveObserver(*this); }

  // printing::PrintViewManagerBase::Observer:
  void OnPrintNow(const content::RenderFrameHost* rfh) override {
    EXPECT_FALSE(print_now_called_);
    EXPECT_FALSE(print_preview_called_);
    EXPECT_EQ(rfh, rfh_);
    run_loop_.Quit();
    print_now_called_ = true;
  }
  void OnPrintPreview(const content::RenderFrameHost* rfh) override {
    EXPECT_FALSE(print_preview_called_);
    EXPECT_FALSE(print_now_called_);
    EXPECT_EQ(rfh, rfh_);
    run_loop_.Quit();
    print_preview_called_ = true;
  }

  void WaitForPrintNow() {
    WaitIfNotAlreadyPrinted();
    EXPECT_TRUE(print_now_called_);
    EXPECT_FALSE(print_preview_called_);
  }

  void WaitForPrintPreview() {
    WaitIfNotAlreadyPrinted();
    EXPECT_TRUE(print_preview_called_);
    EXPECT_FALSE(print_now_called_);
  }

 private:
  void WaitIfNotAlreadyPrinted() {
    if (!print_now_called_ && !print_preview_called_)
      run_loop_.Run();
  }

  bool print_now_called_ = false;
  bool print_preview_called_ = false;

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  using PrintViewManagerImpl = printing::PrintViewManager;
#else
  using PrintViewManagerImpl = printing::PrintViewManagerBasic;
#endif
  const raw_ptr<PrintViewManagerImpl> print_view_manager_;
  const raw_ptr<const content::RenderFrameHost> rfh_;
  base::RunLoop run_loop_;
};

}  // namespace

// TODO(crbug.com/1258561): Fix flakes.
IN_PROC_BROWSER_TEST_P(PDFExtensionTest, DISABLED_BasicPrintCommand) {
  content::WebContents* guest_contents =
      LoadPdfGetGuestContents(embedded_test_server()->GetURL("/pdf/test.pdf"));
  content::RenderFrameHost* frame = GetPluginFrame(guest_contents);
  ASSERT_TRUE(frame);

  PrintObserver print_observer(guest_contents, frame);
  chrome::BasicPrint(browser());
  print_observer.WaitForPrintNow();
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
IN_PROC_BROWSER_TEST_P(PDFExtensionTest, PrintCommand) {
  content::WebContents* guest_contents =
      LoadPdfGetGuestContents(embedded_test_server()->GetURL("/pdf/test.pdf"));
  content::RenderFrameHost* frame = GetPluginFrame(guest_contents);
  ASSERT_TRUE(frame);

  PrintObserver print_observer(guest_contents, frame);
  chrome::Print(browser());
  print_observer.WaitForPrintPreview();
}

IN_PROC_BROWSER_TEST_P(PDFExtensionTest,
                       ContextMenuPrintCommandExtensionMainFrame) {
  content::WebContents* guest_contents =
      LoadPdfGetGuestContents(embedded_test_server()->GetURL("/pdf/test.pdf"));
  content::RenderFrameHost* plugin_frame = GetPluginFrame(guest_contents);
  ASSERT_TRUE(plugin_frame);

  // Makes sure that the correct frame invoked the context menu.
  content::ContextMenuInterceptor menu_interceptor(
      guest_contents->GetMainFrame());

  // Executes the print command as soon as the context menu is shown.
  ContextMenuNotificationObserver context_menu_observer(IDC_PRINT);

  PrintObserver print_observer(guest_contents, plugin_frame);
  guest_contents->GetMainFrame()->GetRenderWidgetHost()->ShowContextMenuAtPoint(
      {1, 1}, ui::MENU_SOURCE_MOUSE);
  print_observer.WaitForPrintPreview();
  menu_interceptor.Wait();
}

IN_PROC_BROWSER_TEST_P(PDFExtensionTest, ContextMenuPrintCommandPluginFrame) {
  content::WebContents* guest_contents =
      LoadPdfGetGuestContents(embedded_test_server()->GetURL("/pdf/test.pdf"));
  content::RenderFrameHost* plugin_frame = GetPluginFrame(guest_contents);
  ASSERT_TRUE(plugin_frame);

  // Makes sure that the correct frame invoked the context menu.
  content::ContextMenuInterceptor menu_interceptor(plugin_frame);

  // Executes the print command as soon as the context menu is shown.
  ContextMenuNotificationObserver context_menu_observer(IDC_PRINT);

  PrintObserver print_observer(guest_contents, plugin_frame);
  SetInputFocusOnPlugin(guest_contents);
  plugin_frame->GetRenderWidgetHost()->ShowContextMenuAtPoint(
      {1, 1}, ui::MENU_SOURCE_MOUSE);
  print_observer.WaitForPrintPreview();
  menu_interceptor.Wait();
}

IN_PROC_BROWSER_TEST_P(PDFExtensionTest, PrintButton) {
  content::WebContents* guest_contents =
      LoadPdfGetGuestContents(embedded_test_server()->GetURL("/pdf/test.pdf"));
  content::RenderFrameHost* frame = GetPluginFrame(guest_contents);
  ASSERT_TRUE(frame);

  PrintObserver print_observer(guest_contents, frame);
  constexpr char kClickPrintButtonScript[] = R"(
    viewer.shadowRoot.querySelector('#toolbar')
        .shadowRoot.querySelector('#print')
        .click();
  )";
  EXPECT_TRUE(ExecuteScript(guest_contents, kClickPrintButtonScript));
  print_observer.WaitForPrintPreview();
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
#endif  // BUILDFLAG(ENABLE_PRINTING)

class PDFExtensionRegionSearchTest : public PDFExtensionTest {
 protected:
  std::vector<base::Feature> GetEnabledFeatures() const override {
    auto enabled = PDFExtensionTest::GetEnabledFeatures();
    enabled.push_back(lens::features::kLensRegionSearch);
    return enabled;
  }
};

IN_PROC_BROWSER_TEST_P(PDFExtensionRegionSearchTest,
                       NoContextMenuCommandExtensionMainFrame) {
  content::WebContents* guest_contents =
      LoadPdfGetGuestContents(embedded_test_server()->GetURL("/pdf/test.pdf"));

  // Makes sure that the correct frame invoked the context menu.
  content::ContextMenuInterceptor menu_interceptor(
      guest_contents->GetMainFrame());

  // Captures the command IDs of the context menu.
  ContextMenuWaiter menu_observer;

  guest_contents->GetMainFrame()->GetRenderWidgetHost()->ShowContextMenuAtPoint(
      {1, 1}, ui::MENU_SOURCE_MOUSE);

  menu_observer.WaitForMenuOpenAndClose();
  menu_interceptor.Wait();

  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              Not(Contains(IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH)));
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              Not(Contains(IDC_CONTENT_CONTEXT_WEB_REGION_SEARCH)));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionRegionSearchTest,
                       NoContextMenuCommandPluginFrame) {
  content::WebContents* guest_contents =
      LoadPdfGetGuestContents(embedded_test_server()->GetURL("/pdf/test.pdf"));

  // Makes sure that the correct frame invoked the context menu.
  content::ContextMenuInterceptor menu_interceptor(
      GetPluginFrame(guest_contents));

  // Captures the command IDs of the context menu.
  ContextMenuWaiter menu_observer;

  SetInputFocusOnPlugin(guest_contents);
  GetPluginFrame(guest_contents)
      ->GetRenderWidgetHost()
      ->ShowContextMenuAtPoint({1, 1}, ui::MENU_SOURCE_MOUSE);

  menu_observer.WaitForMenuOpenAndClose();
  menu_interceptor.Wait();

  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              Not(Contains(IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH)));
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              Not(Contains(IDC_CONTENT_CONTEXT_WEB_REGION_SEARCH)));
}

namespace {

std::string DumpPdfAccessibilityTree(const ui::AXTreeUpdate& ax_tree) {
  // Create a string representation of the tree starting with the kPdfRoot
  // object.
  std::string ax_tree_dump;
  std::map<int32_t, int> id_to_indentation;
  bool found_pdf_root = false;
  for (const ui::AXNodeData& node : ax_tree.nodes) {
    if (node.role == ax::mojom::Role::kPdfRoot)
      found_pdf_root = true;
    if (!found_pdf_root)
      continue;

    auto indent_found = id_to_indentation.find(node.id);
    int indent = 0;
    if (indent_found != id_to_indentation.end()) {
      indent = indent_found->second;
    } else if (node.role != ax::mojom::Role::kPdfRoot) {
      // If this node has no indent and isn't the kPdfRoot object, finish dump
      // as this indicates the end of the PDF.
      break;
    }

    ax_tree_dump += std::string(2 * indent, ' ');
    ax_tree_dump += ui::ToString(node.role);

    std::string name =
        node.GetStringAttribute(ax::mojom::StringAttribute::kName);
    base::ReplaceChars(name, "\r\n", "", &name);
    if (!name.empty())
      ax_tree_dump += " '" + name + "'";
    ax_tree_dump += "\n";
    for (size_t j = 0; j < node.child_ids.size(); ++j)
      id_to_indentation[node.child_ids[j]] = indent + 1;
  }

  EXPECT_TRUE(found_pdf_root);
  return ax_tree_dump;
}

constexpr char kExpectedPDFAXTree[] =
    "pdfRoot 'PDF document containing 3 pages'\n"
    "  region 'Page 1'\n"
    "    paragraph\n"
    "      staticText '1 First Section'\n"
    "        inlineTextBox '1 '\n"
    "        inlineTextBox 'First Section'\n"
    "    paragraph\n"
    "      staticText 'This is the first section.'\n"
    "        inlineTextBox 'This is the first section.'\n"
    "    paragraph\n"
    "      staticText '1'\n"
    "        inlineTextBox '1'\n"
    "  region 'Page 2'\n"
    "    paragraph\n"
    "      staticText '1.1 First Subsection'\n"
    "        inlineTextBox '1.1 '\n"
    "        inlineTextBox 'First Subsection'\n"
    "    paragraph\n"
    "      staticText 'This is the first subsection.'\n"
    "        inlineTextBox 'This is the first subsection.'\n"
    "    paragraph\n"
    "      staticText '2'\n"
    "        inlineTextBox '2'\n"
    "  region 'Page 3'\n"
    "    paragraph\n"
    "      staticText '2 Second Section'\n"
    "        inlineTextBox '2 '\n"
    "        inlineTextBox 'Second Section'\n"
    "    paragraph\n"
    "      staticText '3'\n"
    "        inlineTextBox '3'\n";

}  // namespace

IN_PROC_BROWSER_TEST_P(PDFExtensionTest, PdfAccessibility) {
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();

  WebContents* guest_contents = LoadPdfGetGuestContents(
      embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf"));
  ASSERT_TRUE(guest_contents);

  WaitForAccessibilityTreeToContainNodeWithName(guest_contents,
                                                "1 First Section\r\n");
  ui::AXTreeUpdate ax_tree = GetAccessibilityTreeSnapshotForPdf(guest_contents);
  std::string ax_tree_dump = DumpPdfAccessibilityTree(ax_tree);

  ASSERT_MULTILINE_STREQ(kExpectedPDFAXTree, ax_tree_dump);
}

IN_PROC_BROWSER_TEST_P(PDFExtensionTest, PdfAccessibilityEnableLater) {
  // In this test, load the PDF file first, with accessibility off.
  WebContents* guest_contents = LoadPdfGetGuestContents(
      embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf"));
  ASSERT_TRUE(guest_contents);

  // Now enable accessibility globally, and assert that the PDF accessibility
  // tree loads.
  EnableAccessibilityForWebContents(guest_contents);
  WaitForAccessibilityTreeToContainNodeWithName(guest_contents,
                                                "1 First Section\r\n");
  ui::AXTreeUpdate ax_tree = GetAccessibilityTreeSnapshotForPdf(guest_contents);
  std::string ax_tree_dump = DumpPdfAccessibilityTree(ax_tree);
  ASSERT_MULTILINE_STREQ(kExpectedPDFAXTree, ax_tree_dump);
}

// Flaky, see crbug.com/1228762
#if defined(OS_CHROMEOS)
#define MAYBE_PdfAccessibilityInIframe DISABLED_PdfAccessibilityInIframe
#else
#define MAYBE_PdfAccessibilityInIframe PdfAccessibilityInIframe
#endif

IN_PROC_BROWSER_TEST_P(PDFExtensionTest, MAYBE_PdfAccessibilityInIframe) {
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/pdf/test-iframe.html")));

  WebContents* contents = GetActiveWebContents();
  WaitForAccessibilityTreeToContainNodeWithName(contents,
                                                "1 First Section\r\n");

  WebContents* guest_contents = GetOnlyGuestContents(contents);
  ASSERT_TRUE(guest_contents);

  ui::AXTreeUpdate ax_tree = GetAccessibilityTreeSnapshotForPdf(guest_contents);
  std::string ax_tree_dump = DumpPdfAccessibilityTree(ax_tree);
  ASSERT_MULTILINE_STREQ(kExpectedPDFAXTree, ax_tree_dump);
}

IN_PROC_BROWSER_TEST_P(PDFExtensionTest, PdfAccessibilityInOOPIF) {
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/pdf/test-cross-site-iframe.html")));

  WebContents* contents = GetActiveWebContents();
  WaitForAccessibilityTreeToContainNodeWithName(contents,
                                                "1 First Section\r\n");

  WebContents* guest_contents = GetOnlyGuestContents(contents);
  ASSERT_TRUE(guest_contents);

  ui::AXTreeUpdate ax_tree = GetAccessibilityTreeSnapshotForPdf(guest_contents);
  std::string ax_tree_dump = DumpPdfAccessibilityTree(ax_tree);
  ASSERT_MULTILINE_STREQ(kExpectedPDFAXTree, ax_tree_dump);
}

IN_PROC_BROWSER_TEST_P(PDFExtensionTest, PdfAccessibilityWordBoundaries) {
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
  WebContents* guest_contents = LoadPdfGetGuestContents(
      embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf"));
  ASSERT_TRUE(guest_contents);

  WaitForAccessibilityTreeToContainNodeWithName(guest_contents,
                                                "1 First Section\r\n");
  ui::AXTreeUpdate ax_tree = GetAccessibilityTreeSnapshotForPdf(guest_contents);

  bool found = false;
  for (auto& node : ax_tree.nodes) {
    std::string name =
        node.GetStringAttribute(ax::mojom::StringAttribute::kName);
    if (node.role == ax::mojom::Role::kInlineTextBox &&
        name == "First Section\r\n") {
      found = true;
      std::vector<int32_t> word_starts =
          node.GetIntListAttribute(ax::mojom::IntListAttribute::kWordStarts);
      std::vector<int32_t> word_ends =
          node.GetIntListAttribute(ax::mojom::IntListAttribute::kWordEnds);
      ASSERT_EQ(2U, word_starts.size());
      ASSERT_EQ(2U, word_ends.size());
      EXPECT_EQ(0, word_starts[0]);
      EXPECT_EQ(5, word_ends[0]);
      EXPECT_EQ(6, word_starts[1]);
      EXPECT_EQ(13, word_ends[1]);
    }
  }
  ASSERT_TRUE(found);
}

IN_PROC_BROWSER_TEST_P(PDFExtensionTest, PdfAccessibilitySelection) {
  WebContents* guest_contents = LoadPdfGetGuestContents(
      embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf"));
  ASSERT_TRUE(guest_contents);

  ASSERT_TRUE(content::ExecuteScript(
      GetActiveWebContents(),
      "document.getElementsByTagName('embed')[0].postMessage("
      "{type: 'selectAll'});"));

  EnableAccessibilityForWebContents(guest_contents);
  WaitForAccessibilityTreeToContainNodeWithName(guest_contents,
                                                "1 First Section\r\n");
  ui::AXTreeUpdate ax_tree_update =
      GetAccessibilityTreeSnapshotForPdf(guest_contents);
  ui::AXTree ax_tree(ax_tree_update);

  // Ensure that the selection spans the beginning of the first text
  // node to the end of the last one.
  ui::AXNode* sel_start_node =
      ax_tree.GetFromId(ax_tree.data().sel_anchor_object_id);
  ASSERT_TRUE(sel_start_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, sel_start_node->GetRole());
  std::string start_node_name =
      sel_start_node->GetStringAttribute(ax::mojom::StringAttribute::kName);
  EXPECT_EQ("1 First Section\r\n", start_node_name);
  EXPECT_EQ(0, ax_tree.data().sel_anchor_offset);
  ui::AXNode* para = sel_start_node->parent();
  EXPECT_EQ(ax::mojom::Role::kParagraph, para->GetRole());
  ui::AXNode* region = para->parent();
  EXPECT_EQ(ax::mojom::Role::kRegion, region->GetRole());

  ui::AXNode* sel_end_node =
      ax_tree.GetFromId(ax_tree.data().sel_focus_object_id);
  ASSERT_TRUE(sel_end_node);
  std::string end_node_name =
      sel_end_node->GetStringAttribute(ax::mojom::StringAttribute::kName);
  EXPECT_EQ("3", end_node_name);
  EXPECT_EQ(static_cast<int>(end_node_name.size()),
            ax_tree.data().sel_focus_offset);
  para = sel_end_node->parent();
  EXPECT_EQ(ax::mojom::Role::kParagraph, para->GetRole());
  region = para->parent();
  EXPECT_EQ(ax::mojom::Role::kRegion, region->GetRole());
}

IN_PROC_BROWSER_TEST_P(PDFExtensionTest, PdfAccessibilityContextMenuAction) {
  // Validate the context menu arguments for PDF selection when context menu is
  // invoked via accessibility tree.
  const char kExepectedPDFSelection[] =
      "1 First Section\n"
      "This is the first section.\n"
      "1\n"
      "1.1 First Subsection\n"
      "This is the first subsection.\n"
      "2\n"
      "2 Second Section\n"
      "3";

  WebContents* guest_contents = LoadPdfGetGuestContents(
      embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf"));
  ASSERT_TRUE(guest_contents);

  ASSERT_TRUE(content::ExecuteScript(
      GetActiveWebContents(),
      "document.getElementsByTagName('embed')[0].postMessage("
      "{type: 'selectAll'});"));

  EnableAccessibilityForWebContents(guest_contents);
  WaitForAccessibilityTreeToContainNodeWithName(guest_contents,
                                                "1 First Section\r\n");

  // Find pdfRoot node in the accessibility tree.
  content::FindAccessibilityNodeCriteria find_criteria;
  find_criteria.role = ax::mojom::Role::kPdfRoot;
  ui::AXPlatformNodeDelegate* pdf_root =
      content::FindAccessibilityNode(guest_contents, find_criteria);
  ASSERT_TRUE(pdf_root);

  content::ContextMenuInterceptor context_menu_interceptor(
      GetPluginFrame(guest_contents));

  ContextMenuWaiter menu_waiter;
  // Invoke kShowContextMenu accessibility action on the node with the kPdfRoot
  // role.
  ui::AXActionData data;
  data.action = ax::mojom::Action::kShowContextMenu;
  pdf_root->AccessibilityPerformAction(data);
  menu_waiter.WaitForMenuOpenAndClose();

  context_menu_interceptor.Wait();
  blink::UntrustworthyContextMenuParams params =
      context_menu_interceptor.get_params();

  // Validate the context menu params for selection.
  EXPECT_EQ(blink::mojom::ContextMenuDataMediaType::kPlugin, params.media_type);
  std::string selected_text = base::UTF16ToUTF8(params.selection_text);
  base::ReplaceChars(selected_text, "\r", "", &selected_text);
  EXPECT_EQ(kExepectedPDFSelection, selected_text);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Test a particular PDF encountered in the wild that triggered a crash
// when accessibility is enabled.  (http://crbug.com/668724)
IN_PROC_BROWSER_TEST_P(PDFExtensionTest, PdfAccessibilityTextRunCrash) {
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
  WebContents* guest_contents = LoadPdfGetGuestContents(
      embedded_test_server()->GetURL("/pdf_private/accessibility_crash_2.pdf"));
  ASSERT_TRUE(guest_contents);

  WaitForAccessibilityTreeToContainNodeWithName(guest_contents, "Page 1");
}
#endif

// Test that even if a different tab is selected when a navigation occurs,
// the correct tab still gets navigated (see crbug.com/672563).
IN_PROC_BROWSER_TEST_P(PDFExtensionTest, NavigationOnCorrectTab) {
  WebContents* guest_contents =
      LoadPdfGetGuestContents(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest_contents);
  WebContents* web_contents = GetActiveWebContents();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  WebContents* active_web_contents = GetActiveWebContents();
  ASSERT_NE(web_contents, active_web_contents);

  content::TestNavigationObserver active_navigation_observer(
      active_web_contents);
  content::TestNavigationObserver navigation_observer(web_contents);
  ASSERT_TRUE(
      content::ExecuteScript(guest_contents,
                             "viewer.navigator_.navigate("
                             "    'www.example.com',"
                             "    WindowOpenDisposition.CURRENT_TAB);"));
  navigation_observer.Wait();

  EXPECT_FALSE(navigation_observer.last_navigation_url().is_empty());
  EXPECT_TRUE(active_navigation_observer.last_navigation_url().is_empty());
  EXPECT_FALSE(active_web_contents->GetController().GetPendingEntry());
}

IN_PROC_BROWSER_TEST_P(PDFExtensionTest, MultipleDomains) {
  ASSERT_TRUE(LoadPdfInNewTab(
      embedded_test_server()->GetURL("a.com", "/pdf/test.pdf")));
  ASSERT_TRUE(LoadPdfInNewTab(
      embedded_test_server()->GetURL("b.com", "/pdf/test.pdf")));
  EXPECT_EQ(2, CountPDFProcesses());
}

using PDFExtensionIsolatedContentTest = PDFExtensionTest;

IN_PROC_BROWSER_TEST_P(PDFExtensionIsolatedContentTest, PdfAndHtml) {
  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  // Load a page with an embedded PDF and an HTML iframe, both of the same
  // origin.
  WebContents* guest_contents = LoadPdfGetGuestContents(
      embedded_test_server()->GetURL("/pdf/embed_pdf_and_html.html"));
  ASSERT_TRUE(guest_contents);

  // The PDF and the iframe should not share renderer processes even though they
  // share origins.
  std::vector<content::RenderFrameHost*> pdf_frames =
      GetUnseasonedPdfFrames(guest_contents);
  ASSERT_EQ(pdf_frames.size(), 1u);

  content::RenderFrameHost* iframe = ChildFrameAt(GetActiveWebContents(), 0);
  ASSERT_TRUE(iframe);
  EXPECT_EQ(iframe->GetLastCommittedURL(),
            embedded_test_server()->GetURL("/title1.html"));

  EXPECT_EQ(pdf_frames[0]->GetLastCommittedOrigin(),
            iframe->GetLastCommittedOrigin());
  EXPECT_NE(pdf_frames[0]->GetProcess(), iframe->GetProcess());
  EXPECT_FALSE(content::HasOriginKeyedProcess(pdf_frames[0]));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionIsolatedContentTest, DataNavigation) {
  WebContents* guest_contents = LoadPdfGetGuestContents(
      embedded_test_server()->GetURL("/pdf/data_url_rectangles.html"));

  // The PDF plugin frame and the extension main frame should not share renderer
  // processes even though the extension triggers a data: navigation when
  // loading its plugin.
  std::vector<content::RenderFrameHost*> pdf_frames =
      GetUnseasonedPdfFrames(guest_contents);
  ASSERT_EQ(pdf_frames.size(), 1u);
  EXPECT_NE(pdf_frames[0]->GetProcess(),
            guest_contents->GetMainFrame()->GetProcess());
}

IN_PROC_BROWSER_TEST_P(PDFExtensionIsolatedContentTest, HistoryNavigation) {
  // Navigating to a PDF should spawn a PDF renderer process.
  EXPECT_TRUE(LoadPdf(embedded_test_server()->GetURL("/pdf/test.pdf")));
  EXPECT_EQ(CountPDFProcesses(), 1);

  // Navigating to non-PDF content should remove the PDF renderer process.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_EQ(CountPDFProcesses(), 0);

  // Navigating back to the PDF should once again spawn a PDF renderer process.
  WebContents* web_contents = GetActiveWebContents();
  web_contents->GetController().GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(web_contents));
  EXPECT_EQ(CountPDFProcesses(), 1);
}

IN_PROC_BROWSER_TEST_P(PDFExtensionIsolatedContentTest, Jitless) {
  WebContents* guest_contents =
      LoadPdfGetGuestContents(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest_contents);

  // PDF content should always be in JIT-less processes.
  std::vector<content::RenderFrameHost*> pdf_frames =
      GetUnseasonedPdfFrames(guest_contents);
  ASSERT_EQ(pdf_frames.size(), 1u);
  EXPECT_TRUE(pdf_frames[0]->GetProcess()->IsJitDisabled());
}

INSTANTIATE_TEST_SUITE_P(All,
                         PDFExtensionIsolatedContentTest,
                         testing::Values(true));

class PDFExtensionLinkClickTest : public PDFExtensionTest {
 public:
  PDFExtensionLinkClickTest() : guest_contents_(nullptr) {}
  ~PDFExtensionLinkClickTest() override {}

 protected:
  WebContents* LoadTestLinkPdfGetGuestContents() {
    guest_contents_ = LoadPdfGetGuestContents(
        embedded_test_server()->GetURL("/pdf/test-link.pdf"));
    EXPECT_TRUE(guest_contents_);
    return guest_contents_;
  }

  // The rectangle of the link in test-link.pdf is [72 706 164 719] in PDF user
  // space. To calculate a position inside this rectangle, several
  // transformations have to be applied:
  // [a] (110, 110) in Blink page coordinates ->
  // [b] (219, 169) in Blink screen coordinates ->
  // [c] (115, 169) in PDF Device space coordinates ->
  // [d] (82.5, 709.5) in PDF user space coordinates.
  // This performs the [a] to [b] transformation, since that is the coordinate
  // space content::SimulateMouseClickAt() needs.
  gfx::Point GetLinkPosition() {
    return ConvertPageCoordToScreenCoord(guest_contents_, {110, 110});
  }

  void SetGuestContents(WebContents* guest_contents) {
    ASSERT_TRUE(guest_contents);
    guest_contents_ = guest_contents;
  }

  WebContents* GetWebContentsForInputRouting() { return guest_contents_; }

 private:
  raw_ptr<WebContents> guest_contents_;
};

IN_PROC_BROWSER_TEST_P(PDFExtensionLinkClickTest, CtrlLeft) {
  LoadTestLinkPdfGetGuestContents();

  WebContents* web_contents = GetActiveWebContents();

  content::SimulateMouseClickAt(
      GetWebContentsForInputRouting(), kDefaultKeyModifier,
      blink::WebMouseEvent::Button::kLeft, GetLinkPosition());
  ui_test_utils::TabAddedWaiter(browser()).Wait();

  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  WebContents* active_web_contents = GetActiveWebContents();
  ASSERT_EQ(web_contents, active_web_contents);

  WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(new_web_contents);
  ASSERT_NE(web_contents, new_web_contents);

  const GURL& url = new_web_contents->GetVisibleURL();
  EXPECT_EQ("http://www.example.com/", url.spec());
}

IN_PROC_BROWSER_TEST_P(PDFExtensionLinkClickTest, Middle) {
  LoadTestLinkPdfGetGuestContents();

  WebContents* web_contents = GetActiveWebContents();

  content::SimulateMouseClickAt(GetWebContentsForInputRouting(), 0,
                                blink::WebMouseEvent::Button::kMiddle,
                                GetLinkPosition());
  ui_test_utils::TabAddedWaiter(browser()).Wait();

  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  WebContents* active_web_contents = GetActiveWebContents();
  ASSERT_EQ(web_contents, active_web_contents);

  WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(new_web_contents);
  ASSERT_NE(web_contents, new_web_contents);

  const GURL& url = new_web_contents->GetVisibleURL();
  EXPECT_EQ("http://www.example.com/", url.spec());
}

IN_PROC_BROWSER_TEST_P(PDFExtensionLinkClickTest, CtrlShiftLeft) {
  LoadTestLinkPdfGetGuestContents();

  WebContents* web_contents = GetActiveWebContents();

  const int modifiers = blink::WebInputEvent::kShiftKey | kDefaultKeyModifier;

  content::SimulateMouseClickAt(GetWebContentsForInputRouting(), modifiers,
                                blink::WebMouseEvent::Button::kLeft,
                                GetLinkPosition());
  ui_test_utils::TabAddedWaiter(browser()).Wait();

  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  WebContents* active_web_contents = GetActiveWebContents();
  ASSERT_NE(web_contents, active_web_contents);

  const GURL& url = active_web_contents->GetVisibleURL();
  EXPECT_EQ("http://www.example.com/", url.spec());
}

IN_PROC_BROWSER_TEST_P(PDFExtensionLinkClickTest, ShiftMiddle) {
  LoadTestLinkPdfGetGuestContents();

  WebContents* web_contents = GetActiveWebContents();

  content::SimulateMouseClickAt(
      GetWebContentsForInputRouting(), blink::WebInputEvent::kShiftKey,
      blink::WebMouseEvent::Button::kMiddle, GetLinkPosition());
  ui_test_utils::TabAddedWaiter(browser()).Wait();

  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  WebContents* active_web_contents = GetActiveWebContents();
  ASSERT_NE(web_contents, active_web_contents);

  const GURL& url = active_web_contents->GetVisibleURL();
  EXPECT_EQ("http://www.example.com/", url.spec());
}

IN_PROC_BROWSER_TEST_P(PDFExtensionLinkClickTest, ShiftLeft) {
  LoadTestLinkPdfGetGuestContents();

  ASSERT_EQ(1U, chrome::GetTotalBrowserCount());

  WebContents* web_contents = GetActiveWebContents();

  content::SimulateMouseClickAt(
      GetWebContentsForInputRouting(), blink::WebInputEvent::kShiftKey,
      blink::WebMouseEvent::Button::kLeft, GetLinkPosition());
  ui_test_utils::WaitForBrowserToOpen();

  ASSERT_EQ(2U, chrome::GetTotalBrowserCount());

  WebContents* active_web_contents =
      chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(web_contents, active_web_contents);

  const GURL& url = active_web_contents->GetVisibleURL();
  EXPECT_EQ("http://www.example.com/", url.spec());
}

// This test opens a PDF by clicking a link via javascript and verifies that
// the PDF is loaded and functional by clicking a link in the PDF. The link
// click in the PDF opens a new tab. The main page handles the pageShow event
// and updates the history state.
IN_PROC_BROWSER_TEST_P(PDFExtensionLinkClickTest, OpenPDFWithReplaceState) {
  // Navigate to the main page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/pdf/pdf_href_replace_state.html")));
  WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Click on the link which opens the PDF via JS.
  content::TestNavigationObserver navigation_observer(web_contents);
  const char kPdfLinkClick[] = "document.getElementById('link').click();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, kPdfLinkClick));
  navigation_observer.Wait();
  const GURL& current_url = web_contents->GetLastCommittedURL();
  ASSERT_EQ("/pdf/test-link.pdf", current_url.path());

  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(web_contents));

  // Now click on the link to example.com in the PDF. This should open up a new
  // tab.
  content::BrowserPluginGuestManager* guest_manager =
      web_contents->GetBrowserContext()->GetGuestManager();
  SetGuestContents(guest_manager->GetFullPageGuest(web_contents));

  content::SimulateMouseClickAt(
      GetWebContentsForInputRouting(), kDefaultKeyModifier,
      blink::WebMouseEvent::Button::kLeft, GetLinkPosition());
  ui_test_utils::TabAddedWaiter(browser()).Wait();

  // We should have two tabs now. One with the PDF and the second for
  // example.com
  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  WebContents* active_web_contents = GetActiveWebContents();
  ASSERT_EQ(web_contents, active_web_contents);

  WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(new_web_contents);
  ASSERT_NE(web_contents, new_web_contents);

  const GURL& url = new_web_contents->GetVisibleURL();
  EXPECT_EQ("http://www.example.com/", url.spec());
}

namespace {
// Fails the test if a navigation is started in the given WebContents.
class FailOnNavigation : public content::WebContentsObserver {
 public:
  explicit FailOnNavigation(WebContents* contents)
      : content::WebContentsObserver(contents) {}

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    ADD_FAILURE() << "Unexpected navigation";
  }
};
}  // namespace

// If the PDF viewer can't navigate the tab using a tab id, make sure it doesn't
// try to navigate the mime handler extension's frame.
// Regression test for https://crbug.com/1158381
IN_PROC_BROWSER_TEST_P(PDFExtensionLinkClickTest, LinkClickInPdfInNonTab) {
  // For ease of testing, we'll still load the PDF in a tab, but we clobber the
  // tab id in the viewer to make it think it's not in a tab.
  WebContents* guest_contents = LoadTestLinkPdfGetGuestContents();
  ASSERT_TRUE(guest_contents);
  ASSERT_TRUE(
      content::ExecuteScript(guest_contents,
                             "window.viewer.browserApi.getStreamInfo().tabId = "
                             "    chrome.tabs.TAB_ID_NONE;"));

  FailOnNavigation fail_if_mimehandler_navigates(guest_contents);
  content::SimulateMouseClickAt(
      GetWebContentsForInputRouting(), blink::WebInputEvent::kNoModifiers,
      blink::WebMouseEvent::Button::kLeft, GetLinkPosition());

  // Since the guest contents is for a mime handling extension (in this case,
  // the PDF viewer extension), it must not navigate away from the extension. If
  // |fail_if_mimehandler_navigates| doesn't see a navigation, we consider the
  // test to have passed.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();
}

class PDFExtensionInternalLinkClickTest : public PDFExtensionTest {
 public:
  PDFExtensionInternalLinkClickTest() : guest_contents_(nullptr) {}
  ~PDFExtensionInternalLinkClickTest() override {}

 protected:
  void LoadTestLinkPdfGetGuestContents() {
    guest_contents_ = LoadPdfGetGuestContents(
        embedded_test_server()->GetURL("/pdf/test-internal-link.pdf"));
    ASSERT_TRUE(guest_contents_);
  }

  gfx::Point GetLinkPosition() {
    // The whole first page is a link.
    return ConvertPageCoordToScreenCoord(guest_contents_, {100, 100});
  }

  WebContents* GetWebContentsForInputRouting() { return guest_contents_; }

 private:
  raw_ptr<WebContents> guest_contents_;
};

IN_PROC_BROWSER_TEST_P(PDFExtensionInternalLinkClickTest, CtrlLeft) {
  LoadTestLinkPdfGetGuestContents();

  WebContents* web_contents = GetActiveWebContents();

  content::SimulateMouseClickAt(
      GetWebContentsForInputRouting(), kDefaultKeyModifier,
      blink::WebMouseEvent::Button::kLeft, GetLinkPosition());
  ui_test_utils::TabAddedWaiter(browser()).Wait();

  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  WebContents* active_web_contents = GetActiveWebContents();
  ASSERT_EQ(web_contents, active_web_contents);

  WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(new_web_contents);
  ASSERT_NE(web_contents, new_web_contents);

  const GURL& url = new_web_contents->GetVisibleURL();
  EXPECT_EQ("/pdf/test-internal-link.pdf", url.path());
  EXPECT_EQ("page=2&zoom=100,0,200", url.ref());
}

IN_PROC_BROWSER_TEST_P(PDFExtensionInternalLinkClickTest, Middle) {
  LoadTestLinkPdfGetGuestContents();

  WebContents* web_contents = GetActiveWebContents();

  content::SimulateMouseClickAt(GetWebContentsForInputRouting(), 0,
                                blink::WebMouseEvent::Button::kMiddle,
                                GetLinkPosition());
  ui_test_utils::TabAddedWaiter(browser()).Wait();

  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  WebContents* active_web_contents = GetActiveWebContents();
  ASSERT_EQ(web_contents, active_web_contents);

  WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(new_web_contents);
  ASSERT_NE(web_contents, new_web_contents);

  const GURL& url = new_web_contents->GetVisibleURL();
  EXPECT_EQ("/pdf/test-internal-link.pdf", url.path());
  EXPECT_EQ("page=2&zoom=100,0,200", url.ref());
}

IN_PROC_BROWSER_TEST_P(PDFExtensionInternalLinkClickTest, ShiftLeft) {
  LoadTestLinkPdfGetGuestContents();

  ASSERT_EQ(1U, chrome::GetTotalBrowserCount());

  WebContents* web_contents = GetActiveWebContents();

  content::SimulateMouseClickAt(
      GetWebContentsForInputRouting(), blink::WebInputEvent::kShiftKey,
      blink::WebMouseEvent::Button::kLeft, GetLinkPosition());
  ui_test_utils::WaitForBrowserToOpen();

  ASSERT_EQ(2U, chrome::GetTotalBrowserCount());

  WebContents* active_web_contents =
      chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(web_contents, active_web_contents);

  const GURL& url = active_web_contents->GetVisibleURL();
  EXPECT_EQ("/pdf/test-internal-link.pdf", url.path());
  EXPECT_EQ("page=2&zoom=100,0,200", url.ref());
}

class PDFExtensionComboBoxTest : public PDFExtensionTest {
 public:
  void LoadTestComboBoxPdfGetGuestContents() {
    guest_contents_ = LoadPdfGetGuestContents(
        embedded_test_server()->GetURL("/pdf/combobox_form.pdf"));
    ASSERT_TRUE(guest_contents_);
  }

  // Returns a point near the left edge of the editable combo box in
  // combobox_form.pdf, inside the combo box rect. The point is in Blink screen
  // coordinates.
  //
  // The combo box's rect is [100 50 200 80] in PDF user space. (136, 318) in
  // Blink page coordinates corresponds to approximately (102, 62) in PDF user
  // space coordinates. See PDFExtensionLinkClickTest::GetLinkPosition() for
  // more information on all the coordinate systems involved.
  gfx::Point GetEditableComboBoxLeftPosition() {
    return ConvertPageCoordToScreenCoord(guest_contents_, {136, 318});
  }

  void ClickLeftSideOfEditableComboBox() {
    WebContents* contents = GetWebContentsForInputRouting();
    content::SimulateMouseClickAt(contents, 0,
                                  blink::WebMouseEvent::Button::kLeft,
                                  GetEditableComboBoxLeftPosition());

    // Make sure mouse events are sent completely before proceeding, in order to
    // avoid races with subsequent keyboard events.
    content::InputEventAckWaiter mouse_waiter(
        GetPluginFrame(contents)->GetRenderWidgetHost(),
        blink::WebInputEvent::Type::kMouseUp);
    mouse_waiter.Wait();
  }

  void TypeHello() {
    struct KeyData {
      char ch;
      ui::DomCode code;
      ui::KeyboardCode key_code;
    };

    constexpr KeyData kData[] = {
        {'H', ui::DomCode::US_H, ui::VKEY_H},
        {'E', ui::DomCode::US_E, ui::VKEY_E},
        {'L', ui::DomCode::US_L, ui::VKEY_L},
        {'L', ui::DomCode::US_L, ui::VKEY_L},
        {'O', ui::DomCode::US_O, ui::VKEY_O},
    };

    auto* contents = GetWebContentsForInputRouting();
    auto* rwh = GetPluginFrame(contents)->GetRenderWidgetHost();
    for (const auto& data : kData) {
      content::SimulateKeyPress(contents, ui::DomKey::FromCharacter(data.ch),
                                data.code, data.key_code, /*control=*/false,
                                /*shift=*/false, /*alt=*/false,
                                /*command=*/false);
      content::InputEventAckWaiter key_waiter(
          rwh, blink::WebInputEvent::Type::kKeyUp);
      key_waiter.Wait();
    }
  }

  // Presses the left arrow key.
  void PressLeftArrow() {
    content::SimulateKeyPressWithoutChar(
        GetWebContentsForInputRouting(), ui::DomKey::ARROW_LEFT,
        ui::DomCode::ARROW_LEFT, ui::VKEY_LEFT, false, false, false, false);
  }

  // Presses down shift, presses the left arrow, and lets go of shift.
  void PressShiftLeftArrow() {
    content::SimulateKeyPressWithoutChar(GetWebContentsForInputRouting(),
                                         ui::DomKey::ARROW_LEFT,
                                         ui::DomCode::ARROW_LEFT, ui::VKEY_LEFT,
                                         false, /*shift=*/true, false, false);
  }

  // Presses the right arrow key.
  void PressRightArrow() {
    content::SimulateKeyPressWithoutChar(
        GetWebContentsForInputRouting(), ui::DomKey::ARROW_RIGHT,
        ui::DomCode::ARROW_RIGHT, ui::VKEY_RIGHT, false, false, false, false);
  }

  // Presses down shift, presses the right arrow, and lets go of shift.
  void PressShiftRightArrow() {
    content::SimulateKeyPressWithoutChar(
        GetWebContentsForInputRouting(), ui::DomKey::ARROW_RIGHT,
        ui::DomCode::ARROW_RIGHT, ui::VKEY_RIGHT, false, /*shift=*/true, false,
        false);
  }

  WebContents* GetWebContentsForInputRouting() { return guest_contents_; }

 private:
  raw_ptr<WebContents> guest_contents_ = nullptr;
};

class PDFExtensionSaveTest : public PDFExtensionComboBoxTest {
 public:
  void SetUpOnMainThread() override {
    PDFExtensionComboBoxTest::SetUpOnMainThread();

    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void SaveEditedPdf() {
    ASSERT_TRUE(content::ExecuteScript(
        GetWebContentsForInputRouting(),
        "var viewer = document.getElementById('viewer');"
        "var toolbar = viewer.shadowRoot.getElementById('toolbar');"
        "var downloads = toolbar.shadowRoot.getElementById('downloads');"
        "downloads.shadowRoot.getElementById('download-edited').click();"));
  }

  void WaitForSavedPdf(const base::FilePath& path) {
    while (!base::PathExists(path))
      content::RunAllTasksUntilIdle();
  }

  base::FilePath GetDownloadDir() const { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
};

// Flaky, http://crbug.com/1269103
#if defined(OS_LINUX) || defined(OS_WIN)
#define MAYBE_Save DISABLED_Save
#else
#define MAYBE_Save Save
#endif
IN_PROC_BROWSER_TEST_P(PDFExtensionSaveTest, MAYBE_Save) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath save_path = GetDownloadDir().AppendASCII("edited.pdf");
  ASSERT_FALSE(base::PathExists(save_path));

  using FileChooser = extensions::FileSystemChooseEntryFunction;
  FileChooser::SkipPickerAndAlwaysSelectPathForTest file_picker(save_path);

  LoadTestComboBoxPdfGetGuestContents();
  ClickLeftSideOfEditableComboBox();
  TypeHello();
  SaveEditedPdf();
  WaitForSavedPdf(save_path);
}

class PDFExtensionSaveWithPolicyTest : public PDFExtensionSaveTest {
 public:
  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void SetDownloadPolicyManagedPath(const base::FilePath& path) {
    policy::PolicyMap policies;
    policies.Set(policy::key::kDownloadDirectory,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(path.AsUTF8Unsafe()),
                 nullptr);
    policy_provider_.UpdateChromePolicy(policies);
    base::RunLoop().RunUntilIdle();
  }

  void CreateConflictingFilenames(const base::FilePath& path, int count) {
    for (int i = 0; i < count; ++i) {
      base::FilePath unique_path = base::GetUniquePath(path);
      ASSERT_TRUE(!unique_path.empty());
      ASSERT_TRUE(base::WriteFile(unique_path, ""));
    }
  }

  int CountPdfFilesInDir(const base::FilePath& dir) {
    base::FileEnumerator file_enumerator(dir,
                                         /*recursive=*/false,
                                         base::FileEnumerator::FILES,
                                         FILE_PATH_LITERAL("*.pdf"));

    int count = 0;
    for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
         file_path = file_enumerator.Next()) {
      ++count;
    }
    return count;
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

// Flaky, http://crbug.com/1269103
#if defined(OS_LINUX) || defined(OS_WIN)
#define MAYBE_SaveWithPolicy DISABLED_SaveWithPolicy
#else
#define MAYBE_SaveWithPolicy SaveWithPolicy
#endif
IN_PROC_BROWSER_TEST_P(PDFExtensionSaveWithPolicyTest, MAYBE_SaveWithPolicy) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath save_path = GetDownloadDir().AppendASCII("combobox_form.pdf");
  ASSERT_FALSE(base::PathExists(save_path));

  SetDownloadPolicyManagedPath(GetDownloadDir());
  DownloadPrefs::FromBrowserContext(profile())
      ->SkipSanitizeDownloadTargetPathForTesting();

  LoadTestComboBoxPdfGetGuestContents();
  ClickLeftSideOfEditableComboBox();
  TypeHello();
  SaveEditedPdf();
  WaitForSavedPdf(save_path);
}

// Flaky, http://crbug.com/1269103
#if defined(OS_LINUX)
#define MAYBE_SaveWithPolicyUniqueNumberSuffix \
  DISABLED_SaveWithPolicyUniqueNumberSuffix
#else
#define MAYBE_SaveWithPolicyUniqueNumberSuffix SaveWithPolicyUniqueNumberSuffix
#endif
IN_PROC_BROWSER_TEST_P(PDFExtensionSaveWithPolicyTest,
                       MAYBE_SaveWithPolicyUniqueNumberSuffix) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  CreateConflictingFilenames(GetDownloadDir().AppendASCII("combobox_form.pdf"),
                             5);
  EXPECT_EQ(5, CountPdfFilesInDir(GetDownloadDir()));

  base::FilePath save_path =
      GetDownloadDir().AppendASCII("combobox_form (5).pdf");
  ASSERT_FALSE(base::PathExists(save_path));

  SetDownloadPolicyManagedPath(GetDownloadDir());
  DownloadPrefs::FromBrowserContext(profile())
      ->SkipSanitizeDownloadTargetPathForTesting();

  LoadTestComboBoxPdfGetGuestContents();
  ClickLeftSideOfEditableComboBox();
  TypeHello();
  SaveEditedPdf();
  WaitForSavedPdf(save_path);
}

// Flaky, http://crbug.com/1269103
#if defined(OS_LINUX) || defined(OS_WIN)
#define MAYBE_SaveWithPolicyUniqueTimeSuffix \
  DISABLED_SaveWithPolicyUniqueTimeSuffix
#else
#define MAYBE_SaveWithPolicyUniqueTimeSuffix SaveWithPolicyUniqueTimeSuffix
#endif
IN_PROC_BROWSER_TEST_P(PDFExtensionSaveWithPolicyTest,
                       MAYBE_SaveWithPolicyUniqueTimeSuffix) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  CreateConflictingFilenames(GetDownloadDir().AppendASCII("combobox_form.pdf"),
                             101);
  EXPECT_EQ(101, CountPdfFilesInDir(GetDownloadDir()));

  SetDownloadPolicyManagedPath(GetDownloadDir());
  DownloadPrefs::FromBrowserContext(profile())
      ->SkipSanitizeDownloadTargetPathForTesting();

  LoadTestComboBoxPdfGetGuestContents();
  ClickLeftSideOfEditableComboBox();
  TypeHello();
  SaveEditedPdf();
  while (CountPdfFilesInDir(GetDownloadDir()) != 102)
    content::RunAllTasksUntilIdle();
}

class PDFExtensionClipboardTest : public PDFExtensionComboBoxTest,
                                  public ui::ClipboardObserver {
 public:
  // PDFExtensionTest:
  void SetUpOnMainThread() override {
    PDFExtensionTest::SetUpOnMainThread();
    ui::TestClipboard::CreateForCurrentThread();
  }
  void TearDownOnMainThread() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
    PDFExtensionTest::TearDownOnMainThread();
  }

  // ui::ClipboardObserver:
  void OnClipboardDataChanged() override {
    DCHECK(!clipboard_changed_);
    clipboard_changed_ = true;
    std::move(clipboard_quit_closure_).Run();
  }

  // Runs `action` and checks the Linux selection clipboard contains `expected`.
  void DoActionAndCheckSelectionClipboard(base::OnceClosure action,
                                          const std::string& expected) {
    if (ui::Clipboard::IsSupportedClipboardBuffer(
            ui::ClipboardBuffer::kSelection)) {
      DoActionAndCheckClipboard(std::move(action),
                                ui::ClipboardBuffer::kSelection, expected);
    } else {
      // Even though there is no selection clipboard to check, `action` still
      // needs to run.
      std::move(action).Run();
    }
  }

  // Sends a copy command and checks the copy/paste clipboard.
  // Note: Trying to send ctrl+c does not work correctly with
  // SimulateKeyPress(). Using IDC_COPY does not work on Mac in browser_tests.
  void SendCopyCommandAndCheckCopyPasteClipboard(const std::string& expected) {
    DoActionAndCheckClipboard(base::BindLambdaForTesting([&]() {
                                GetWebContentsForInputRouting()->Copy();
                              }),
                              ui::ClipboardBuffer::kCopyPaste, expected);
  }

 private:
  // Runs `action` and checks `clipboard_buffer` contains `expected`.
  void DoActionAndCheckClipboard(base::OnceClosure action,
                                 ui::ClipboardBuffer clipboard_buffer,
                                 const std::string& expected) {
    ASSERT_FALSE(clipboard_quit_closure_);

    ui::ClipboardMonitor::GetInstance()->AddObserver(this);
    EXPECT_FALSE(clipboard_changed_);
    clipboard_changed_ = false;

    base::RunLoop run_loop;
    clipboard_quit_closure_ = run_loop.QuitClosure();
    std::move(action).Run();
    run_loop.Run();
    EXPECT_FALSE(clipboard_quit_closure_);

    EXPECT_TRUE(clipboard_changed_);
    clipboard_changed_ = false;
    ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);

    auto* clipboard = ui::Clipboard::GetForCurrentThread();
    std::string clipboard_data;
    clipboard->ReadAsciiText(clipboard_buffer, /* data_dst=*/nullptr,
                             &clipboard_data);
    EXPECT_EQ(expected, clipboard_data);
  }

  base::RepeatingClosure clipboard_quit_closure_;
  bool clipboard_changed_ = false;
};

// TODO(crbug.com/1268983): Fix flakiness on Linux and reenable.
#if defined(OS_LINUX)
#define MAYBE_IndividualShiftRightArrowPresses \
  DISABLED_IndividualShiftRightArrowPresses
#else
#define MAYBE_IndividualShiftRightArrowPresses IndividualShiftRightArrowPresses
#endif
IN_PROC_BROWSER_TEST_P(PDFExtensionClipboardTest,
                       MAYBE_IndividualShiftRightArrowPresses) {
  LoadTestComboBoxPdfGetGuestContents();

  // Give the editable combo box focus.
  ClickLeftSideOfEditableComboBox();

  TypeHello();

  // Put the cursor back to the left side of the combo box.
  ClickLeftSideOfEditableComboBox();

  // Press shift + right arrow 3 times. Letting go of shift in between.
  auto action = base::BindLambdaForTesting([&]() { PressShiftRightArrow(); });
  DoActionAndCheckSelectionClipboard(action, "H");
  DoActionAndCheckSelectionClipboard(action, "HE");
  DoActionAndCheckSelectionClipboard(action, "HEL");
  SendCopyCommandAndCheckCopyPasteClipboard("HEL");
}

// TODO(crbug.com/897801): test is flaky.
IN_PROC_BROWSER_TEST_P(PDFExtensionClipboardTest,
                       DISABLED_IndividualShiftLeftArrowPresses) {
  LoadTestComboBoxPdfGetGuestContents();

  // Give the editable combo box focus.
  ClickLeftSideOfEditableComboBox();

  TypeHello();

  // Put the cursor back to the left side of the combo box.
  ClickLeftSideOfEditableComboBox();

  for (int i = 0; i < 3; ++i)
    PressRightArrow();

  // Press shift + left arrow 2 times. Letting go of shift in between.
  auto action = base::BindLambdaForTesting([&]() { PressShiftLeftArrow(); });
  DoActionAndCheckSelectionClipboard(action, "L");
  DoActionAndCheckSelectionClipboard(action, "EL");
  SendCopyCommandAndCheckCopyPasteClipboard("EL");

  // Press shift + left arrow 2 times. Letting go of shift in between.
  DoActionAndCheckSelectionClipboard(action, "HEL");
  DoActionAndCheckSelectionClipboard(action, "HEL");
  SendCopyCommandAndCheckCopyPasteClipboard("HEL");
}

// Flaky, http://crbug.com/1269104
#if defined(OS_LINUX)
#define MAYBE_CombinedShiftRightArrowPresses \
  DISABLED_CombinedShiftRightArrowPresses
#else
#define MAYBE_CombinedShiftRightArrowPresses CombinedShiftRightArrowPresses
#endif
IN_PROC_BROWSER_TEST_P(PDFExtensionClipboardTest,
                       MAYBE_CombinedShiftRightArrowPresses) {
  LoadTestComboBoxPdfGetGuestContents();

  // Give the editable combo box focus.
  ClickLeftSideOfEditableComboBox();

  TypeHello();

  // Put the cursor back to the left side of the combo box.
  ClickLeftSideOfEditableComboBox();

  // Press shift + right arrow 3 times. Holding down shift in between.
  {
    content::ScopedSimulateModifierKeyPress hold_shift(
        GetWebContentsForInputRouting(), false, true, false, false);
    auto action = base::BindLambdaForTesting([&]() {
      hold_shift.KeyPressWithoutChar(ui::DomKey::ARROW_RIGHT,
                                     ui::DomCode::ARROW_RIGHT, ui::VKEY_RIGHT);
    });
    DoActionAndCheckSelectionClipboard(action, "H");
    DoActionAndCheckSelectionClipboard(action, "HE");
    DoActionAndCheckSelectionClipboard(action, "HEL");
  }
  SendCopyCommandAndCheckCopyPasteClipboard("HEL");
}

// Flaky on Linux (https://crbug.com/1121446)
#if defined(OS_LINUX)
#define MAYBE_CombinedShiftArrowPresses DISABLED_CombinedShiftArrowPresses
#else
#define MAYBE_CombinedShiftArrowPresses CombinedShiftArrowPresses
#endif
IN_PROC_BROWSER_TEST_P(PDFExtensionClipboardTest,
                       MAYBE_CombinedShiftArrowPresses) {
  LoadTestComboBoxPdfGetGuestContents();

  // Give the editable combo box focus.
  ClickLeftSideOfEditableComboBox();

  TypeHello();

  // Put the cursor back to the left side of the combo box.
  ClickLeftSideOfEditableComboBox();

  for (int i = 0; i < 3; ++i)
    PressRightArrow();

  // Press shift + left arrow 3 times. Holding down shift in between.
  {
    content::ScopedSimulateModifierKeyPress hold_shift(
        GetWebContentsForInputRouting(), false, true, false, false);
    auto action = base::BindLambdaForTesting([&]() {
      hold_shift.KeyPressWithoutChar(ui::DomKey::ARROW_LEFT,
                                     ui::DomCode::ARROW_LEFT, ui::VKEY_LEFT);
    });
    DoActionAndCheckSelectionClipboard(action, "L");
    DoActionAndCheckSelectionClipboard(action, "EL");
    DoActionAndCheckSelectionClipboard(action, "HEL");
  }
  SendCopyCommandAndCheckCopyPasteClipboard("HEL");

  // Press shift + right arrow 2 times. Holding down shift in between.
  {
    content::ScopedSimulateModifierKeyPress hold_shift(
        GetWebContentsForInputRouting(), false, true, false, false);
    auto action = base::BindLambdaForTesting([&]() {
      hold_shift.KeyPressWithoutChar(ui::DomKey::ARROW_RIGHT,
                                     ui::DomCode::ARROW_RIGHT, ui::VKEY_RIGHT);
    });
    DoActionAndCheckSelectionClipboard(action, "EL");
    DoActionAndCheckSelectionClipboard(action, "L");
  }
  SendCopyCommandAndCheckCopyPasteClipboard("L");
}

// Verifies that an <embed> of size zero will still instantiate a guest and post
// message to the <embed> is correctly forwarded to the extension. This is for
// catching future regression in docs/ and slides/ pages (see
// https://crbug.com/763812).
IN_PROC_BROWSER_TEST_P(PDFExtensionTest, PostMessageForZeroSizedEmbed) {
  content::DOMMessageQueue queue;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/pdf/post_message_zero_sized_embed.html")));
  std::string message;
  EXPECT_TRUE(queue.WaitForMessage(&message));
  EXPECT_EQ("\"POST_MESSAGE_OK\"", message);
}

// In response to the events sent in |send_events|, ensures the PDF viewer zooms
// in and that the viewer's custom pinch zooming mechanism is used to do so.
void EnsureCustomPinchZoomInvoked(WebContents* guest_contents,
                                  WebContents* contents,
                                  base::OnceClosure send_events) {
  constexpr char kListenPinchUpdate[] = R"(
      const gestureDetector = viewer.viewport.getGestureDetectorForTesting();
      const updatePromise = new Promise((resolve) => {
        gestureDetector.getEventTarget().addEventListener('pinchupdate',
                                                          resolve);
      });
  )";
  ASSERT_TRUE(content::ExecuteScript(guest_contents, kListenPinchUpdate));

  zoom::ZoomChangedWatcher zoom_watcher(
      contents,
      base::BindRepeating(
          [](const zoom::ZoomController::ZoomChangedEventData& event) {
            return event.new_zoom_level > event.old_zoom_level &&
                   event.zoom_mode == zoom::ZoomController::ZOOM_MODE_MANUAL &&
                   !event.can_show_bubble;
          }));

  std::move(send_events).Run();

  bool got_update;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      guest_contents,
      "updatePromise.then(function(update) { "
      "  window.domAutomationController.send(!!update); "
      "});",
      &got_update));
  EXPECT_TRUE(got_update);

  zoom_watcher.Wait();

  // Check that the browser's native pinch zoom was prevented.
  double scale_factor;
  ASSERT_TRUE(content::ExecuteScriptAndExtractDouble(
      contents,
      "window.domAutomationController.send(window.visualViewport.scale);",
      &scale_factor));
  EXPECT_DOUBLE_EQ(1.0, scale_factor);
}

// Ensure that touchpad pinch events are handled by the PDF viewer.
IN_PROC_BROWSER_TEST_P(PDFExtensionTest, TouchpadPinchInvokesCustomZoom) {
  WebContents* guest_contents =
      LoadPdfGetGuestContents(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest_contents);

  base::OnceClosure send_pinch = base::BindOnce(
      [](WebContents* guest_contents) {
        const gfx::Rect guest_rect = guest_contents->GetContainerBounds();
        const gfx::Point mouse_position(guest_rect.width() / 2,
                                        guest_rect.height() / 2);
        content::SimulateGesturePinchSequence(
            guest_contents, mouse_position, 1.23,
            blink::WebGestureDevice::kTouchpad);
      },
      guest_contents);

  EnsureCustomPinchZoomInvoked(guest_contents, GetActiveWebContents(),
                               std::move(send_pinch));
}

#if !defined(OS_MAC)
// Ensure that ctrl-wheel events are handled by the PDF viewer.
IN_PROC_BROWSER_TEST_P(PDFExtensionTest, CtrlWheelInvokesCustomZoom) {
  WebContents* guest_contents =
      LoadPdfGetGuestContents(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest_contents);

  base::OnceClosure send_ctrl_wheel = base::BindOnce(
      [](WebContents* guest_contents) {
        const gfx::Rect guest_rect = guest_contents->GetContainerBounds();
        const gfx::Point mouse_position(guest_rect.width() / 2,
                                        guest_rect.height() / 2);
        content::SimulateMouseWheelCtrlZoomEvent(
            guest_contents, mouse_position, true,
            blink::WebMouseWheelEvent::kPhaseBegan);
      },
      guest_contents);

  EnsureCustomPinchZoomInvoked(guest_contents, GetActiveWebContents(),
                               std::move(send_ctrl_wheel));
}

// Flaky on ChromeOS (https://crbug.com/922974)
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_TouchscreenPinchInvokesCustomZoom \
  DISABLED_TouchscreenPinchInvokesCustomZoom
#else
#define MAYBE_TouchscreenPinchInvokesCustomZoom \
  TouchscreenPinchInvokesCustomZoom
#endif
IN_PROC_BROWSER_TEST_P(PDFExtensionTest,
                       MAYBE_TouchscreenPinchInvokesCustomZoom) {
  WebContents* guest_contents =
      LoadPdfGetGuestContents(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest_contents);

  base::OnceClosure send_touchscreen_pinch = base::BindOnce(
      [](WebContents* guest_contents) {
        const gfx::Rect guest_rect = guest_contents->GetContainerBounds();
        const gfx::PointF anchor_position(guest_rect.width() / 2,
                                          guest_rect.height() / 2);
        base::RunLoop run_loop;
        content::SimulateTouchscreenPinch(guest_contents, anchor_position, 1.2f,
                                          run_loop.QuitClosure());
        run_loop.Run();
      },
      guest_contents);

  EnsureCustomPinchZoomInvoked(guest_contents, GetActiveWebContents(),
                               std::move(send_touchscreen_pinch));
}

#endif  // !defined(OS_MAC)

using PDFExtensionHitTestTest = PDFExtensionTest;

// Flaky in nearly all configurations; see https://crbug.com/856169.
IN_PROC_BROWSER_TEST_P(PDFExtensionHitTestTest, DISABLED_MouseLeave) {
  // Load page with embedded PDF and make sure it succeeds.
  WebContents* guest_contents = LoadPdfGetGuestContents(
      embedded_test_server()->GetURL("/pdf/pdf_embed.html"));
  ASSERT_TRUE(guest_contents);

  gfx::Point point_in_parent(250, 25);
  gfx::Point point_in_pdf(250, 250);

  // Inject script to count MouseLeaves in the PDF.
  ASSERT_TRUE(content::ExecuteScript(
      guest_contents,
      "var enter_count = 0;\n"
      "var leave_count = 0;\n"
      "document.addEventListener('mouseenter', function (){\n"
      "  enter_count++;"
      "});\n"
      "document.addEventListener('mouseleave', function (){\n"
      "  leave_count++;"
      "});"));

  // Inject some MouseMoves to invoke a MouseLeave in the PDF.
  WebContents* embedder_contents = GetActiveWebContents();
  content::SimulateMouseEvent(embedder_contents,
                              blink::WebInputEvent::Type::kMouseMove,
                              point_in_parent);
  content::SimulateMouseEvent(
      embedder_contents, blink::WebInputEvent::Type::kMouseMove, point_in_pdf);
  content::SimulateMouseEvent(embedder_contents,
                              blink::WebInputEvent::Type::kMouseMove,
                              point_in_parent);

  // Verify MouseEnter, MouseLeave received.
  int leave_count = 0;
  do {
    ASSERT_TRUE(ExecuteScriptAndExtractInt(
        guest_contents, "window.domAutomationController.send(leave_count);",
        &leave_count));
  } while (!leave_count);
  int enter_count = 0;
  ASSERT_TRUE(ExecuteScriptAndExtractInt(
      guest_contents, "window.domAutomationController.send(enter_count);",
      &enter_count));
  EXPECT_EQ(1, enter_count);
}

IN_PROC_BROWSER_TEST_P(PDFExtensionHitTestTest, ContextMenuCoordinates) {
  // Load page with embedded PDF and make sure it succeeds.
  WebContents* guest_contents = LoadPdfGetGuestContents(
      embedded_test_server()->GetURL("/pdf/pdf_embed.html"));
  ASSERT_TRUE(guest_contents);

  // Observe context menu IPC.
  content::RenderFrameHost* plugin_frame = GetPluginFrame(guest_contents);
  content::ContextMenuInterceptor context_menu_interceptor(plugin_frame);

  ContextMenuWaiter menu_observer;

  // Send mouse right-click to activate context menu.
  gfx::Point context_menu_position(80, 130);
  content::SimulateMouseClickAt(GetActiveWebContents(), kDefaultKeyModifier,
                                blink::WebMouseEvent::Button::kRight,
                                context_menu_position);

  // We expect the context menu, invoked via the RenderFrameHost, to be using
  // root view coordinates.
  menu_observer.WaitForMenuOpenAndClose();
  ASSERT_EQ(context_menu_position.x(), menu_observer.params().x);
  ASSERT_EQ(context_menu_position.y(), menu_observer.params().y);

  // We expect the IPC, received from the renderer, to be using local coords.
  context_menu_interceptor.Wait();
  blink::UntrustworthyContextMenuParams params =
      context_menu_interceptor.get_params();
  gfx::Point received_context_menu_position =
      plugin_frame->GetRenderWidgetHost()
          ->GetView()
          ->TransformPointToRootCoordSpace({params.x, params.y});
  EXPECT_EQ(context_menu_position, received_context_menu_position);

  // TODO(wjmaclean): If it ever becomes possible to filter outgoing IPCs from
  // the RenderProcessHost, we should verify the blink.mojom.PluginActionAt
  // message is sent with the same coordinates as in the
  // UntrustworthyContextMenuParams.
}

// The plugin document and the mime handler should both use the same background
// color.
IN_PROC_BROWSER_TEST_P(PDFExtensionTest, BackgroundColor) {
  // The background color for plugins is injected when the first response
  // is intercepted, at which point not all the plugins have loaded. This line
  // ensures that the PDF plugin has loaded and the right background color is
  // beign used.
  WaitForPluginServiceToLoad();
  WebContents* guest_contents =
      LoadPdfGetGuestContents(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest_contents);
  const std::string script =
      "window.domAutomationController.send("
      "    window.getComputedStyle(document.body, null)."
      "    getPropertyValue('background-color'))";
  std::string outer;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(GetActiveWebContents(),
                                                     script, &outer));
  std::string inner;
  ASSERT_TRUE(
      content::ExecuteScriptAndExtractString(guest_contents, script, &inner));
  EXPECT_EQ(inner, outer);
}

IN_PROC_BROWSER_TEST_P(PDFExtensionTest, DefaultFocusForEmbeddedPDF) {
  WebContents* guest_contents = LoadPdfGetGuestContents(
      embedded_test_server()->GetURL("/pdf/pdf_embed.html"));
  ASSERT_TRUE(guest_contents);

  // Verify that current focus state is body element.
  const std::string script =
      "const is_plugin_focused = document.activeElement === "
      "document.body;"
      "window.domAutomationController.send(is_plugin_focused);";

  bool result = false;
  ASSERT_TRUE(
      content::ExecuteScriptAndExtractBool(guest_contents, script, &result));
  ASSERT_TRUE(result);
}

IN_PROC_BROWSER_TEST_P(PDFExtensionTest, DefaultFocusForNonEmbeddedPDF) {
  WebContents* guest_contents =
      LoadPdfGetGuestContents(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest_contents);

  // Verify that current focus state is document element.
  const std::string script =
      "const is_plugin_focused = document.activeElement === "
      "document.body;"
      "window.domAutomationController.send(is_plugin_focused);";

  bool result = false;
  ASSERT_TRUE(
      content::ExecuteScriptAndExtractBool(guest_contents, script, &result));
  ASSERT_TRUE(result);
}

// A helper for waiting for the first request for |url_to_intercept|.
class RequestWaiter {
 public:
  // Start intercepting requests to |url_to_intercept|.
  explicit RequestWaiter(const GURL& url_to_intercept)
      : url_to_intercept_(url_to_intercept),
        interceptor_(base::BindRepeating(&RequestWaiter::InterceptorCallback,
                                         base::Unretained(this))) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(url_to_intercept.is_valid());
  }

  RequestWaiter(const RequestWaiter&) = delete;
  RequestWaiter& operator=(const RequestWaiter&) = delete;

  void WaitForRequest() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!IsAlreadyIntercepted())
      run_loop_.Run();
    DCHECK(IsAlreadyIntercepted());
  }

 private:
  bool InterceptorCallback(
      content::URLLoaderInterceptor::RequestParams* params) {
    // This method may be called either on the IO or UI thread.
    DCHECK(params);

    base::AutoLock lock(lock_);
    if (url_to_intercept_ != params->url_request.url || already_intercepted_)
      return false;

    already_intercepted_ = true;
    run_loop_.Quit();
    return false;
  }

  bool IsAlreadyIntercepted() {
    base::AutoLock lock(lock_);
    return already_intercepted_;
  }

  const GURL url_to_intercept_;
  content::URLLoaderInterceptor interceptor_;
  base::RunLoop run_loop_;

  base::Lock lock_;
  bool already_intercepted_ GUARDED_BY(lock_) = false;
};

// This is a regression test for a problem where DidStopLoading didn't get
// propagated from a remote frame into the main frame.  See also
// https://crbug.com/964364.
IN_PROC_BROWSER_TEST_P(PDFExtensionTest, DidStopLoading) {
  // Prepare to wait for requests for the main page of the MimeHandlerView for
  // PDFs.
  RequestWaiter interceptor(
      GURL("chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html"));

  // Navigate to a page with:
  //   <embed type="application/pdf" src="test.pdf"></embed>
  //   <iframe src="/hung"></iframe>
  // Afterwards, the main page should be still loading because of the hung
  // subframe (but the subframe for the OOPIF-based PDF MimeHandlerView might or
  // might not be created at this point).
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      embedded_test_server()->GetURL(
          "/pdf/pdf_embed_with_hung_sibling_subframe.html"),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);  // Don't wait for completion.

  // Wait for the request for the MimeHandlerView extension.  Afterwards, the
  // main page should be still loading because of
  // 1) the MimeHandlerView frame is loading
  // 2) the hung subframe is loading.
  interceptor.WaitForRequest();

  // Remove the hung subframe.  Afterwards the main page should stop loading as
  // soon as the MimeHandlerView frame stops loading (assumming we have not bugs
  // similar to https://crbug.com/964364).
  WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(content::ExecJs(
      web_contents, "document.getElementById('hung_subframe').remove();"));

  // MAIN VERIFICATION: Wait for the main frame to report that is has stopped
  // loading.
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
}

// This test verifies that it is possible to add an <embed src=pdf> element into
// a new popup window when using document.write.  See also
// https://crbug.com/1041880.
IN_PROC_BROWSER_TEST_P(PDFExtensionTest, DocumentWriteIntoNewPopup) {
  // Navigate to an empty/boring test page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  // Open a new popup and call document.write to add an embedded PDF.
  WebContents* popup = nullptr;
  {
    const char kScriptTemplate[] = R"(
        const url = $1;
        const html = '<embed type="application/pdf" src="' + url + '">';

        const popup = window.open('', '_blank');
        popup.document.write(html);
    )";
    content::WebContentsAddedObserver popup_observer;
    ASSERT_TRUE(content::ExecJs(
        GetActiveWebContents(),
        content::JsReplace(kScriptTemplate,
                           embedded_test_server()->GetURL("/pdf/test.pdf"))));
    popup = popup_observer.GetWebContents();
  }

  // Verify the PDF loaded successfully.
  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(popup));
}

// Tests that the PDF extension loads in the presence of an extension that, on
// the completion of document loading, adds an <iframe> to the body element.
// https://bugs.chromium.org/p/chromium/issues/detail?id=1046795
IN_PROC_BROWSER_TEST_P(PDFExtensionTest,
                       PdfLoadsWithExtensionThatInjectsFrame) {
  // Load the test extension.
  const extensions::Extension* test_extension = LoadExtension(
      GetTestResourcesParentDir().AppendASCII("pdf/extension_injects_iframe"));
  ASSERT_TRUE(test_extension);

  // Load the PDF. The call to LoadPdf() will return false if the pdf extension
  // fails to load.
  ASSERT_TRUE(LoadPdf(embedded_test_server()->GetURL("/pdf/test.pdf")));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionTest, Metrics) {
  base::HistogramTester histograms;
  base::UserActionTester actions;

  WebContents* guest_contents = LoadPdfGetGuestContents(
      embedded_test_server()->GetURL("/pdf/combobox_form.pdf"));
  ASSERT_TRUE(guest_contents);

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // Histograms.
  // Duplicating some constants to avoid reaching into pdf/ internals.
  constexpr int kAcroForm = 1;
  constexpr int k1_7 = 8;
  histograms.ExpectUniqueSample("PDF.FormType", kAcroForm, 1);
  histograms.ExpectUniqueSample("PDF.Version", k1_7, 1);
  histograms.ExpectUniqueSample("PDF.IsTagged", 0, 1);
  histograms.ExpectUniqueSample("PDF.HasAttachment", 0, 1);

  // Custom histograms.
  histograms.ExpectUniqueSample("PDF.PageCount", 1, 1);

  // User actions.
  EXPECT_EQ(1, actions.GetActionCount("PDF.LoadSuccess"));
}

// Flaky. See https://crbug.com/1101514.
IN_PROC_BROWSER_TEST_P(PDFExtensionTest, DISABLED_TabInAndOutOfPDFPlugin) {
  WebContents* guest_contents =
      LoadPdfGetGuestContents(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest_contents);

  // Set focus on last toolbar element (zoom-out-button).
  ASSERT_TRUE(
      content::ExecuteScript(guest_contents,
                             R"(viewer.shadowRoot.querySelector('#zoom-toolbar')
         .$['zoom-out-button']
         .$$('cr-icon-button')
         .focus();)"));

  // The script will ensure we return the the focused element on focus.
  const char kScript[] = R"(
    const plugin = viewer.shadowRoot.querySelector('#plugin');
    plugin.addEventListener('focus', () => {
      window.domAutomationController.send('plugin');
    });

    const button = viewer.shadowRoot.querySelector('#zoom-toolbar')
                   .$['zoom-out-button']
                   .$$('cr-icon-button');
    button.addEventListener('focus', () => {
      window.domAutomationController.send('zoom-out-button');
    });
  )";
  ASSERT_TRUE(content::ExecuteScript(guest_contents, kScript));

  // Helper to simulate a tab press and wait for a focus message.
  auto press_tab_and_wait_for_message = [guest_contents](bool reverse) {
    content::DOMMessageQueue msg_queue;
    std::string reply;
    SimulateKeyPress(guest_contents, ui::DomKey::TAB, ui::DomCode::TAB,
                     ui::VKEY_TAB, false, /*shift=*/reverse, false, false);
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    return reply;
  };

  // Press <tab> and ensure that PDF document receives focus.
  EXPECT_EQ("\"plugin\"", press_tab_and_wait_for_message(false));
  // Press <shift-tab> and ensure that last toolbar element (zoom-out-button)
  // receives focus.
  EXPECT_EQ("\"zoom-out-button\"", press_tab_and_wait_for_message(true));
}

// This test suite does a simple text-extraction based on the accessibility
// internals, breaking lines & paragraphs where appropriate.  Unlike
// TreeDumpTests, this allows us to verify the kNextOnLine and kPreviousOnLine
// relationships.
class PDFExtensionAccessibilityTextExtractionTest : public PDFExtensionTest {
 public:
  PDFExtensionAccessibilityTextExtractionTest() = default;
  ~PDFExtensionAccessibilityTextExtractionTest() override = default;

  void RunTextExtractionTest(const base::FilePath::CharType* pdf_file) {
    base::FilePath test_path = ui_test_utils::GetTestFilePath(
        base::FilePath(FILE_PATH_LITERAL("pdf")),
        base::FilePath(FILE_PATH_LITERAL("accessibility")));
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
    }
    base::FilePath pdf_path = test_path.Append(pdf_file);

    RunTest(pdf_path, "pdf/accessibility");
  }

 protected:
  std::vector<base::Feature> GetEnabledFeatures() const override {
    auto enabled = PDFExtensionTest::GetEnabledFeatures();
    enabled.push_back(chrome_pdf::features::kAccessiblePDFForm);
    return enabled;
  }

 private:
  void RunTest(const base::FilePath& test_file_path, const char* file_dir) {
    // Load the expectation file.
    content::DumpAccessibilityTestHelper test_helper("content");
    absl::optional<base::FilePath> expected_file_path =
        test_helper.GetExpectationFilePath(test_file_path);
    ASSERT_TRUE(expected_file_path) << "No expectation file present.";

    absl::optional<std::vector<std::string>> expected_lines =
        test_helper.LoadExpectationFile(*expected_file_path);
    ASSERT_TRUE(expected_lines) << "Couldn't load expectation file.";

    // Enable accessibility and load the test file.
    content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
    WebContents* guest_contents =
        LoadPdfGetGuestContents(embedded_test_server()->GetURL(
            "/" + std::string(file_dir) + "/" +
            test_file_path.BaseName().MaybeAsASCII()));
    ASSERT_TRUE(guest_contents);
    WaitForAccessibilityTreeToContainNodeWithName(guest_contents, "Page 1");

    // Extract the text content.
    ui::AXTreeUpdate ax_tree =
        GetAccessibilityTreeSnapshotForPdf(guest_contents);
    auto actual_lines = CollectLines(ax_tree);

    // Aborts if CollectLines() had a failure.
    if (HasFailure())
      return;

    // Validate the dump against the expectation file.
    EXPECT_TRUE(test_helper.ValidateAgainstExpectation(
        test_file_path, *expected_file_path, actual_lines, *expected_lines));
  }

 private:
  std::vector<std::string> CollectLines(
      const ui::AXTreeUpdate& ax_tree_update) {
    std::vector<std::string> lines;

    ui::AXTree tree(ax_tree_update);
    std::vector<ui::AXNode*> pdf_root_objs;
    FindAXNodes(tree.root(), {ax::mojom::Role::kPdfRoot}, &pdf_root_objs);
    // Can't use ASSERT_EQ because CollectLines doesn't return void.
    if (pdf_root_objs.size() != 1u) {
      // Add a non-fatal failure here to make the test fail.
      ADD_FAILURE() << "Multiple PDF root nodes in the tree.";
      return {};
    }
    ui::AXNode* pdf_doc_root = pdf_root_objs[0];

    std::vector<ui::AXNode*> text_nodes;
    FindAXNodes(pdf_doc_root,
                {ax::mojom::Role::kStaticText, ax::mojom::Role::kInlineTextBox},
                &text_nodes);

    int previous_node_id = 0;
    int previous_node_next_id = 0;
    std::string line;
    for (ui::AXNode* node : text_nodes) {
      // StaticText begins a new paragraph.
      if (node->GetRole() == ax::mojom::Role::kStaticText && !line.empty()) {
        lines.push_back(line);
        lines.push_back("\u00b6");  // pilcrow/paragraph mark, Alt+0182
        line.clear();
      }

      // We collect all inline text boxes within the paragraph.
      if (node->GetRole() != ax::mojom::Role::kInlineTextBox)
        continue;

      std::string name =
          node->GetStringAttribute(ax::mojom::StringAttribute::kName);
      base::StringPiece trimmed_name =
          base::TrimString(name, "\r\n", base::TRIM_TRAILING);
      int prev_id =
          node->GetIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId);
      if (previous_node_next_id == node->id()) {
        // Previous node pointed to us, so we are part of the same line.
        EXPECT_EQ(previous_node_id, prev_id)
            << "Expect this node to point to previous node.";
        line.append(trimmed_name.data(), trimmed_name.size());
      } else {
        // Not linked with the previous node; this is a new line.
        EXPECT_EQ(previous_node_next_id, 0)
            << "Previous node pointed to something unexpected.";
        EXPECT_EQ(prev_id, 0)
            << "Our back pointer points to something unexpected.";
        if (!line.empty())
          lines.push_back(line);
        line = std::string(trimmed_name);
      }

      previous_node_id = node->id();
      previous_node_next_id =
          node->GetIntAttribute(ax::mojom::IntAttribute::kNextOnLineId);
    }
    if (!line.empty())
      lines.push_back(line);

    // Extra newline to match current expectations. TODO: get rid of this
    // and rebase the expectations files.
    if (!lines.empty())
      lines.push_back("\u00b6");  // pilcrow/paragraph mark, Alt+0182

    return lines;
  }

  // Searches recursively through |current| and all descendants and
  // populates a vector with all nodes that match any of the roles
  // in |roles|.
  void FindAXNodes(ui::AXNode* current,
                   const base::flat_set<ax::mojom::Role> roles,
                   std::vector<ui::AXNode*>* results) {
    if (base::Contains(roles, current->GetRole()))
      results->push_back(current);

    for (ui::AXNode* child : current->children())
      FindAXNodes(child, roles, results);
  }
};

// Test that Previous/NextOnLineId attributes are present and properly linked on
// InlineTextBoxes within a line.
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTextExtractionTest,
                       NextOnLine) {
  RunTextExtractionTest(FILE_PATH_LITERAL("next-on-line.pdf"));
}

// Test that a drop-cap is grouped with the correct line.
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTextExtractionTest, DropCap) {
  RunTextExtractionTest(FILE_PATH_LITERAL("drop-cap.pdf"));
}

// Test that simulated superscripts and subscripts don't cause a line break.
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTextExtractionTest,
                       SuperscriptSubscript) {
  RunTextExtractionTest(FILE_PATH_LITERAL("superscript-subscript.pdf"));
}

// Test that simple font and font-size changes in the middle of a line don't
// cause line breaks.
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTextExtractionTest,
                       FontChange) {
  RunTextExtractionTest(FILE_PATH_LITERAL("font-change.pdf"));
}

// Test one property of pdf_private/accessibility_crash_2.pdf, where a page has
// only whitespace characters.
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTextExtractionTest,
                       OnlyWhitespaceText) {
  RunTextExtractionTest(FILE_PATH_LITERAL("whitespace.pdf"));
}

// Test data of inline text boxes for PDF with weblinks.
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTextExtractionTest, WebLinks) {
  RunTextExtractionTest(FILE_PATH_LITERAL("weblinks.pdf"));
}

// Test data of inline text boxes for PDF with highlights.
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTextExtractionTest,
                       Highlights) {
  RunTextExtractionTest(FILE_PATH_LITERAL("highlights.pdf"));
}

// Test data of inline text boxes for PDF with text fields.
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTextExtractionTest,
                       TextFields) {
  RunTextExtractionTest(FILE_PATH_LITERAL("text_fields.pdf"));
}

// Test data of inline text boxes for PDF with multi-line and various font-sized
// text.
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTextExtractionTest,
                       ParagraphsAndHeadingUntagged) {
  RunTextExtractionTest(
      FILE_PATH_LITERAL("paragraphs-and-heading-untagged.pdf"));
}

// Test data of inline text boxes for PDF with text, weblinks, images and
// annotation links.
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTextExtractionTest,
                       LinksImagesAndText) {
  RunTextExtractionTest(FILE_PATH_LITERAL("text-image-link.pdf"));
}

// Test data of inline text boxes for PDF with overlapping annotations.
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTextExtractionTest,
                       OverlappingAnnots) {
  RunTextExtractionTest(FILE_PATH_LITERAL("overlapping-annots.pdf"));
}

class PDFExtensionAccessibilityTreeDumpTest
    : public PDFExtensionTestWithoutUnseasonedOverride,
      public ::testing::WithParamInterface<
          std::tuple<ui::AXApiType::Type, bool>> {
 public:
  PDFExtensionAccessibilityTreeDumpTest() : test_helper_(ax_inspect_type()) {}
  ~PDFExtensionAccessibilityTreeDumpTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PDFExtensionTestWithoutUnseasonedOverride::SetUpCommandLine(command_line);

    // Each test pass might require custom command-line setup
    test_helper_.SetUpCommandLine(command_line);
  }

 protected:
  ui::AXApiType::Type ax_inspect_type() const {
    return std::get<0>(GetParam());
  }
  bool should_enable_unseasoned() const { return std::get<1>(GetParam()); }

  std::vector<base::Feature> GetEnabledFeatures() const override {
    auto enabled =
        PDFExtensionTestWithoutUnseasonedOverride::GetEnabledFeatures();
    enabled.push_back(chrome_pdf::features::kAccessiblePDFForm);
    if (should_enable_unseasoned())
      enabled.push_back(chrome_pdf::features::kPdfUnseasoned);
    return enabled;
  }

  std::vector<base::Feature> GetDisabledFeatures() const override {
    auto disabled =
        PDFExtensionTestWithoutUnseasonedOverride::GetDisabledFeatures();
    if (!should_enable_unseasoned())
      disabled.push_back(chrome_pdf::features::kPdfUnseasoned);
    return disabled;
  }

  void RunPDFTest(const base::FilePath::CharType* pdf_file) {
    base::FilePath test_path = ui_test_utils::GetTestFilePath(
        base::FilePath(FILE_PATH_LITERAL("pdf")),
        base::FilePath(FILE_PATH_LITERAL("accessibility")));
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
    }
    base::FilePath pdf_path = test_path.Append(pdf_file);

    RunTest(pdf_path, "pdf/accessibility");
  }

 private:
  using AXPropertyFilter = ui::AXPropertyFilter;

  //  See chrome/test/data/pdf/accessibility/readme.md for more info.
  ui::AXInspectScenario ParsePdfForExtraDirectives(
      const std::string& pdf_contents) {
    const char kCommentMark = '%';

    std::vector<std::string> lines;
    for (const std::string& line : base::SplitString(
             pdf_contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      if (line.size() > 1 && line[0] == kCommentMark) {
        // Remove first character since it's the comment mark.
        lines.push_back(line.substr(1));
      }
    }

    return test_helper_.ParseScenario(lines, DefaultFilters());
  }

  void RunTest(const base::FilePath& test_file_path, const char* file_dir) {
    std::string pdf_contents;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::ReadFileToString(test_file_path, &pdf_contents));
    }

    // Set up the tree formatter. Parse filters and other directives in the test
    // file.
    ui::AXInspectScenario scenario = ParsePdfForExtraDirectives(pdf_contents);

    std::unique_ptr<AXTreeFormatter> formatter =
        AXInspectFactory::CreateFormatter(ax_inspect_type());
    formatter->SetPropertyFilters(scenario.property_filters,
                                  AXTreeFormatter::kFiltersDefaultSet);

    // Exit without running the test if we can't find an expectation file or if
    // the expectation file contains a skip marker.
    // This is used to skip certain tests on certain platforms.
    base::FilePath expected_file_path =
        test_helper_.GetExpectationFilePath(test_file_path);
    if (expected_file_path.empty()) {
      LOG(INFO) << "No expectation file present, ignoring test on this "
                   "platform.";
      return;
    }

    absl::optional<std::vector<std::string>> expected_lines =
        test_helper_.LoadExpectationFile(expected_file_path);
    if (!expected_lines) {
      LOG(INFO) << "Skipping this test on this platform.";
      return;
    }

    // Enable accessibility and load the test file.
    content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
    WebContents* guest_contents =
        LoadPdfGetGuestContents(embedded_test_server()->GetURL(
            "/" + std::string(file_dir) + "/" +
            test_file_path.BaseName().MaybeAsASCII()));
    ASSERT_TRUE(guest_contents);
    WaitForAccessibilityTreeToContainNodeWithName(guest_contents, "Page 1");

    // Find the embedded PDF and dump the accessibility tree.
    content::FindAccessibilityNodeCriteria find_criteria;
    find_criteria.role = ax::mojom::Role::kPdfRoot;
    ui::AXPlatformNodeDelegate* pdf_root =
        content::FindAccessibilityNode(guest_contents, find_criteria);
    ASSERT_TRUE(pdf_root);

    std::string actual_contents = formatter->Format(pdf_root);

    std::vector<std::string> actual_lines =
        base::SplitString(actual_contents, "\n", base::KEEP_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);

    // Validate the dump against the expectation file.
    EXPECT_TRUE(test_helper_.ValidateAgainstExpectation(
        test_file_path, expected_file_path, actual_lines, *expected_lines));
  }

  std::vector<AXPropertyFilter> DefaultFilters() const {
    std::vector<AXPropertyFilter> property_filters;

    property_filters.emplace_back("value='*'", AXPropertyFilter::ALLOW);
    // The value attribute on the document object contains the URL of the
    // current page which will not be the same every time the test is run.
    // The PDF plugin uses the 'chrome-extension' protocol, so block that as
    // well.
    property_filters.emplace_back("value='http*'", AXPropertyFilter::DENY);
    property_filters.emplace_back("value='chrome-extension*'",
                                  AXPropertyFilter::DENY);
    // Object attributes.value
    property_filters.emplace_back("layout-guess:*", AXPropertyFilter::ALLOW);

    property_filters.emplace_back("select*", AXPropertyFilter::ALLOW);
    property_filters.emplace_back("descript*", AXPropertyFilter::ALLOW);
    property_filters.emplace_back("check*", AXPropertyFilter::ALLOW);
    property_filters.emplace_back("horizontal", AXPropertyFilter::ALLOW);
    property_filters.emplace_back("multiselectable", AXPropertyFilter::ALLOW);
    property_filters.emplace_back("isPageBreakingObject*",
                                  AXPropertyFilter::ALLOW);

    // Deny most empty values
    property_filters.emplace_back("*=''", AXPropertyFilter::DENY);
    // After denying empty values, because we want to allow name=''
    property_filters.emplace_back("name=*", AXPropertyFilter::ALLOW_EMPTY);

    return property_filters;
  }

  content::DumpAccessibilityTestHelper test_helper_;
};

// Parameterize the tests so that each test-pass is run independently.
struct DumpAccessibilityTreeTestPassToString {
  std::string operator()(
      const ::testing::TestParamInfo<std::tuple<ui::AXApiType::Type, bool>>& i)
      const {
    return (std::get<1>(i.param) ? "Unseasoned_" : "") +
           std::string(std::get<0>(i.param));
  }
};

// Constructs a list of accessibility tests, one for each accessibility tree
// formatter testpasses.
const std::vector<ui::AXApiType::Type> GetAXTestValues() {
  std::vector<ui::AXApiType::Type> passes =
      content::DumpAccessibilityTestHelper::TreeTestPasses();
  return passes;
}

INSTANTIATE_TEST_SUITE_P(All,
                         PDFExtensionAccessibilityTreeDumpTest,
                         testing::Combine(testing::ValuesIn(GetAXTestValues()),
                                          testing::Bool()),
                         DumpAccessibilityTreeTestPassToString());

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, HelloWorld) {
  RunPDFTest(FILE_PATH_LITERAL("hello-world.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest,
                       ParagraphsAndHeadingUntagged) {
  RunPDFTest(FILE_PATH_LITERAL("paragraphs-and-heading-untagged.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, MultiPage) {
  RunPDFTest(FILE_PATH_LITERAL("multi-page.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest,
                       DirectionalTextRuns) {
  RunPDFTest(FILE_PATH_LITERAL("directional-text-runs.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, TextDirection) {
  RunPDFTest(FILE_PATH_LITERAL("text-direction.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, WebLinks) {
  RunPDFTest(FILE_PATH_LITERAL("weblinks.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest,
                       OverlappingLinks) {
  RunPDFTest(FILE_PATH_LITERAL("overlapping-links.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, Highlights) {
  RunPDFTest(FILE_PATH_LITERAL("highlights.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, TextFields) {
  RunPDFTest(FILE_PATH_LITERAL("text_fields.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, Images) {
  RunPDFTest(FILE_PATH_LITERAL("image_alt_text.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest,
                       LinksImagesAndText) {
  RunPDFTest(FILE_PATH_LITERAL("text-image-link.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest,
                       TextRunStyleHeuristic) {
  RunPDFTest(FILE_PATH_LITERAL("text-run-style-heuristic.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, TextStyle) {
  RunPDFTest(FILE_PATH_LITERAL("text-style.pdf"));
}

// TODO(https://crbug.com/1172026)
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, XfaFields) {
  RunPDFTest(FILE_PATH_LITERAL("xfa_fields.pdf"));
}

// This test suite validates the navigation done using the accessibility client.
using PDFExtensionAccessibilityNavigationTest = PDFExtensionTest;

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityNavigationTest,
                       LinkNavigation) {
  // Enable accessibility and load the test file.
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
  WebContents* guest_contents = LoadPdfGetGuestContents(
      embedded_test_server()->GetURL("/pdf/accessibility/weblinks.pdf"));
  ASSERT_TRUE(guest_contents);
  WaitForAccessibilityTreeToContainNodeWithName(guest_contents, "Page 1");

  // Find the specific link node.
  content::FindAccessibilityNodeCriteria find_criteria;
  find_criteria.role = ax::mojom::Role::kLink;
  find_criteria.name = "http://bing.com";
  ui::AXPlatformNodeDelegate* link_node =
      content::FindAccessibilityNode(guest_contents, find_criteria);
  ASSERT_TRUE(link_node);

  // Invoke action on a link and wait for navigation to complete.
  content::AccessibilityNotificationWaiter event_waiter(
      GetActiveWebContents(), ui::kAXModeComplete,
      ax::mojom::Event::kLoadComplete);
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kDoDefault;
  action_data.target_node_id = link_node->GetData().id;
  link_node->AccessibilityPerformAction(action_data);
  event_waiter.WaitForNotification();

  // Test that navigation occurred correctly.
  const GURL& expected_url = GetActiveWebContents()->GetLastCommittedURL();
  EXPECT_EQ("https://bing.com/", expected_url.spec());
}

class PDFExtensionPrerenderTest : public PDFExtensionTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PDFExtensionTest::SetUpCommandLine(command_line);
    // |prerender_helper_| has a ScopedFeatureList so we needed to delay its
    // creation until now because PDFExtensionTest also uses a ScopedFeatureList
    // and initialization order matters.
    prerender_helper_ = std::make_unique<content::test::PrerenderTestHelper>(
        base::BindRepeating(&PDFExtensionPrerenderTest::GetActiveWebContents,
                            base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    prerender_helper_->SetUp(embedded_test_server());
    PDFExtensionTest::SetUpOnMainThread();
  }

 protected:
  content::test::PrerenderTestHelper& prerender_helper() {
    return *prerender_helper_;
  }

 private:
  std::unique_ptr<content::test::PrerenderTestHelper> prerender_helper_;
};

// TODO(1206312, 1205920): As of writing this test, we can attempt to prerender
// the PDF viewer without crashing, however the viewer itself fails to load a
// PDF. This test should be extended once that works.
IN_PROC_BROWSER_TEST_P(PDFExtensionPrerenderTest,
                       LoadPdfWhilePrerenderedDoesNotCrash) {
  const GURL initial_url =
      embedded_test_server()->GetURL("a.test", "/empty.html");
  const GURL pdf_url =
      embedded_test_server()->GetURL("a.test", "/pdf/test.pdf");
  WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  const int host_id = prerender_helper().AddPrerender(pdf_url);
  content::RenderFrameHost* prerendered_render_frame_host =
      prerender_helper().GetPrerenderedMainFrameHost(host_id);
  ASSERT_TRUE(prerendered_render_frame_host);
  ASSERT_EQ(web_contents->GetLastCommittedURL(), initial_url);

  prerender_helper().NavigatePrimaryPage(pdf_url);
  ASSERT_EQ(web_contents->GetLastCommittedURL(), pdf_url);
}

class PDFExtensionSubmitFormTest : public PDFExtensionTest {
 public:
  void SetUpOnMainThread() override {
    embedded_test_server()->RegisterRequestMonitor(base::BindLambdaForTesting(
        [this](const net::test_server::HttpRequest& request) {
          if (request.relative_url != "/pdf/test_endpoint")
            return;

          EXPECT_EQ(request.method, net::test_server::METHOD_POST);
          EXPECT_THAT(request.content, StartsWith("\%FDF"));
          ASSERT_TRUE(quit_closure_);
          std::move(quit_closure_).Run();
        }));

    PDFExtensionTest::SetUpOnMainThread();
  }

 protected:
  // Retrieves a `base::RunLoop` and saves its `QuitClosure()`. The test
  // monitors HTTP requests on the IO thread, so `quit_closure_` needs to be set
  // up on the UI thread before the requests can arrive.
  std::unique_ptr<base::RunLoop> CreateFormSubmissionRunLoop() {
    auto run_loop = std::make_unique<base::RunLoop>();
    EXPECT_FALSE(quit_closure_);
    quit_closure_ = run_loop->QuitClosure();
    return run_loop;
  }

 private:
  base::OnceClosure quit_closure_;
};

// TODO(crbug.com/1259994): Fix Windows 7 flakes.
#if defined(OS_WIN)
#define MAYBE_SubmitForm DISABLED_SubmitForm
#else
#define MAYBE_SubmitForm SubmitForm
#endif
IN_PROC_BROWSER_TEST_P(PDFExtensionSubmitFormTest, MAYBE_SubmitForm) {
  WebContents* guest_contents = LoadPdfGetGuestContents(
      embedded_test_server()->GetURL("/pdf/submit_form.pdf"));
  ASSERT_TRUE(guest_contents);

  std::unique_ptr<base::RunLoop> run_loop = CreateFormSubmissionRunLoop();

  // Click on the "Submit Form" button.
  content::SimulateMouseClickAt(
      guest_contents, blink::WebInputEvent::kNoModifiers,
      blink::WebMouseEvent::Button::kLeft,
      ConvertPageCoordToScreenCoord(guest_contents, {200, 200}));

  run_loop->Run();
}

// TODO(crbug.com/702993): Stop testing both modes after unseasoned launches.
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionTestWithPartialLoading);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    PDFExtensionTestWithTestGuestViewManager);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionBlobNavigationTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFPluginDisabledTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionJSTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionContentSettingJSTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionWebUICodeCacheJSTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionServiceWorkerJSTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionRegionSearchTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionLinkClickTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionInternalLinkClickTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionSaveTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionSaveWithPolicyTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionClipboardTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    PDFExtensionAccessibilityTextExtractionTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionHitTestTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    PDFExtensionAccessibilityNavigationTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionPrerenderTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionSubmitFormTest);
