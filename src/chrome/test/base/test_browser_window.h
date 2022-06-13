// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TEST_BROWSER_WINDOW_H_
#define CHROME_TEST_BASE_TEST_BROWSER_WINDOW_H_

#include <memory>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/test_download_shelf.h"
#include "chrome/browser/ui/autofill/test/test_autofill_bubble_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/common/buildflags.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#endif  //  !defined(OS_ANDROID)

class FeaturePromoController;
class LocationBarTesting;
class OmniboxView;

namespace qrcode_generator {
class QRCodeGeneratorBubbleController;
class QRCodeGeneratorBubbleView;
}  // namespace qrcode_generator

namespace send_tab_to_self {
class SendTabToSelfBubbleController;
class SendTabToSelfBubbleView;
}  // namespace send_tab_to_self

namespace sharing_hub {
class SharingHubBubbleController;
class SharingHubBubbleView;
}  // namespace sharing_hub

// An implementation of BrowserWindow used for testing. TestBrowserWindow only
// contains a valid LocationBar, all other getters return NULL.
// However, some of them can be preset to a specific value.
// See BrowserWithTestWindowTest for an example of using this class.
class TestBrowserWindow : public BrowserWindow {
 public:
  TestBrowserWindow();
  TestBrowserWindow(const TestBrowserWindow&) = delete;
  TestBrowserWindow& operator=(const TestBrowserWindow&) = delete;
  ~TestBrowserWindow() override;

