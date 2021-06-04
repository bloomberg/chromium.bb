// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/commit_message_delayer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/content_script_tracker.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

// Asks the |extension_id| to inject |content_script| into |web_contents| and
// waits until the script reports that it has finished executing.
void ExecuteProgrammaticContentScript(content::WebContents* web_contents,
                                      const ExtensionId& extension_id,
                                      const std::string& content_script) {
  // Build a script that executes the original `content_script` and then sends
  // an ack via `domAutomationController.send`.
  const char kAckingScriptTemplate[] = R"(
      %s;
      domAutomationController.send("Hello from acking script!");
  )";
  std::string acking_script =
      base::StringPrintf(kAckingScriptTemplate, content_script.c_str());

  // Build a script to execute in the extension's background page.
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);
  std::string background_script = content::JsReplace(
      "chrome.tabs.executeScript($1, { code: $2 });", tab_id, acking_script);

  // Inject the script and wait for the ack.
  //
  // Note that using ExtensionTestMessageListener / `chrome.test.sendMessage`
  // (instead of DOMMessageQueue / `domAutomationController.send`) would have
  // hung in the ProgrammaticInjectionRacingWithDidCommit testcase.  The root
  // cause is not 100% understood, but it might be because the IPC related to
  // `chrome.test.sendMessage` can't be dispatched while running a nested
  // message loop while handling a DidCommit IPC.
  content::DOMMessageQueue message_queue;
  ASSERT_TRUE(browsertest_util::ExecuteScriptInBackgroundPageNoWait(
      web_contents->GetBrowserContext(), extension_id, background_script));
  std::string msg;
  EXPECT_TRUE(message_queue.WaitForMessage(&msg));
  EXPECT_EQ("\"Hello from acking script!\"", msg);
}

// Test suite covering `extensions::ContentScriptTracker` from
// //extensions/browser/content_script_tracker.h.
//
// See also ContentScriptMatchingBrowserTest in
// //extensions/browser/content_script_matching_browsertest.cc.
class ContentScriptTrackerBrowserTest : public ExtensionBrowserTest {
 public:
  ContentScriptTrackerBrowserTest() = default;

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Helper class for executing a content script right before handling a DidCommit
// IPC.
class ContentScriptExecuterBeforeDidCommit {
 public:
  ContentScriptExecuterBeforeDidCommit(const GURL& postponed_commit_url,
                                       content::WebContents* web_contents,
                                       const ExtensionId& extension_id,
                                       const std::string& content_script)
      : commit_delayer_(
            web_contents,
            postponed_commit_url,
            base::BindOnce(
                &ContentScriptExecuterBeforeDidCommit::ExecuteContentScript,
                base::Unretained(web_contents),
                extension_id,
                content_script)) {}

 private:
  static void ExecuteContentScript(content::WebContents* web_contents,
                                   const ExtensionId& extension_id,
                                   const std::string& content_script,
                                   content::RenderFrameHost* ignored) {
    ExecuteProgrammaticContentScript(web_contents, extension_id,
                                     content_script);
  }

