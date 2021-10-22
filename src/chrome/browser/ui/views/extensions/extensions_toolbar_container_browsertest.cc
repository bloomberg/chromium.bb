// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/test_toolbar_actions_bar_bubble_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_browsertest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/widget_test.h"

namespace {

constexpr char kInjectionSucceededMessage[] = "injection succeeded";

class BlockedActionWaiter
    : public extensions::ExtensionActionRunner::TestObserver {
 public:
  explicit BlockedActionWaiter(extensions::ExtensionActionRunner* runner)
      : runner_(runner), run_loop_(std::make_unique<base::RunLoop>()) {
    runner_->set_observer_for_testing(this);
  }
  BlockedActionWaiter(const BlockedActionWaiter&) = delete;
  BlockedActionWaiter& operator=(const BlockedActionWaiter&) = delete;
  ~BlockedActionWaiter() { runner_->set_observer_for_testing(nullptr); }

  void WaitAndReset() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

 private:
  // ExtensionActionRunner::TestObserver:
  void OnBlockedActionAdded() override { run_loop_->Quit(); }

  extensions::ExtensionActionRunner* runner_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace

class ExtensionsToolbarContainerBrowserTest
    : public ExtensionsToolbarBrowserTest {
 public:
  enum class ExtensionRemovalMethod {
    kDisable,
    kUninstall,
    kBlocklist,
    kTerminate,
  };

  ExtensionsToolbarContainerBrowserTest() = default;
  ExtensionsToolbarContainerBrowserTest(
      const ExtensionsToolbarContainerBrowserTest&) = delete;
  ExtensionsToolbarContainerBrowserTest& operator=(
      const ExtensionsToolbarContainerBrowserTest&) = delete;
  ~ExtensionsToolbarContainerBrowserTest() override = default;

  void ClickOnAction(ToolbarActionView* action) {
    ui::MouseEvent click_down_event(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                    gfx::Point(), base::TimeTicks(),
                                    ui::EF_LEFT_MOUSE_BUTTON, 0);
    ui::MouseEvent click_up_event(ui::ET_MOUSE_RELEASED, gfx::Point(),
                                  gfx::Point(), base::TimeTicks(),
                                  ui::EF_LEFT_MOUSE_BUTTON, 0);
    action->OnMouseEvent(&click_down_event);
    action->OnMouseEvent(&click_up_event);
  }

  void ShowUi(const std::string& name) override { NOTREACHED(); }

  void RemoveExtension(ExtensionRemovalMethod method,
                       const std::string& extension_id) {
    extensions::ExtensionService* const extension_service =
        extensions::ExtensionSystem::Get(browser()->profile())
            ->extension_service();
    switch (method) {
      case ExtensionRemovalMethod::kDisable:
        extension_service->DisableExtension(
            extension_id, extensions::disable_reason::DISABLE_USER_ACTION);
        break;
      case ExtensionRemovalMethod::kUninstall:
        extension_service->UninstallExtension(
            extension_id, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
        break;
      case ExtensionRemovalMethod::kBlocklist:
        extension_service->BlocklistExtensionForTest(extension_id);
        break;
      case ExtensionRemovalMethod::kTerminate:
        extension_service->TerminateExtension(extension_id);
        break;
    }

    // Removing an extension can result in the container changing visibility.
    // Allow it to finish laying out appropriately.
    auto* container = GetExtensionsToolbarContainer();
    container->GetWidget()->LayoutRootViewIfNecessary();
  }

  void VerifyContainerVisibility(ExtensionRemovalMethod method,
                                 bool expected_visibility) {
    // An empty container should not be shown.
    EXPECT_FALSE(GetExtensionsToolbarContainer()->GetVisible());

    // Loading the first extension should show the button (and container).
    LoadTestExtension("extensions/uitest/long_name");
    EXPECT_TRUE(GetExtensionsToolbarContainer()->IsDrawn());

    // Add another extension so we can make sure that removing some don't change
    // the visibility.
    LoadTestExtension("extensions/uitest/window_open");

    // Remove 1/2 extensions, should still be drawn.
    RemoveExtension(method, extensions()[0]->id());
    EXPECT_TRUE(GetExtensionsToolbarContainer()->IsDrawn());

    // Removing the last extension. All actions now have the same state.
    RemoveExtension(method, extensions()[1]->id());

    // Container should remain visible during the removal animation.
    EXPECT_TRUE(GetExtensionsToolbarContainer()->IsDrawn());
    views::test::WaitForAnimatingLayoutManager(GetExtensionsToolbarContainer());
    EXPECT_EQ(expected_visibility, GetExtensionsToolbarContainer()->IsDrawn());
  }
};

// TODO(devlin): There are probably some tests from
// ExtensionsMenuViewInteractiveUITest that should move here (if they test the
// toolbar container more than the menu).

// Tests that invocation metrics are properly recorded when triggering
// extensions from the toolbar.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerBrowserTest,
                       InvocationMetrics) {
  base::HistogramTester histogram_tester;
  scoped_refptr<const extensions::Extension> extension =
      LoadTestExtension("extensions/uitest/extension_with_action_and_command");

  EXPECT_EQ(1u, GetToolbarActionViews().size());
  EXPECT_EQ(0u, GetVisibleToolbarActionViews().size());

  ToolbarActionsModel* const model = ToolbarActionsModel::Get(profile());
  model->SetActionVisibility(extension->id(), true);

  auto* container = GetExtensionsToolbarContainer();
  container->GetWidget()->LayoutRootViewIfNecessary();

  ASSERT_EQ(1u, GetVisibleToolbarActionViews().size());
  ToolbarActionView* const action = GetVisibleToolbarActionViews()[0];

  constexpr char kHistogramName[] = "Extensions.Toolbar.InvocationSource";
  histogram_tester.ExpectTotalCount(kHistogramName, 0);

  // Trigger the extension by clicking on it.
  ClickOnAction(action);

  histogram_tester.ExpectTotalCount(kHistogramName, 1);
  histogram_tester.ExpectBucketCount(
      kHistogramName,
      ToolbarActionViewController::InvocationSource::kToolbarButton, 1);
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerBrowserTest,
                       InvisibleWithoutExtension_Disable) {
  VerifyContainerVisibility(ExtensionRemovalMethod::kDisable, false);
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerBrowserTest,
                       InvisibleWithoutExtension_Uninstall) {
  VerifyContainerVisibility(ExtensionRemovalMethod::kUninstall, false);
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerBrowserTest,
                       InvisibleWithoutExtension_Blocklist) {
  VerifyContainerVisibility(ExtensionRemovalMethod::kBlocklist, false);
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerBrowserTest,
                       InvisibleWithoutExtension_Terminate) {
  // TODO(pbos): Keep the container visible when extensions are terminated
  // (crash). This lets users find and restart them. Then update this test
  // expectation to be kept visible by terminated extensions. Also update the
  // test name to reflect that the container should be visible with only
  // terminated extensions.
  VerifyContainerVisibility(ExtensionRemovalMethod::kTerminate, false);
}

// Tests that clicking on a second extension action will close a first if its
// popup was open.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerBrowserTest,
                       ClickingOnASecondActionClosesTheFirst) {
  std::vector<std::unique_ptr<extensions::TestExtensionDir>> test_dirs;
  auto load_extension = [&](const char* extension_name) {
    constexpr char kManifestTemplate[] =
        R"({
             "name": "%s",
             "manifest_version": 3,
             "action": { "default_popup": "popup.html" },
             "version": "0.1"
           })";
    constexpr char kPopupHtml[] =
        R"(<html><script src="popup.js"></script></html>)";
    constexpr char kPopupJsTemplate[] =
        R"(chrome.test.sendMessage('%s popup opened');)";

