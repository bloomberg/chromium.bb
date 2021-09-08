// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/custom_handlers/register_protocol_handler_permission_request.h"
#include "chrome/browser/download/download_permission_request.h"
#include "chrome/browser/permissions/attestation_permission_request.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/permission_request_chip.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_bubble_view.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_impl.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_controller_views.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_impl.h"
#include "components/permissions/permission_ui_selector.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_request.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/button_test_api.h"

class PermissionPromptBubbleViewBrowserTest
    : public DialogBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  PermissionPromptBubbleViewBrowserTest() {
    feature_list_.InitWithFeatureState(permissions::features::kPermissionChip,
                                       GetParam());
  }

  PermissionPromptBubbleViewBrowserTest(
      const PermissionPromptBubbleViewBrowserTest&) = delete;
  PermissionPromptBubbleViewBrowserTest& operator=(
      const PermissionPromptBubbleViewBrowserTest&) = delete;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    ui_test_utils::NavigateToURL(browser(),
                                 GURL("https://toplevel.example.com"));
    test_api_ =
        std::make_unique<test::PermissionRequestManagerTestApi>(browser());
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    const std::string& actual_name = name.substr(0, name.find("/"));
    if (actual_name == "security_key") {
      // This one doesn't have a ContentSettingsType.
      test_api_->manager()->AddRequest(
          GetActiveMainFrame(),
          NewAttestationPermissionRequest(url::Origin::Create(GetTestUrl()),
                                          base::BindOnce([](bool) {})));
    } else {
      AddRequestForContentSetting(actual_name);
    }
    base::RunLoop().RunUntilIdle();

    PermissionChip* chip = GetPermissionRequestChipView();
    if (chip) {
      views::test::ButtonTestApi(chip->button())
          .NotifyClick(ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                      gfx::Point(), ui::EventTimeForNow(),
                                      ui::EF_LEFT_MOUSE_BUTTON, 0));
      base::RunLoop().RunUntilIdle();
    }
  }

  bool VerifyUi() override {
    const bool should_close_on_deactivate = GetPermissionRequestChipView();
    views::Widget* prompt_widget = test_api_->GetPromptWindow();
    views::BubbleDialogDelegate* bubble_dialog =
        prompt_widget->widget_delegate()->AsBubbleDialogDelegate();
    EXPECT_EQ(bubble_dialog->close_on_deactivate(), should_close_on_deactivate);

    return DialogBrowserTest::VerifyUi();
  }

  GURL GetTestUrl() { return GURL("https://example.com"); }

  content::RenderFrameHost* GetActiveMainFrame() {
    return browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  }

  PermissionChip* GetPermissionRequestChipView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->toolbar()->location_bar()->chip();
  }

  permissions::PermissionRequest* MakeRegisterProtocolHandlerRequest() {
    std::string protocol = "mailto";
    ProtocolHandler handler =
        ProtocolHandler::CreateProtocolHandler(protocol, GetTestUrl());
    ProtocolHandlerRegistry* registry =
        ProtocolHandlerRegistryFactory::GetForBrowserContext(
            browser()->profile());
    // Deleted in RegisterProtocolHandlerPermissionRequest::RequestFinished().
    return new RegisterProtocolHandlerPermissionRequest(
        registry, handler, GetTestUrl(), base::ScopedClosureRunner());
  }

  void AddRequestForContentSetting(const std::string& name) {
    constexpr const char* kMultipleName = "multiple";
    constexpr struct {
      const char* name;
      ContentSettingsType type;
    } kNameToType[] = {
        {"geolocation", ContentSettingsType::GEOLOCATION},
        {"protected_media", ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER},
        {"notifications", ContentSettingsType::NOTIFICATIONS},
        {"mic", ContentSettingsType::MEDIASTREAM_MIC},
        {"camera", ContentSettingsType::MEDIASTREAM_CAMERA},
        {"protocol_handlers", ContentSettingsType::PROTOCOL_HANDLERS},
        {"midi", ContentSettingsType::MIDI_SYSEX},
        {"storage_access", ContentSettingsType::STORAGE_ACCESS},
        {"downloads", ContentSettingsType::AUTOMATIC_DOWNLOADS},
        {kMultipleName, ContentSettingsType::DEFAULT}};
    const auto* it = std::begin(kNameToType);
    for (; it != std::end(kNameToType); ++it) {
      if (name == it->name)
        break;
    }
    if (it == std::end(kNameToType)) {
      ADD_FAILURE() << "Unknown: " << name;
      return;
    }
    permissions::PermissionRequestManager* manager = test_api_->manager();
    content::RenderFrameHost* source_frame = GetActiveMainFrame();
    switch (it->type) {
      case ContentSettingsType::PROTOCOL_HANDLERS:
        manager->AddRequest(source_frame, MakeRegisterProtocolHandlerRequest());
        break;
      case ContentSettingsType::AUTOMATIC_DOWNLOADS:
        manager->AddRequest(source_frame,
                            new DownloadPermissionRequest(
                                nullptr, url::Origin::Create(GetTestUrl())));
        break;
      case ContentSettingsType::DURABLE_STORAGE:
        // TODO(tapted): Prompt for quota request.
        break;
      case ContentSettingsType::MEDIASTREAM_MIC:
      case ContentSettingsType::MEDIASTREAM_CAMERA:
      case ContentSettingsType::MIDI_SYSEX:
      case ContentSettingsType::NOTIFICATIONS:
      case ContentSettingsType::GEOLOCATION:
      case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:  // ChromeOS only.
      case ContentSettingsType::PPAPI_BROKER:
      case ContentSettingsType::STORAGE_ACCESS:
        test_api_->AddSimpleRequest(source_frame, it->type);
        break;
      case ContentSettingsType::DEFAULT:
        // Permissions to request for a "multiple" request. Only mic/camera
        // requests are grouped together.
        EXPECT_EQ(kMultipleName, name);
        test_api_->AddSimpleRequest(source_frame,
                                    ContentSettingsType::MEDIASTREAM_MIC);
        test_api_->AddSimpleRequest(source_frame,
                                    ContentSettingsType::MEDIASTREAM_CAMERA);
        break;
      default:
        ADD_FAILURE() << "Not a permission type, or one that doesn't prompt.";
        return;
    }
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<test::PermissionRequestManagerTestApi> test_api_;
};

IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest,
                       AlertAccessibleEvent) {
  views::test::AXEventCounter counter(views::AXEventManager::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));
  ShowUi("geolocation");

// AnnounceText is called when permission requests are announced. But on Mac,
// AnnounceText doesn't go through the path that uses Event::kAlert. Therefore
// we can't test it.
#if !defined(OS_MAC)
  PermissionChip* chip = GetPermissionRequestChipView();
  // If chip UI is used, two notifications will be announced: one that
  // permission was requested and second when bubble is opened.
  if (chip && !chip->should_start_open_for_testing()) {
    EXPECT_EQ(2, counter.GetCount(ax::mojom::Event::kAlert));
  } else {
    EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kAlert));
  }
#else
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kAlert));
#endif
}

// Test bubbles showing when tabs move between windows. Simulates a situation
// that could result in permission bubbles not being dismissed, and a problem
// referencing a temporary drag window. See http://crbug.com/754552.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest,
                       SwitchBrowserWindow) {
  ShowUi("geolocation");
  TabStripModel* strip = browser()->tab_strip_model();

  // Drag out into a dragging window. E.g. see steps in [BrowserWindowController
  // detachTabsToNewWindow:..].
  std::vector<TabStripModelDelegate::NewStripContents> contentses(1);
  contentses.back().add_types = TabStripModel::ADD_ACTIVE;
  contentses.back().web_contents = strip->DetachWebContentsAt(0);
  Browser* dragging_browser = strip->delegate()->CreateNewStripWithContents(
      std::move(contentses), gfx::Rect(100, 100, 640, 480), false);

  // Attach the tab back to the original window. E.g. See steps in
  // [BrowserWindowController moveTabViews:..].
  TabStripModel* drag_strip = dragging_browser->tab_strip_model();
  std::unique_ptr<content::WebContents> removed_contents =
      drag_strip->DetachWebContentsAt(0);
  strip->InsertWebContentsAt(0, std::move(removed_contents),
                             TabStripModel::ADD_ACTIVE);

  // Clear the request. There should be no crash.
  test_api_->SimulateWebContentsDestroyed();
}

// crbug.com/989858
#if defined(OS_WIN)
#define MAYBE_ActiveTabClosedAfterRendererCrashesWithPendingPermissionRequest \
  DISABLED_ActiveTabClosedAfterRendererCrashesWithPendingPermissionRequest
#else
#define MAYBE_ActiveTabClosedAfterRendererCrashesWithPendingPermissionRequest \
  ActiveTabClosedAfterRendererCrashesWithPendingPermissionRequest