  // BrowserWindow:
  void Show() override {}
  void ShowInactive() override {}
  void Hide() override {}
  bool IsVisible() const override;
  void SetBounds(const gfx::Rect& bounds) override {}
  void Close() override;
  void Activate() override {}
  void Deactivate() override {}
  bool IsActive() const override;
  void FlashFrame(bool flash) override {}
  ui::ZOrderLevel GetZOrderLevel() const override;
  void SetZOrderLevel(ui::ZOrderLevel order) override {}
  gfx::NativeWindow GetNativeWindow() const override;
  bool IsOnCurrentWorkspace() const override;
  void SetTopControlsShownRatio(content::WebContents* web_contents,
                                float ratio) override;
  bool DoBrowserControlsShrinkRendererSize(
      const content::WebContents* contents) const override;
  ui::NativeTheme* GetNativeTheme() override;
  const ui::ColorProvider* GetColorProvider() const override;
  int GetTopControlsHeight() const override;
  void SetTopControlsGestureScrollInProgress(bool in_progress) override;
  StatusBubble* GetStatusBubble() override;
  void UpdateTitleBar() override {}
  void UpdateFrameColor() override {}
  void BookmarkBarStateChanged(
      BookmarkBar::AnimateChangeType change_type) override {}
  void UpdateDevTools() override {}
  void UpdateLoadingAnimations(bool should_animate) override {}
  void SetStarredState(bool is_starred) override {}
  void SetTranslateIconToggled(bool is_lit) override {}
  void OnActiveTabChanged(content::WebContents* old_contents,
                          content::WebContents* new_contents,
                          int index,
                          int reason) override {}
  void OnTabDetached(content::WebContents* contents, bool was_active) override {
  }
  void OnTabRestored(int command_id) override {}
  void ZoomChangedForActiveTab(bool can_show_bubble) override {}
  gfx::Rect GetRestoredBounds() const override;
  ui::WindowShowState GetRestoredState() const override;
  gfx::Rect GetBounds() const override;
  gfx::Size GetContentsSize() const override;
  void SetContentsSize(const gfx::Size& size) override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  void Maximize() override {}
  void Minimize() override {}
  void Restore() override {}
  bool ShouldHideUIForFullscreen() const override;
  bool IsFullscreen() const override;
  bool IsFullscreenBubbleVisible() const override;
  LocationBar* GetLocationBar() const override;
  void UpdatePageActionIcon(PageActionIconType type) override {}
  autofill::AutofillBubbleHandler* GetAutofillBubbleHandler() override;
  void ExecutePageActionIconForTesting(PageActionIconType type) override {}
  void SetFocusToLocationBar(bool is_user_initiated) override {}
  void UpdateReloadStopState(bool is_loading, bool force) override {}
  void UpdateToolbar(content::WebContents* contents) override {}
  void UpdateCustomTabBarVisibility(bool visible, bool animate) override {}
  void ResetToolbarTabState(content::WebContents* contents) override {}
  void FocusToolbar() override {}
  ExtensionsContainer* GetExtensionsContainer() override;
  void ToolbarSizeChanged(bool is_animating) override {}
  void TabDraggingStatusChanged(bool is_dragging) override {}
  void LinkOpeningFromGesture(WindowOpenDisposition disposition) override {}
  void FocusAppMenu() override {}
  void FocusBookmarksToolbar() override {}
  void FocusInactivePopupForAccessibility() override {}
  void RotatePaneFocus(bool forwards) override {}
  void FocusWebContentsPane() override {}
  void ShowAppMenu() override {}
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  bool IsBookmarkBarVisible() const override;
  bool IsBookmarkBarAnimating() const override;
  bool IsTabStripEditable() const override;
  void SetIsTabStripEditable(bool is_editable);
  bool IsToolbarVisible() const override;
  bool IsToolbarShowing() const override;
  SharingDialog* ShowSharingDialog(content::WebContents* contents,
                                   SharingDialogData data) override;
  void ShowUpdateChromeDialog() override {}
  void ShowBookmarkBubble(const GURL& url, bool already_bookmarked) override {}
  qrcode_generator::QRCodeGeneratorBubbleView* ShowQRCodeGeneratorBubble(
      content::WebContents* contents,
      qrcode_generator::QRCodeGeneratorBubbleController* controller,
      const GURL& url,
      bool show_back_button) override;
#if !defined(OS_ANDROID)
  sharing_hub::ScreenshotCapturedBubble* ShowScreenshotCapturedBubble(
      content::WebContents* contents,
      const gfx::Image& image,
      sharing_hub::ScreenshotCapturedBubbleController* controller) override;
  void ShowIntentPickerBubble(
      std::vector<apps::IntentPickerAppInfo> app_info,
      bool show_stay_in_chrome,
      bool show_remember_selection,
      PageActionIconType icon_type,
      const absl::optional<url::Origin>& initiating_origin,
      IntentPickerResponse callback) override {}
#endif  //  !define(OS_ANDROID)
  send_tab_to_self::SendTabToSelfBubbleView* ShowSendTabToSelfBubble(
      content::WebContents* contents,
      send_tab_to_self::SendTabToSelfBubbleController* controller,
      bool is_user_gesture) override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  views::Button* GetSharingHubIconButton() override;
#else
  sharing_hub::SharingHubBubbleView* ShowSharingHubBubble(
      content::WebContents* contents,
      sharing_hub::SharingHubBubbleController* controller,
      bool is_user_gesture) override;
#endif
  ShowTranslateBubbleResult ShowTranslateBubble(
      content::WebContents* contents,
      translate::TranslateStep step,
      const std::string& source_language,
      const std::string& target_language,
      translate::TranslateErrors::Type error_type,
      bool is_user_gesture) override;
#if BUILDFLAG(ENABLE_ONE_CLICK_SIGNIN)
  void ShowOneClickSigninConfirmation(
      const std::u16string& email,
      base::OnceCallback<void(bool)> confirmed_callback) override {}
#endif
  bool IsDownloadShelfVisible() const override;
  DownloadShelf* GetDownloadShelf() override;
  void ConfirmBrowserCloseWithPendingDownloads(
      int download_count,
      Browser::DownloadCloseType dialog_type,
      base::OnceCallback<void(bool)> callback) override {}
  void UserChangedTheme(BrowserThemeChangeType theme_change_type) override {}
  void CutCopyPaste(int command_id) override {}
  std::unique_ptr<FindBar> CreateFindBar() override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;
  void ShowAvatarBubbleFromAvatarButton(
      AvatarBubbleMode mode,
      signin_metrics::AccessPoint access_point,
      bool is_source_keyboard) override {}
  void MaybeShowProfileSwitchIPH() override {}

#if defined(OS_CHROMEOS) || defined(OS_MAC) || defined(OS_WIN) || \
    defined(OS_LINUX) || defined(OS_FUCHSIA)
  void ShowHatsDialog(
      const std::string& site_id,
      base::OnceClosure success_callback,
      base::OnceClosure failure_callback,
      const SurveyBitsData& product_specific_bits_data,
      const SurveyStringData& product_specific_string_data) override {}