    auto test_dir = std::make_unique<extensions::TestExtensionDir>();
    test_dir->WriteManifest(
        base::StringPrintf(kManifestTemplate, extension_name));
    test_dir->WriteFile(FILE_PATH_LITERAL("popup.html"), kPopupHtml);
    test_dir->WriteFile(FILE_PATH_LITERAL("popup.js"),
                        base::StringPrintf(kPopupJsTemplate, extension_name));
    scoped_refptr<const extensions::Extension> extension =
        extensions::ChromeTestExtensionLoader(browser()->profile())
            .LoadExtension(test_dir->UnpackedPath());
    test_dirs.push_back(std::move(test_dir));
    return extension;
  };

  // Load up a couple extensions with actions in the toolbar.
  scoped_refptr<const extensions::Extension> alpha = load_extension("alpha");
  ASSERT_TRUE(alpha);
  scoped_refptr<const extensions::Extension> beta = load_extension("beta");
  ASSERT_TRUE(beta);

  // Pin each to the toolbar, and grab their views.
  ToolbarActionsModel* const model = ToolbarActionsModel::Get(profile());
  model->SetActionVisibility(alpha->id(), true);
  model->SetActionVisibility(beta->id(), true);

  auto* container = GetExtensionsToolbarContainer();
  container->GetWidget()->LayoutRootViewIfNecessary();

  auto toolbar_views = GetVisibleToolbarActionViews();
  ASSERT_EQ(2u, toolbar_views.size());

  ToolbarActionView* const alpha_action = toolbar_views[0];
  EXPECT_EQ(alpha->id(), alpha_action->view_controller()->GetId());
  ToolbarActionView* const beta_action = toolbar_views[1];
  EXPECT_EQ(beta->id(), beta_action->view_controller()->GetId());

  extensions::ProcessManager* const process_manager =
      extensions::ProcessManager::Get(profile());

  // To start, neither extensions should have any render frames (which here
  // equates to no open popus).
  EXPECT_EQ(
      0u, process_manager->GetRenderFrameHostsForExtension(alpha->id()).size());
  EXPECT_EQ(
      0u, process_manager->GetRenderFrameHostsForExtension(beta->id()).size());

  {
    // Click on Alpha and wait for it to open the popup.
    ExtensionTestMessageListener listener("alpha popup opened",
                                          /*will_reply=*/false);
    ClickOnAction(alpha_action);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  // Verify that Alpha (and only Alpha) has an active frame (i.e., popup).
  ASSERT_EQ(
      1u, process_manager->GetRenderFrameHostsForExtension(alpha->id()).size());
  EXPECT_EQ(
      0u, process_manager->GetRenderFrameHostsForExtension(beta->id()).size());
  // And confirm this matches the underlying controller's state.
  EXPECT_TRUE(alpha_action->view_controller()->IsShowingPopup());
  EXPECT_FALSE(beta_action->view_controller()->IsShowingPopup());

  {
    // Click on Beta. This should result in Beta's popup opening and Alpha's
    // closing.
    content::RenderFrameHost* const popup_frame =
        *process_manager->GetRenderFrameHostsForExtension(alpha->id()).begin();
    content::WebContentsDestroyedWatcher popup_destroyed(
        content::WebContents::FromRenderFrameHost(popup_frame));
    ExtensionTestMessageListener listener("beta popup opened",
                                          /*will_reply=*/false);
    ClickOnAction(beta_action);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    popup_destroyed.Wait();
  }

  // Beta (and only Beta) should have an active popup.
  EXPECT_EQ(
      0u, process_manager->GetRenderFrameHostsForExtension(alpha->id()).size());
  ASSERT_EQ(
      1u, process_manager->GetRenderFrameHostsForExtension(beta->id()).size());
  EXPECT_FALSE(alpha_action->view_controller()->IsShowingPopup());
  EXPECT_TRUE(beta_action->view_controller()->IsShowingPopup());
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerBrowserTest,
                       ShowToolbarActionsBarBubbleForExtension_Pinned) {
  scoped_refptr<const extensions::Extension> extension =
      LoadTestExtension("extensions/simple_with_popup");
  ASSERT_TRUE(extension);

  ToolbarActionsModel* const model = ToolbarActionsModel::Get(profile());
  model->SetActionVisibility(extension->id(), true);
  ExtensionsToolbarContainer* const container = GetExtensionsToolbarContainer();
  container->GetWidget()->LayoutRootViewIfNecessary();

  {
    auto visible_actions = GetVisibleToolbarActionViews();
    ASSERT_EQ(1u, visible_actions.size());
    EXPECT_EQ(extension->id(), visible_actions[0]->view_controller()->GetId());
  }

  TestToolbarActionsBarBubbleDelegate test_delegate(u"Heading", u"Body",
                                                    u"Action");
  test_delegate.set_action_id(extension->id());
  container->ShowToolbarActionBubble(test_delegate.GetDelegate());
  views::Widget* const bubble_widget =
      container->GetAnchoredWidgetForExtensionForTesting(extension->id());
  ASSERT_TRUE(bubble_widget);
  views::test::WidgetVisibleWaiter(bubble_widget).Wait();

  EXPECT_TRUE(test_delegate.shown());
  {
    views::test::WidgetDestroyedWaiter destroyed_waiter(bubble_widget);
    bubble_widget->Close();
    destroyed_waiter.Wait();
  }

  ASSERT_TRUE(test_delegate.close_action());
  EXPECT_EQ(ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_DEACTIVATION,
            *test_delegate.close_action());
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerBrowserTest,
                       ShowToolbarActionsBarBubbleForExtension_Unpinned) {
  scoped_refptr<const extensions::Extension> extension =
      LoadTestExtension("extensions/simple_with_popup");
  ASSERT_TRUE(extension);

  ExtensionsToolbarContainer* const container = GetExtensionsToolbarContainer();
  ToolbarActionViewController* const action =
      container->GetActionForId(extension->id());

  EXPECT_EQ(0u, GetVisibleToolbarActionViews().size());

  TestToolbarActionsBarBubbleDelegate test_delegate(u"Heading", u"Body",
                                                    u"Action");
  test_delegate.set_action_id(extension->id());
  container->ShowToolbarActionBubble(test_delegate.GetDelegate());
  views::Widget* const bubble_widget =
      container->GetAnchoredWidgetForExtensionForTesting(extension->id());
  ASSERT_TRUE(bubble_widget);
  views::test::WidgetVisibleWaiter(bubble_widget).Wait();

  EXPECT_TRUE(container->IsActionVisibleOnToolbar(action));

  EXPECT_TRUE(test_delegate.shown());
  {
    views::test::WidgetDestroyedWaiter destroyed_waiter(bubble_widget);
    bubble_widget->Close();
    destroyed_waiter.Wait();
  }

  ASSERT_TRUE(test_delegate.close_action());
  EXPECT_EQ(ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_DEACTIVATION,
            *test_delegate.close_action());

  EXPECT_FALSE(container->IsActionVisibleOnToolbar(action));
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerBrowserTest,
                       ShowToolbarActionsBarBubbleForExtension_NoAction) {
  scoped_refptr<const extensions::Extension> extension =
      LoadTestExtension("extensions/simple_with_popup");
  ASSERT_TRUE(extension);

  // Disable the extension. Disabled extensions don't display in the toolbar.
  extensions::ExtensionService* const extension_service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  extension_service->DisableExtension(
      extension->id(), extensions::disable_reason::DISABLE_USER_ACTION);

  ExtensionsToolbarContainer* const container = GetExtensionsToolbarContainer();
  EXPECT_FALSE(container->GetActionForId(extension->id()));

  EXPECT_EQ(0u, GetVisibleToolbarActionViews().size());

  TestToolbarActionsBarBubbleDelegate test_delegate(u"Heading", u"Body",
                                                    u"Action");
  test_delegate.set_action_id(extension->id());
  container->ShowToolbarActionBubble(test_delegate.GetDelegate());
  views::Widget* const bubble_widget =
      container->GetAnchoredWidgetForExtensionForTesting(extension->id());
  ASSERT_TRUE(bubble_widget);
  views::test::WidgetVisibleWaiter(bubble_widget).Wait();

  EXPECT_EQ(0u, GetVisibleToolbarActionViews().size());

  EXPECT_TRUE(test_delegate.shown());
  {
    views::test::WidgetDestroyedWaiter destroyed_waiter(bubble_widget);
    bubble_widget->Close();
    destroyed_waiter.Wait();
  }

  ASSERT_TRUE(test_delegate.close_action());
  EXPECT_EQ(ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_DEACTIVATION,
            *test_delegate.close_action());
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerBrowserTest,
                       UninstallExtensionWithActivelyShownToolbarActionBubble) {
  scoped_refptr<const extensions::Extension> extension =
      LoadTestExtension("extensions/simple_with_popup");
  ASSERT_TRUE(extension);

  ToolbarActionsModel* const model = ToolbarActionsModel::Get(profile());
  model->SetActionVisibility(extension->id(), true);
  ExtensionsToolbarContainer* const container = GetExtensionsToolbarContainer();
  container->GetWidget()->LayoutRootViewIfNecessary();

  {
    auto visible_actions = GetVisibleToolbarActionViews();
    ASSERT_EQ(1u, visible_actions.size());
    EXPECT_EQ(extension->id(), visible_actions[0]->view_controller()->GetId());
  }

  TestToolbarActionsBarBubbleDelegate test_delegate(u"Heading", u"Body",
                                                    u"Action");
  test_delegate.set_action_id(extension->id());
  container->ShowToolbarActionBubble(test_delegate.GetDelegate());
  views::Widget* const bubble_widget =
      container->GetAnchoredWidgetForExtensionForTesting(extension->id());
  ASSERT_TRUE(bubble_widget);
  views::test::WidgetVisibleWaiter(bubble_widget).Wait();

  EXPECT_TRUE(test_delegate.shown());

  {
    extensions::ExtensionService* const extension_service =
        extensions::ExtensionSystem::Get(profile())->extension_service();
    extension_service->UninstallExtension(
        extension->id(), extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
  }

  EXPECT_EQ(0u, GetVisibleToolbarActionViews().size());
  EXPECT_FALSE(container->GetActionForId(extension->id()));

  // TODO(devlin): When the extension is removed, we don't currently remove any
  // widgets associated with it. This test ensures we don't crash (yay!), but we
  // should very likely close the bubble as well. I wouldn't be surprised if
  // some bubble handlers don't expect the extension to be gone.
  views::test::WidgetDestroyedWaiter destroyed_waiter(bubble_widget);
  bubble_widget->Close();
  destroyed_waiter.Wait();
}

// Verifies that dragging extension icons is disabled in incognito windows.
// https://crbug.com/1203833.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarContainerBrowserTest,
                       IncognitoDraggingIsDisabled) {
  // Load an extension, pin it, and enable it in incognito.
  scoped_refptr<const extensions::Extension> extension =
      LoadTestExtension("extensions/simple_with_popup");
  ASSERT_TRUE(extension);

  ToolbarActionsModel* const toolbar_model =
      ToolbarActionsModel::Get(profile());
  toolbar_model->SetActionVisibility(extension->id(), true);

  {
    extensions::TestExtensionRegistryObserver observer(
        extensions::ExtensionRegistry::Get(profile()), extension->id());
    extensions::util::SetIsIncognitoEnabled(extension->id(), profile(), true);
    ASSERT_TRUE(observer.WaitForExtensionLoaded());
  }

  Browser* incognito_browser = CreateIncognitoBrowser();

  views::test::WaitForAnimatingLayoutManager(GetExtensionsToolbarContainer());
  views::test::WaitForAnimatingLayoutManager(
      GetExtensionsToolbarContainerForBrowser(incognito_browser));

  // Verify the extension has a (visible) action for both the incognito and
  // on-the-record browser.
  std::vector<ToolbarActionView*> on_the_record_views = GetToolbarActionViews();
  ASSERT_EQ(1u, on_the_record_views.size());
  ToolbarActionView* on_the_record_view = on_the_record_views[0];
  EXPECT_EQ(extension->id(), on_the_record_view->view_controller()->GetId());
  EXPECT_TRUE(on_the_record_view->GetVisible());

  std::vector<ToolbarActionView*> incognito_views =
      GetToolbarActionViewsForBrowser(incognito_browser);
  ASSERT_EQ(1u, incognito_views.size());
  ToolbarActionView* incognito_view = incognito_views[0];
  EXPECT_EQ(extension->id(), incognito_view->view_controller()->GetId());
  EXPECT_TRUE(incognito_view->GetVisible());

  // Dragging should be enabled for the on-the-record view, but not the
  // incognito view.
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE,
            on_the_record_view->GetDragOperationsForTest(gfx::Point()));
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE,
            incognito_view->GetDragOperationsForTest(gfx::Point()));

  // The two views should have the same notifiable event. This is important to
  // test, since it can be dependent on draggability.
  EXPECT_EQ(on_the_record_view->button_controller()->notify_action(),
            incognito_view->button_controller()->notify_action());
}