#endif
// Regression test for https://crbug.com/933321.
IN_PROC_BROWSER_TEST_P(
    PermissionPromptBubbleViewBrowserTest,
    MAYBE_ActiveTabClosedAfterRendererCrashesWithPendingPermissionRequest) {
  ShowUi("geolocation");
  ASSERT_TRUE(VerifyUi());

  // Simulate a render process crash while the permission prompt is pending.
  content::RenderViewHost* render_view_host = browser()
                                                  ->tab_strip_model()
                                                  ->GetActiveWebContents()
                                                  ->GetMainFrame()
                                                  ->GetRenderViewHost();
  content::RenderProcessHost* render_process_host =
      render_view_host->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      render_process_host,
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  ASSERT_TRUE(render_process_host->Shutdown(0));
  crash_observer.Wait();

  // The permission request is still pending, but the BrowserView's WebView is
  // now showing a crash overlay, so the permission prompt is hidden.
  //
  // Now close the tab. This will first detach the WebContents, causing the
  // WebView's crash overlay to be torn down, which, in turn, will temporarily
  // make the dying WebContents visible again, albeit without being attached to
  // any BrowserView.
  //
  // Wait until the WebContents, and with it, the PermissionRequestManager, is
  // gone, and make sure nothing crashes.
  content::WebContentsDestroyedWatcher web_contents_destroyed_watcher(
      browser()->tab_strip_model()->GetActiveWebContents());
  browser()->tab_strip_model()->CloseAllTabs();
  web_contents_destroyed_watcher.Wait();
}

// Host wants to know your location.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest,
                       InvokeUi_geolocation) {
  ShowAndVerifyUi();
}

// Host wants to show notifications.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest,
                       InvokeUi_notifications) {
  ShowAndVerifyUi();
}

// Host wants to use your microphone.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest, InvokeUi_mic) {
  ShowAndVerifyUi();
}

// Host wants to use your camera.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest, InvokeUi_camera) {
  ShowAndVerifyUi();
}

// Host wants to open email links.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest,
                       InvokeUi_protocol_handlers) {
  ShowAndVerifyUi();
}

// Host wants to use your MIDI devices.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest, InvokeUi_midi) {
  ShowAndVerifyUi();
}

// Host wants to access storage from the site in which it's embedded.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest,
                       InvokeUi_storage_access) {
  ShowAndVerifyUi();
}

// Host wants to trigger multiple downloads.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest,
                       InvokeUi_downloads) {
  ShowAndVerifyUi();
}

// Host wants to access data about your security key.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest,
                       InvokeUi_security_key) {
  ShowAndVerifyUi();
}

// Shows a permissions bubble with multiple requests.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest,
                       InvokeUi_multiple) {
  ShowAndVerifyUi();
}