  content::CommitMessageDelayer commit_delayer_;
};

// Tests tracking of content scripts injected/declared via
// `chrome.scripting.executeScript` API.  See also:
// https://developer.chrome.com/docs/extensions/mv3/content_scripts/#programmatic
IN_PROC_BROWSER_TEST_F(ContentScriptTrackerBrowserTest,
                       ProgrammaticContentScript) {
  // Install a test extension.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "ContentScriptTrackerBrowserTest - Programmatic",
        "version": "1.0",
        "manifest_version": 2,
        "permissions": [ "tabs", "<all_urls>" ],
        "background": {"scripts": ["background_script.js"]}
      } )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("background_script.js"), "");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to an arbitrary, mostly-empty test page.
  GURL page_url = embedded_test_server()->GetURL("foo.com", "/title1.html");
  ui_test_utils::NavigateToURL(browser(), page_url);

  // Verify that initially no processes show up as having been injected with
  // content scripts.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* background_frame =
      ProcessManager::Get(browser()->profile())
          ->GetBackgroundHostForExtension(extension->id())
          ->main_frame_host();
  EXPECT_EQ("This page has no title.",
            content::EvalJs(web_contents, "document.body.innerText"));
  EXPECT_FALSE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
      *web_contents->GetMainFrame()->GetProcess(), extension->id()));
  EXPECT_FALSE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
      *background_frame->GetProcess(), extension->id()));

  // Programmatically inject a content script.
  const char kContentScript[] = R"(
      document.body.innerText = 'content script has run';
  )";
  ExecuteProgrammaticContentScript(web_contents, extension->id(),
                                   kContentScript);

  // Verify that the right processes show up as having been injected with
  // content scripts.
  EXPECT_EQ("content script has run",
            content::EvalJs(web_contents, "document.body.innerText"));
  EXPECT_TRUE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
      *web_contents->GetMainFrame()->GetProcess(), extension->id()));
  EXPECT_FALSE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
      *background_frame->GetProcess(), extension->id()));

  // Navigate to a different same-site document and verify if
  // ContentScriptTracker still thinks that content scripts have been injected.
  //
  // DidProcessRunContentScriptFromExtension is expected to return true, because
  // content scripts have been injected into the renderer process in the *past*,
  // even though the *current* set of documents hosted in the renderer process
  // have not run a content script.
  GURL new_url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  ui_test_utils::NavigateToURL(browser(), new_url);
  EXPECT_EQ("This page has a title.",
            content::EvalJs(web_contents, "document.body.innerText"));
  EXPECT_TRUE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
      *web_contents->GetMainFrame()->GetProcess(), extension->id()));
  EXPECT_FALSE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
      *background_frame->GetProcess(), extension->id()));
}

// Tests what happens when the ExtensionMsg_ExecuteCode is sent *after* sending
// a Commit IPC to the renderer (i.e. after ReadyToCommit) but *before* a
// corresponding DidCommit IPC has been received by the browser process.  See
// also the "RenderDocumentHostUserData race w/ Commit IPC" section in the
// document here:
// https://docs.google.com/document/d/1MFprp2ss2r9RNamJ7Jxva1bvRZvec3rzGceDGoJ6vW0/edit#heading=h.n2ppjzx4jpzt
IN_PROC_BROWSER_TEST_F(ContentScriptTrackerBrowserTest,
                       ProgrammaticInjectionRacingWithDidCommit) {
  // Install a test extension.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "ContentScriptTrackerBrowserTest - DidCommit race",
        "version": "1.0",
        "manifest_version": 2,
        "permissions": [ "tabs", "<all_urls>" ],
        "background": {"scripts": ["background_script.js"]}
      } )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("background_script.js"), "");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to an arbitrary, mostly-empty test page.
  GURL page_url = embedded_test_server()->GetURL("foo.com", "/title1.html");
  ui_test_utils::NavigateToURL(browser(), page_url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Programmatically inject a content script between ReadyToCommit and
  // DidCommit events.
  {
    GURL new_url = embedded_test_server()->GetURL("foo.com", "/title2.html");
    ContentScriptExecuterBeforeDidCommit content_script_executer(
        new_url, web_contents, extension->id(),
        "document.body.innerText = 'content script has run'");
    ui_test_utils::NavigateToURL(browser(), new_url);
  }

  // Verify that the process shows up as having been injected with content
  // scripts.
  EXPECT_EQ("content script has run",
            content::EvalJs(web_contents, "document.body.innerText"));
  EXPECT_TRUE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
      *web_contents->GetMainFrame()->GetProcess(), extension->id()));
}