namespace {

class IncognitoExtensionsToolbarContainerBrowserTest
    : public ExtensionsToolbarContainerBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionsToolbarContainerBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIncognito);
  }
};

}  // namespace

// Tests that first loading an extension action in an incognito profile, then
// removing the incognito profile and using the extension action in a normal
// profile doesn't crash.
// Regression test for crbug.com/663726.
IN_PROC_BROWSER_TEST_F(IncognitoExtensionsToolbarContainerBrowserTest,
                       TestExtensionFirstLoadedInIncognitoMode) {
  EXPECT_TRUE(browser()->profile()->IsOffTheRecord());

  scoped_refptr<const extensions::Extension> extension =
      LoadTestExtension("extensions/api_test/browser_action_with_icon",
                        /*allow_incognito=*/true);
  ASSERT_TRUE(extension);
  Browser* second_browser = CreateBrowser(profile()->GetOriginalProfile());
  EXPECT_FALSE(second_browser->profile()->IsOffTheRecord());

  CloseBrowserSynchronously(browser());

  std::vector<ToolbarActionView*> extension_views =
      GetToolbarActionViewsForBrowser(second_browser);
  ASSERT_EQ(1u, extension_views.size());

  gfx::ImageSkia icon = extension_views[0]->GetIconForTest();
  // Force the image to try and load a representation.
  icon.GetRepresentation(2.0);
}