class QuietUIPromoBrowserTest : public PermissionPromptBubbleViewBrowserTest {
 public:
  QuietUIPromoBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kQuietNotificationPrompts,
        {{QuietNotificationPermissionUiConfig::kEnableAdaptiveActivation,
          "true"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(QuietUIPromoBrowserTest, InvokeUi_QuietUIPromo) {
  auto* profile = browser()->profile();
  // Promo is not enabled by default.
  EXPECT_FALSE(QuietNotificationPermissionUiState::ShouldShowPromo(profile));

  for (const char* origin_spec :
       {"https://a.com", "https://b.com", "https://c.com"}) {
    GURL requesting_origin(origin_spec);
    ui_test_utils::NavigateToURL(browser(), requesting_origin);
    permissions::MockPermissionRequest notification_request(
        u"request", permissions::RequestType::kNotifications,
        requesting_origin);
    test_api_->manager()->AddRequest(GetActiveMainFrame(),
                                     &notification_request);
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(test_api_->manager()->ShouldCurrentRequestUseQuietUI());
    EXPECT_FALSE(QuietNotificationPermissionUiState::ShouldShowPromo(profile));
    test_api_->manager()->Deny();
    base::RunLoop().RunUntilIdle();
  }

  LocationBarView* location_bar_view =
      BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBarView();
  ContentSettingImageView& quiet_ui_icon = **std::find_if(
      location_bar_view->GetContentSettingViewsForTest().begin(),
      location_bar_view->GetContentSettingViewsForTest().end(),
      [](ContentSettingImageView* view) {
        return view->GetTypeForTesting() ==
               ContentSettingImageModel::ImageType::NOTIFICATIONS_QUIET_PROMPT;
      });

  EXPECT_FALSE(quiet_ui_icon.GetVisible());
  // `ContentSettingImageView::AnimationEnded()` was not triggered and IPH is
  // not shown.
  EXPECT_FALSE(quiet_ui_icon.get_critical_promo_id_for_testing().has_value());

  GURL notification("http://www.notification1.com/");
  ui_test_utils::NavigateToURL(browser(), notification);
  permissions::MockPermissionRequest notification_request(
      u"request", permissions::RequestType::kNotifications, notification);
  test_api_->manager()->AddRequest(GetActiveMainFrame(), &notification_request);
  base::RunLoop().RunUntilIdle();

  // After 3 denied Notifications requests, Adaptive activation enabled quiet
  // permission prompt.
  EXPECT_TRUE(test_api_->manager()->ShouldCurrentRequestUseQuietUI());
  // At the first quiet permission prompt we show IPH.
  ASSERT_TRUE(QuietNotificationPermissionUiState::ShouldShowPromo(profile));

  EXPECT_TRUE(quiet_ui_icon.GetVisible());
  EXPECT_TRUE(quiet_ui_icon.is_animating_label());
  // Animation is reset to trigger `ContentSettingImageView::AnimationEnded()`.
  // `AnimationEnded` contains logic for displaying IPH and marking it as shown.
  quiet_ui_icon.reset_animation_for_testing();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(quiet_ui_icon.is_animating_label());

  // The IPH is showing.
  ASSERT_TRUE(quiet_ui_icon.get_critical_promo_id_for_testing().has_value());
  FeaturePromoControllerViews* iph_controller =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->feature_promo_controller();
  // The critical promo that is currently showing is the one created by a quiet
  // permission prompt.
  EXPECT_TRUE(iph_controller->CriticalPromoIsShowing(
      quiet_ui_icon.get_critical_promo_id_for_testing().value()));

  iph_controller->CloseBubbleForCriticalPromo(
      quiet_ui_icon.get_critical_promo_id_for_testing().value());

  test_api_->manager()->Deny();
  base::RunLoop().RunUntilIdle();

  // After quiet permission prompt was resolved, the critical promo is reset.
  EXPECT_FALSE(quiet_ui_icon.get_critical_promo_id_for_testing().has_value());

  EXPECT_FALSE(quiet_ui_icon.GetVisible());

  // The second Notifications permission request to verify that the IPH is not
  // shown.
  GURL notification2("http://www.notification2.com/");
  ui_test_utils::NavigateToURL(browser(), notification2);
  permissions::MockPermissionRequest notification_request2(
      u"request", permissions::RequestType::kNotifications, notification2);
  test_api_->manager()->AddRequest(GetActiveMainFrame(),
                                   &notification_request2);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_api_->manager()->ShouldCurrentRequestUseQuietUI());
  // At the second quiet permission prompt the IPH should be disabled.
  EXPECT_FALSE(QuietNotificationPermissionUiState::ShouldShowPromo(profile));

  EXPECT_TRUE(quiet_ui_icon.GetVisible());
  EXPECT_TRUE(quiet_ui_icon.is_animating_label());
  quiet_ui_icon.reset_animation_for_testing();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(quiet_ui_icon.is_animating_label());

  // The IPH id is not empty because `ContentSettingImageView::AnimationEnded()`
  // was triggered.
  EXPECT_TRUE(quiet_ui_icon.get_critical_promo_id_for_testing().has_value());
  // The critical promo is not shown.
  EXPECT_FALSE(iph_controller->CriticalPromoIsShowing(
      quiet_ui_icon.get_critical_promo_id_for_testing().value()));

  test_api_->manager()->Deny();
  base::RunLoop().RunUntilIdle();
}

// ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER is ChromeOS only.
#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest,
                       InvokeUi_protected_media) {
  ShowAndVerifyUi();
}
#endif

INSTANTIATE_TEST_SUITE_P(All,
                         PermissionPromptBubbleViewBrowserTest,
                         ::testing::Values(false, true));

class OneTimePermissionPromptBubbleViewBrowserTest
    : public PermissionPromptBubbleViewBrowserTest {
 public:
  OneTimePermissionPromptBubbleViewBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        permissions::features::kOneTimeGeolocationPermission,
        {{"OkButtonBehavesAsAllowAlways", GetParam() ? "true" : "false"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(OneTimePermissionPromptBubbleViewBrowserTest,
                       InvokeUi_geolocation) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All,
                         OneTimePermissionPromptBubbleViewBrowserTest,
                         ::testing::Values(false, true));

INSTANTIATE_TEST_SUITE_P(All, QuietUIPromoBrowserTest, ::testing::Values(true));