  void ShowIncognitoClearBrowsingDataDialog() override {}
  void ShowIncognitoHistoryDisclaimerDialog() override {}
#endif

  ExclusiveAccessContext* GetExclusiveAccessContext() override;
  std::string GetWorkspace() const override;
  bool IsVisibleOnAllWorkspaces() const override;
  void ShowEmojiPanel() override {}
  void ShowCaretBrowsingDialog() override {}
  std::unique_ptr<content::EyeDropper> OpenEyeDropper(
      content::RenderFrameHost* frame,
      content::EyeDropperListener* listener) override;

  void SetNativeWindow(gfx::NativeWindow window);

  void SetCloseCallback(base::OnceClosure close_callback);

  void CreateTabSearchBubble() override {}
  void CloseTabSearchBubble() override {}

#if BUILDFLAG(ENABLE_SIDE_SEARCH)
  bool IsSideSearchPanelVisible() const override;
  void MaybeRestoreSideSearchStatePerWindow(
      const std::map<std::string, std::string>& extra_data) override;
#endif

  FeaturePromoController* GetFeaturePromoController() override;

  // Sets the controller returned by GetFeaturePromoController().
  // Deletes the existing one, if any.
  FeaturePromoController* SetFeaturePromoController(
      std::unique_ptr<FeaturePromoController> feature_promo_controller);

  void set_workspace(std::string workspace) { workspace_ = workspace; }
  void set_visible_on_all_workspaces(bool visible_on_all_workspaces) {
    visible_on_all_workspaces_ = visible_on_all_workspaces;
  }
  void set_is_active(bool active) { is_active_ = active; }
  void set_is_minimized(bool minimized) { is_minimized_ = minimized; }

 protected:
  void DestroyBrowser() override {}

 private:
  class TestLocationBar : public LocationBar {
   public:
    TestLocationBar() = default;
    TestLocationBar(const TestLocationBar&) = delete;
    TestLocationBar& operator=(const TestLocationBar&) = delete;
    ~TestLocationBar() override = default;

    // LocationBar:
    GURL GetDestinationURL() const override;
    bool IsInputTypedUrlWithoutScheme() const override;
    WindowOpenDisposition GetWindowOpenDisposition() const override;
    ui::PageTransition GetPageTransition() const override;
    base::TimeTicks GetMatchSelectionTimestamp() const override;
    void AcceptInput() override {}
    void AcceptInput(base::TimeTicks match_selection_timestamp) override {}
    void FocusLocation(bool select_all) override {}
    void FocusSearch() override {}
    void UpdateContentSettingsIcons() override {}
    void SaveStateToContents(content::WebContents* contents) override {}
    void Revert() override {}
    const OmniboxView* GetOmniboxView() const override;
    OmniboxView* GetOmniboxView() override;
    LocationBarTesting* GetLocationBarForTesting() override;
  };

  autofill::TestAutofillBubbleHandler autofill_bubble_handler_;
  TestDownloadShelf download_shelf_{nullptr};
  TestLocationBar location_bar_;
  gfx::NativeWindow native_window_ = nullptr;

  std::string workspace_;
  bool visible_on_all_workspaces_ = false;
  bool is_minimized_ = false;
  bool is_active_ = false;
  bool is_tab_strip_editable_ = true;

  std::unique_ptr<FeaturePromoController> feature_promo_controller_;

  base::OnceClosure close_callback_;
};

// Handles destroying a TestBrowserWindow when the Browser it is attached to is
// destroyed.
class TestBrowserWindowOwner : public BrowserListObserver {
 public:
  explicit TestBrowserWindowOwner(TestBrowserWindow* window);
  TestBrowserWindowOwner(const TestBrowserWindowOwner&) = delete;
  TestBrowserWindowOwner& operator=(const TestBrowserWindowOwner&) = delete;
  ~TestBrowserWindowOwner() override;

 private:
  // Overridden from BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;
  std::unique_ptr<TestBrowserWindow> window_;
};

// Helper that handle the lifetime of TestBrowserWindow instances.
std::unique_ptr<Browser> CreateBrowserWithTestWindowForParams(
    Browser::CreateParams params);

#endif  // CHROME_TEST_BASE_TEST_BROWSER_WINDOW_H_