class ExtensionsToolbarRuntimeHostPermissionsBrowserTest
    : public ExtensionsToolbarContainerBrowserTest {
 public:
  enum class ContentScriptRunLocation {
    DOCUMENT_START,
    DOCUMENT_IDLE,
  };

  ExtensionsToolbarRuntimeHostPermissionsBrowserTest() = default;
  ExtensionsToolbarRuntimeHostPermissionsBrowserTest(
      const ExtensionsToolbarRuntimeHostPermissionsBrowserTest&) = delete;
  ExtensionsToolbarRuntimeHostPermissionsBrowserTest& operator=(
      const ExtensionsToolbarRuntimeHostPermissionsBrowserTest&) = delete;
  ~ExtensionsToolbarRuntimeHostPermissionsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionsToolbarContainerBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void LoadAllUrlsExtension(ContentScriptRunLocation run_location) {
    std::string run_location_str;
    switch (run_location) {
      case ContentScriptRunLocation::DOCUMENT_START:
        run_location_str = "document_start";
        break;
      case ContentScriptRunLocation::DOCUMENT_IDLE:
        run_location_str = "document_idle";
        break;
    }
    extension_dir_.WriteManifest(base::StringPrintf(R"({
             "name": "All Urls Extension",
             "description": "Runs a content script everywhere",
             "manifest_version": 2,
             "version": "0.1",
             "content_scripts": [{
               "matches": ["<all_urls>"],
               "js": ["script.js"],
               "run_at": "%s"
             }]
           })",
                                                    run_location_str.c_str()));
    extension_dir_.WriteFile(
        FILE_PATH_LITERAL("script.js"),
        base::StringPrintf("chrome.test.sendMessage('%s');",
                           kInjectionSucceededMessage));

    extension_ = extensions::ChromeTestExtensionLoader(profile()).LoadExtension(
        extension_dir_.UnpackedPath());
    ASSERT_TRUE(extension_);
    AppendExtension(extension_);
    extensions::ScriptingPermissionsModifier(profile(), extension_)
        .SetWithholdHostPermissions(true);
  }

  const extensions::Extension* extension() const { return extension_.get(); }

  extensions::ExtensionContextMenuModel* GetExtensionContextMenu() {
    ToolbarActionViewController* const controller =
        GetExtensionsToolbarContainer()->GetActionForId(extension_->id());
    return static_cast<extensions::ExtensionContextMenuModel*>(
        controller->GetContextMenu());
  }

  std::u16string GetActionTooltip() {
    return GetExtensionsToolbarContainer()
        ->GetViewForId(extension_->id())
        ->GetTooltipText();
  }

 private:
  extensions::TestExtensionDir extension_dir_;
  scoped_refptr<const extensions::Extension> extension_;
};