// Tests tracking of content scripts injected/declared via `content_scripts`
// entry in the extension manifest.  See also:
// https://developer.chrome.com/docs/extensions/mv3/content_scripts/#static-declarative
IN_PROC_BROWSER_TEST_F(ContentScriptTrackerBrowserTest,
                       ContentScriptDeclarationInExtensionManifest) {
  // Install a test extension.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "ContentScriptTrackerBrowserTest - Declarative",
        "version": "1.0",
        "manifest_version": 2,
        "permissions": [ "tabs", "<all_urls>" ],
        "content_scripts": [{
          "all_frames": true,
          "matches": ["*://bar.com/*"],
          "js": ["content_script.js"]
        }]
      } )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
                document.body.innerText = 'content script has run';
                chrome.test.sendMessage('Hello from content script!'); )");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to a test page that is *not* covered by `content_scripts.matches`
  // manifest entry above.
  GURL ignored_url = embedded_test_server()->GetURL("foo.com", "/title1.html");
  ui_test_utils::NavigateToURL(browser(), ignored_url);
  content::WebContents* first_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Verify that initially no processes show up as having been injected with
  // content scripts.
  EXPECT_EQ("This page has no title.",
            content::EvalJs(first_tab, "document.body.innerText"));
  EXPECT_FALSE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
      *first_tab->GetMainFrame()->GetProcess(), extension->id()));

  // Navigate to a test page that *is* covered by `content_scripts.matches`
  // manifest entry above.
  {
    GURL injected_url =
        embedded_test_server()->GetURL("bar.com", "/title1.html");
    ExtensionTestMessageListener listener("Hello from content script!", false);
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), injected_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }
  content::WebContents* second_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(first_tab, second_tab);

  // Verify that the new tab shows up as having been injected with content
  // scripts.
  EXPECT_EQ("content script has run",
            content::EvalJs(second_tab, "document.body.innerText"));
  EXPECT_EQ("This page has no title.",
            content::EvalJs(first_tab, "document.body.innerText"));
  EXPECT_TRUE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
      *second_tab->GetMainFrame()->GetProcess(), extension->id()));
  EXPECT_FALSE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
      *first_tab->GetMainFrame()->GetProcess(), extension->id()));
}

class ContentScriptTrackerMatchOriginAsFallbackBrowserTest
    : public ContentScriptTrackerBrowserTest {
 public:
  ContentScriptTrackerMatchOriginAsFallbackBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kContentScriptsMatchOriginAsFallback);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Covers detecting content script injection into a 'data:...' URL.
IN_PROC_BROWSER_TEST_F(
    ContentScriptTrackerMatchOriginAsFallbackBrowserTest,
    ContentScriptDeclarationInExtensionManifest_DataUrlIframe) {
  // Install a test extension.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "ContentScriptTrackerBrowserTest - Declarative",
        "version": "1.0",
        "manifest_version": 2,
        "permissions": [ "tabs", "<all_urls>" ],
        "content_scripts": [{
          "all_frames": true,
          "match_about_blank": true,
          "match_origin_as_fallback": true,
          "matches": ["*://bar.com/*"],
          "js": ["content_script.js"]
        }]
      } )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
                document.body.innerText = 'content script has run';
                chrome.test.sendMessage('Hello from content script!'); )");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to a test page that *is* covered by `content_scripts.matches`
  // manifest entry above.
  {
    GURL injected_url =
        embedded_test_server()->GetURL("bar.com", "/title1.html");
    ExtensionTestMessageListener listener("Hello from content script!", false);
    ui_test_utils::NavigateToURL(browser(), injected_url);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  // Verify that ContentScriptTracker properly covered the initial frame.
  content::WebContents* first_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ("content script has run",
            content::EvalJs(first_tab, "document.body.innerText"));
  EXPECT_TRUE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
      *first_tab->GetMainFrame()->GetProcess(), extension->id()));

  // Add a new subframe with a `data:...` URL.  This will verify that the
  // browser-side ContentScriptTracker correctly accounts for the renderer-side
  // support for injecting contents scripts into data: URLs (see r793302).
  {
    ExtensionTestMessageListener listener("Hello from content script!", false);
    const char kScript[] = R"(
        let iframe = document.createElement('iframe');
        iframe.src = 'data:text/html,contents';
        document.body.appendChild(iframe);
    )";
    ExecuteScriptAsync(first_tab, kScript);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  // Verify that ContentScriptTracker properly covered the new child frame (and
  // continues to correctly cover the initial frame).
  //
  // The verification below is a bit redundant, because `main_frame` and
  // `child_frame` are currently hosted in the same process, but this kind of
  // verification is important if 1( we ever consider going back to per-frame
  // tracking or 2) we start isolating opaque-origin/sandboxed frames into a
  // separate process (tracked in https://crbug.com/510122).
  content::RenderFrameHost* main_frame = first_tab->GetMainFrame();
  content::RenderFrameHost* child_frame = content::ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(child_frame);
  EXPECT_EQ("content script has run",
            content::EvalJs(main_frame, "document.body.innerText"));
  EXPECT_EQ("content script has run",
            content::EvalJs(child_frame, "document.body.innerText"));
  EXPECT_TRUE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
      *main_frame->GetProcess(), extension->id()));
  EXPECT_TRUE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
      *child_frame->GetProcess(), extension->id()));
}

// Covers detecting content script injection into 'about:blank'.
IN_PROC_BROWSER_TEST_F(
    ContentScriptTrackerBrowserTest,
    ContentScriptDeclarationInExtensionManifest_AboutBlankPopup) {
  // Install a test extension.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "ContentScriptTrackerBrowserTest - Declarative",
        "version": "1.0",
        "manifest_version": 2,
        "permissions": [ "tabs", "<all_urls>" ],
        "content_scripts": [{
          "all_frames": true,
          "match_about_blank": true,
          "matches": ["*://bar.com/*"],
          "js": ["content_script.js"]
        }]
      } )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
                document.body.innerText = 'content script has run';
                chrome.test.sendMessage('Hello from content script!'); )");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to a test page that *is* covered by `content_scripts.matches`
  // manifest entry above.
  {
    GURL injected_url =
        embedded_test_server()->GetURL("bar.com", "/title1.html");
    ExtensionTestMessageListener listener("Hello from content script!", false);
    ui_test_utils::NavigateToURL(browser(), injected_url);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  // Verify that ContentScriptTracker properly covered the initial frame.
  content::WebContents* first_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ("content script has run",
            content::EvalJs(first_tab, "document.body.innerText"));
  EXPECT_TRUE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
      *first_tab->GetMainFrame()->GetProcess(), extension->id()));

  // Open a new tab with 'about:blank'.  This may be tricky, because the initial
  // 'about:blank' navigation will not go through ReadyToCommit state.
  content::WebContents* popup = nullptr;
  {
    ExtensionTestMessageListener listener("Hello from content script!", false);
    content::WebContentsAddedObserver popup_observer;
    ASSERT_TRUE(ExecJs(first_tab, "window.open('about:blank', '_blank')"));
    popup = popup_observer.GetWebContents();
    WaitForLoadStop(popup);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  // Verify that ContentScriptTracker properly covered the popup (and continues
  // to correctly cover the initial frame).  The verification below is a bit
  // redundant, because `first_tab` and `popup` are hosted in the same process,
  // but this kind of verification is important if we ever consider going back
  // to per-frame tracking.
  EXPECT_EQ("content script has run",
            content::EvalJs(first_tab, "document.body.innerText"));
  EXPECT_EQ("content script has run",
            content::EvalJs(popup, "document.body.innerText"));
  EXPECT_TRUE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
      *first_tab->GetMainFrame()->GetProcess(), extension->id()));
  EXPECT_TRUE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
      *popup->GetMainFrame()->GetProcess(), extension->id()));
}