// Tests page access modifications through the context menu which require a page
// refresh.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarRuntimeHostPermissionsBrowserTest,
                       ContextMenuPageAccess_RefreshRequired) {
  LoadAllUrlsExtension(ContentScriptRunLocation::DOCUMENT_START);
  std::u16string tooltip_wants_access = base::JoinString(
      {u"All Urls Extension",
       l10n_util::GetStringUTF16(IDS_EXTENSIONS_WANTS_ACCESS_TO_SITE)},
      u"\n");
  std::u16string tooltip_has_access = base::JoinString(
      {u"All Urls Extension",
       l10n_util::GetStringUTF16(IDS_EXTENSIONS_HAS_ACCESS_TO_SITE)},
      u"\n");

  ExtensionTestMessageListener injection_listener(kInjectionSucceededMessage,
                                                  false /* will_reply */);
  injection_listener.set_extension_id(extension()->id());

  GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  extensions::ExtensionActionRunner* runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  BlockedActionWaiter blocked_action_waiter(runner);
  {
    content::TestNavigationObserver observer(web_contents);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // Access to |url| should have been withheld.
  blocked_action_waiter.WaitAndReset();
  EXPECT_TRUE(runner->WantsToRun(extension()));
  extensions::ScriptingPermissionsModifier permissions_modifier(profile(),
                                                                extension());
  EXPECT_FALSE(permissions_modifier.HasGrantedHostPermission(url));
  EXPECT_EQ(tooltip_wants_access, GetActionTooltip());
  EXPECT_FALSE(injection_listener.was_satisfied());

  extensions::ExtensionContextMenuModel* extension_menu =
      GetExtensionContextMenu();
  ASSERT_TRUE(extension_menu);

  // Allow the extension to run on this site. This should show a refresh page
  // bubble. Accept the bubble.
  {
    content::TestNavigationObserver observer(web_contents);
    runner->set_default_bubble_close_action_for_testing(
        std::make_unique<ToolbarActionsBarBubbleDelegate::CloseAction>(
            ToolbarActionsBarBubbleDelegate::CLOSE_EXECUTE));
    extension_menu->ExecuteCommand(
        extensions::ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_SITE,
        0 /* event_flags */);
    observer.WaitForNavigationFinished();
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // The extension should have injected and the extension should no longer want
  // to run.
  ASSERT_TRUE(injection_listener.WaitUntilSatisfied());
  injection_listener.Reset();
  EXPECT_TRUE(permissions_modifier.HasGrantedHostPermission(url));
  EXPECT_EQ(tooltip_has_access, GetActionTooltip());
  EXPECT_FALSE(runner->WantsToRun(extension()));

  // Now navigate to a different host. The extension should have blocked
  // actions.
  {
    url = embedded_test_server()->GetURL("abc.com", "/title1.html");
    content::TestNavigationObserver observer(web_contents);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }
  blocked_action_waiter.WaitAndReset();
  EXPECT_TRUE(runner->WantsToRun(extension()));
  EXPECT_FALSE(permissions_modifier.HasGrantedHostPermission(url));
  EXPECT_EQ(tooltip_wants_access, GetActionTooltip());
  EXPECT_FALSE(injection_listener.was_satisfied());

  // Allow the extension to run on all sites this time. This should again show a
  // refresh bubble. Dismiss it.
  runner->set_default_bubble_close_action_for_testing(
      std::make_unique<ToolbarActionsBarBubbleDelegate::CloseAction>(
          ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_USER_ACTION));
  extension_menu->ExecuteCommand(
      extensions::ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_ALL_SITES,
      0 /* event_flags */);

  // Permissions to the extension shouldn't have been granted, and the extension
  // should still be in wants-to-run state.
  EXPECT_TRUE(runner->WantsToRun(extension()));
  EXPECT_FALSE(permissions_modifier.HasGrantedHostPermission(url));
  EXPECT_EQ(tooltip_wants_access, GetActionTooltip());
  EXPECT_FALSE(injection_listener.was_satisfied());
}

// Tests page access modifications through the context menu which don't require
// a page refresh.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarRuntimeHostPermissionsBrowserTest,
                       ContextMenuPageAccess_RefreshNotRequired) {
  LoadAllUrlsExtension(ContentScriptRunLocation::DOCUMENT_IDLE);
  std::u16string tooltip_wants_access = base::JoinString(
      {u"All Urls Extension",
       l10n_util::GetStringUTF16(IDS_EXTENSIONS_WANTS_ACCESS_TO_SITE)},
      u"\n");
  std::u16string tooltip_has_access = base::JoinString(
      {u"All Urls Extension",
       l10n_util::GetStringUTF16(IDS_EXTENSIONS_HAS_ACCESS_TO_SITE)},
      u"\n");

  ExtensionTestMessageListener injection_listener(kInjectionSucceededMessage,
                                                  false /* will_reply */);
  injection_listener.set_extension_id(extension()->id());

  GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  extensions::ExtensionActionRunner* runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  BlockedActionWaiter blocked_action_waiter(runner);
  {
    content::TestNavigationObserver observer(web_contents);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // Access to |url| should have been withheld.
  blocked_action_waiter.WaitAndReset();
  EXPECT_TRUE(runner->WantsToRun(extension()));
  extensions::ScriptingPermissionsModifier permissions_modifier(profile(),
                                                                extension());
  EXPECT_FALSE(permissions_modifier.HasGrantedHostPermission(url));
  EXPECT_EQ(tooltip_wants_access, GetActionTooltip());
  EXPECT_FALSE(injection_listener.was_satisfied());

  extensions::ExtensionContextMenuModel* extension_menu =
      GetExtensionContextMenu();
  ASSERT_TRUE(extension_menu);

  // Allow the extension to run on this site. Since the blocked actions don't
  // require a refresh, the permission should be granted and the page actions
  // should run.
  extension_menu->ExecuteCommand(
      extensions::ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_SITE,
      0 /* event_flags */);
  ASSERT_TRUE(injection_listener.WaitUntilSatisfied());
  EXPECT_FALSE(runner->WantsToRun(extension()));
  EXPECT_TRUE(permissions_modifier.HasGrantedHostPermission(url));
  EXPECT_EQ(tooltip_has_access, GetActionTooltip());
}