// Covers detecting content script injection into an initial empty document.
//
// The code below exercises the test steps from "scenario #3" from the "Tracking
// injections in an initial empty document" section of a @chromium.org document
// here:
// https://docs.google.com/document/d/1MFprp2ss2r9RNamJ7Jxva1bvRZvec3rzGceDGoJ6vW0/edit?usp=sharing
IN_PROC_BROWSER_TEST_F(
    ContentScriptTrackerBrowserTest,
    ContentScriptDeclarationInExtensionManifest_SubframeWithInitialEmptyDoc) {
  // Install a test extension.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "ContentScriptTrackerBrowserTest - Declarative",
        "version": "1.0",
        "manifest_version": 2,
        "permissions": [ "tabs", "<all_urls>" ],
        "content_scripts": [{
          "all_frames": true,
          "match_about_blank": true,
          "matches": ["*://bar.com/title1.html"],
          "js": ["content_script.js"]
        }]
      } )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
      var counter = 0;
      function leaveContentScriptMarker() {
          const kExpectedText = 'content script has run: ';
          if (document.body.innerText.startsWith(kExpectedText))
            return;

          counter += 1;
          document.body.innerText = kExpectedText + counter;
          chrome.test.sendMessage('Hello from content script!');
      }

      // Leave a content script mark *now*.
      leaveContentScriptMarker();

      // Periodically check if the mark needs to be reinserted (with a new value
      // of `counter`).  This helps to demonstrate (in a test step somewhere
      // below) that the content script "survives" a `document.open` operation.
      setInterval(leaveContentScriptMarker, 100);  )");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to a test page that *is* covered by `content_scripts.matches`
  // manifest entry above.
  {
    GURL injected_url =
        embedded_test_server()->GetURL("bar.com", "/title1.html");
    ExtensionTestMessageListener listener("Hello from content script!", false);
    ui_test_utils::NavigateToURL(browser(), injected_url);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  // Verify that ContentScriptTracker properly covered the initial frame.
  content::WebContents* first_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ("content script has run: 1",
            content::EvalJs(first_tab, "document.body.innerText"));
  EXPECT_TRUE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
      *first_tab->GetMainFrame()->GetProcess(), extension->id()));

  // Add a new subframe with `src=javascript:...` attribute.  This will leave
  // the subframe at the initial empty document (no navigation / no
  // ReadyToCommit), but still end up injecting the content script.
  //
  // (This is "Step 1" from the doc linked in the comment right above
  // IN_PROC_BROWSER_TEST_F.)
  {
    ExtensionTestMessageListener listener("Hello from content script!", false);
    const char kScript[] = R"(
        let iframe = document.createElement('iframe');
        iframe.name = 'test-child-frame';
        iframe.src = 'javascript:"something"';
        document.body.appendChild(iframe);
    )";
    ExecuteScriptAsync(first_tab, kScript);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  // Verify expected properties of the test scenario - the `child_frame` should
  // have stayed at the initial empty document.
  content::RenderFrameHost* main_frame = first_tab->GetMainFrame();
  content::RenderFrameHost* child_frame = content::ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(child_frame);
  EXPECT_EQ(main_frame->GetLastCommittedOrigin().Serialize(),
            content::EvalJs(child_frame, "origin"));
  // Renderer-side and browser-side do not exactly agree on the URL of the child
  // frame...
  EXPECT_EQ("about:blank", content::EvalJs(child_frame, "location.href"));
  EXPECT_EQ(GURL(), child_frame->GetLastCommittedURL());

  // Verify that ContentScriptTracker properly covered the new child frame (and
  // continues to correctly cover the initial frame).  The verification below is
  // a bit redundant, because `main_frame` and `child_frame` are hosted in the
  // same process, but this kind of verification is important if we ever
  // consider going back to per-frame tracking.
  EXPECT_EQ("content script has run: 1",
            content::EvalJs(main_frame, "document.body.innerText"));
  EXPECT_EQ("content script has run: 1",
            content::EvalJs(child_frame, "document.body.innerText"));
  EXPECT_TRUE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
      *main_frame->GetProcess(), extension->id()));
  EXPECT_TRUE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
      *child_frame->GetProcess(), extension->id()));

  // Execute `document.open()` on the initial empty document child frame.  The
  // content script injected previously will survive this (event listeners are
  // reset but the `setInterval` callback keeps executing).
  //
  // This step changes the URL of the `child_frame` (in a same-document
  // navigation) from "about:blank" to a URL that (unlike the parent) is no
  // longer covered by the `matches` patterns from the extension manifest.
  {
    // Inject a new frame to execute `document.open` from.
    //
    // (This is "Step 2" from the doc linked in the comment right above
    // IN_PROC_BROWSER_TEST_F.)
    content::TestNavigationObserver nav_observer(first_tab, 1);
    const char kFrameInsertingScriptTemplate[] = R"(
        var f = document.createElement('iframe');
        f.src = $1;
        document.body.appendChild(f);
    )";
    GURL non_injected_url =
        embedded_test_server()->GetURL("bar.com", "/title2.html");
    ASSERT_TRUE(content::ExecJs(
        main_frame,
        content::JsReplace(kFrameInsertingScriptTemplate, non_injected_url)));
    nav_observer.Wait();
  }
  content::RenderFrameHost* another_frame =
      content::ChildFrameAt(main_frame, 1);
  ASSERT_TRUE(another_frame);
  {
    // Execute `document.open`.
    //
    // (This is "Step 3" from the doc linked in the comment right above
    // IN_PROC_BROWSER_TEST_F.)
    ExtensionTestMessageListener listener("Hello from content script!", false);
    const char kDocumentWritingScript[] = R"(
        var win = window.open('', 'test-child-frame');
        win.document.open();
        win.document.close();
    )";
    ASSERT_TRUE(content::ExecJs(another_frame, kDocumentWritingScript));

    // Demonstrate that the original content script has survived "resetting" of
    // the document.  (document.open/write/close triggers a same-document
    // navigation - it keeps the document/window/RenderFrame[Host];  OTOH we use
    // setInterval because it is one of few things that survive across such
    // boundary - in particular all event listeners will be reset.)
    ASSERT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_EQ("content script has run: 2",
              content::EvalJs(child_frame, "document.body.innerText"));

    // Demonstrate that `document.open` didn't change the URL of the
    // `child_frame`.
    EXPECT_EQ(another_frame->GetLastCommittedURL(),
              content::EvalJs(child_frame, "location.href"));
    EXPECT_EQ(GURL(), child_frame->GetLastCommittedURL());
  }

  // Verify that ContentScriptTracker still properly covers both frames.  The
  // verification below is a bit redundant, because `main_frame` and
  // `child_frame` are hosted in the same process, but this kind of verification
  // is important if we ever consider going back to per-frame tracking.
  EXPECT_TRUE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
      *main_frame->GetProcess(), extension->id()));
  EXPECT_TRUE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
      *child_frame->GetProcess(), extension->id()));
}

// Tests tracking of content scripts injected/declared via
// `chrome.declarativeContent` API.  See also:
// https://developer.chrome.com/docs/extensions/reference/declarativeContent/#type-RequestContentScript
IN_PROC_BROWSER_TEST_F(ContentScriptTrackerBrowserTest,
                       ContentScriptViaDeclarativeContentApi) {
  // Install a test extension.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "ContentScriptTrackerBrowserTest - Declarative",
        "version": "1.0",
        "manifest_version": 2,
        "permissions": [ "tabs", "<all_urls>", "declarativeContent" ],
        "background": {"scripts": ["background_script.js"]}
      } )";
  const char kBackgroundScript[] = R"(
      var rule = {
        conditions: [
          new chrome.declarativeContent.PageStateMatcher({
            pageUrl: { hostEquals: 'bar.com', schemes: ['http', 'https'] }
          })
        ],
        actions: [ new chrome.declarativeContent.RequestContentScript({
          js: ["content_script.js"]
        }) ]
      };

      chrome.runtime.onInstalled.addListener(function(details) {
          chrome.declarativeContent.onPageChanged.addRules([rule]);
      }); )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("background_script.js"), kBackgroundScript);
  const char kContentScript[] = R"(
      window.onload = function() {
          document.body.innerText = 'content script has run';
          chrome.test.sendMessage('Hello from content script!');
      }
  )";
  dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), kContentScript);
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to a test page that is *not* covered by the PageStateMatcher used
  // above.
  GURL ignored_url = embedded_test_server()->GetURL("foo.com", "/title1.html");
  ui_test_utils::NavigateToURL(browser(), ignored_url);

  // Verify that initially no frames show up as having been injected with
  // content scripts.
  content::WebContents* first_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ("This page has no title.",
            content::EvalJs(first_tab, "document.body.innerText"));
  EXPECT_FALSE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
      *first_tab->GetMainFrame()->GetProcess(), extension->id()));

  // Navigate to a test page that *is* covered by the PageStateMatcher above.
  {
    GURL injected_url =
        embedded_test_server()->GetURL("bar.com", "/title1.html");
    ExtensionTestMessageListener listener("Hello from content script!", false);
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), injected_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }
  content::WebContents* second_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(first_tab, second_tab);

  // Verify that the new tab shows up as having been injected with content
  // scripts.
  EXPECT_EQ("content script has run",
            content::EvalJs(second_tab, "document.body.innerText"));
  EXPECT_EQ("This page has no title.",
            content::EvalJs(first_tab, "document.body.innerText"));
  EXPECT_TRUE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
      *second_tab->GetMainFrame()->GetProcess(), extension->id()));
  EXPECT_FALSE(ContentScriptTracker::DidProcessRunContentScriptFromExtension(
      *first_tab->GetMainFrame()->GetProcess(), extension->id()));
}

}  // namespace extensions
