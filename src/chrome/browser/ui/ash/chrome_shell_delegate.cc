// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include <memory>
#include <utility>

#include "ash/constants/app_types.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/command_line.h"
#include "cc/input/touch_action.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/assistant/assistant_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/nearby_sharing/nearby_share_delegate_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/ash/back_gesture_contextual_nudge_delegate.h"
#include "chrome/browser/ui/ash/capture_mode/chrome_capture_mode_delegate.h"
#include "chrome/browser/ui/ash/chrome_accessibility_delegate.h"
#include "chrome/browser/ui/ash/desks_templates/chrome_desks_templates_delegate.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_ui.h"
#include "chrome/browser/ui/ash/session_util.h"
#include "chrome/browser/ui/ash/window_pin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_scrubber_chromeos.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_layout.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_util.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_service.h"
#include "components/ui_devtools/devtools_server.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "extensions/common/constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "url/gurl.h"

namespace {

const char kKeyboardShortcutHelpPageUrl[] =
    "https://support.google.com/chromebook/answer/183101";

// Browser tests are always started with --disable-logging-redirect, so we need
// independent option here.
absl::optional<bool> disable_logging_redirect_for_testing;

// Returns the TabStripModel that associates with |window| if the given |window|
// contains a browser frame, otherwise returns nullptr.
TabStripModel* GetTabstripModelForWindowIfAny(aura::Window* window) {
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForNativeWindow(window);
  return browser_view ? browser_view->browser()->tab_strip_model() : nullptr;
}

content::WebContents* GetActiveWebContentsForNativeBrowserWindow(
    gfx::NativeWindow window) {
  if (!window)
    return nullptr;

  TabStripModel* tab_strip_model = GetTabstripModelForWindowIfAny(window);
  return tab_strip_model ? tab_strip_model->GetActiveWebContents() : nullptr;
}

}  // namespace

ChromeShellDelegate::ChromeShellDelegate() = default;

ChromeShellDelegate::~ChromeShellDelegate() = default;

bool ChromeShellDelegate::CanShowWindowForUser(
    const aura::Window* window) const {
  return ::CanShowWindowForUser(window,
                                base::BindRepeating(&GetActiveBrowserContext));
}

std::unique_ptr<ash::CaptureModeDelegate>
ChromeShellDelegate::CreateCaptureModeDelegate() const {
  return std::make_unique<ChromeCaptureModeDelegate>();
}

void ChromeShellDelegate::OpenKeyboardShortcutHelpPage() const {
  chrome::ScopedTabbedBrowserDisplayer scoped_tabbed_browser_displayer(
      ProfileManager::GetActiveUserProfile());
  NavigateParams params(scoped_tabbed_browser_displayer.browser(),
                        GURL(kKeyboardShortcutHelpPageUrl),
                        ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  Navigate(&params);
}

bool ChromeShellDelegate::CanGoBack(gfx::NativeWindow window) const {
  content::WebContents* contents =
      GetActiveWebContentsForNativeBrowserWindow(window);
  return contents ? contents->GetController().CanGoBack() : false;
}

void ChromeShellDelegate::SetTabScrubberChromeOSEnabled(bool enabled) {
  TabScrubberChromeOS::GetInstance()->SetEnabled(enabled);
}

bool ChromeShellDelegate::AllowDefaultTouchActions(gfx::NativeWindow window) {
  content::WebContents* contents =
      GetActiveWebContentsForNativeBrowserWindow(window);
  if (!contents)
    return true;
  content::RenderWidgetHostView* render_widget_host_view =
      contents->GetRenderWidgetHostView();
  if (!render_widget_host_view)
    return true;
  content::RenderWidgetHost* render_widget_host =
      render_widget_host_view->GetRenderWidgetHost();
  if (!render_widget_host)
    return true;
  absl::optional<cc::TouchAction> allowed_touch_action =
      render_widget_host->GetAllowedTouchAction();
  return allowed_touch_action.has_value()
             ? *allowed_touch_action != cc::TouchAction::kNone
             : true;
}

bool ChromeShellDelegate::ShouldWaitForTouchPressAck(gfx::NativeWindow window) {
  content::WebContents* contents =
      GetActiveWebContentsForNativeBrowserWindow(window);
  if (!contents)
    return false;
  content::RenderWidgetHostView* render_widget_host_view =
      contents->GetRenderWidgetHostView();
  if (!render_widget_host_view)
    return false;
  return !!render_widget_host_view->GetRenderWidgetHost();
}

bool ChromeShellDelegate::IsTabDrag(const ui::OSExchangeData& drop_data) {
  DCHECK(ash::features::IsWebUITabStripTabDragIntegrationEnabled());
  return tab_strip_ui::IsDraggedTab(drop_data);
}

int ChromeShellDelegate::GetBrowserWebUITabStripHeight() {
  DCHECK(ash::features::IsWebUITabStripTabDragIntegrationEnabled());
  return TabStripUILayout::GetContainerHeight();
}

void ChromeShellDelegate::BindBluetoothSystemFactory(
    mojo::PendingReceiver<device::mojom::BluetoothSystemFactory> receiver) {
  content::GetDeviceService().BindBluetoothSystemFactory(std::move(receiver));
}

void ChromeShellDelegate::BindFingerprint(
    mojo::PendingReceiver<device::mojom::Fingerprint> receiver) {
  content::GetDeviceService().BindFingerprint(std::move(receiver));
}

void ChromeShellDelegate::BindMultiDeviceSetup(
    mojo::PendingReceiver<chromeos::multidevice_setup::mojom::MultiDeviceSetup>
        receiver) {
  chromeos::multidevice_setup::MultiDeviceSetupService* service =
      chromeos::multidevice_setup::MultiDeviceSetupServiceFactory::
          GetForProfile(ProfileManager::GetPrimaryUserProfile());
  if (service)
    service->BindMultiDeviceSetup(std::move(receiver));
}

media_session::MediaSessionService*
ChromeShellDelegate::GetMediaSessionService() {
  return &content::GetMediaSessionService();
}

ash::AccessibilityDelegate* ChromeShellDelegate::CreateAccessibilityDelegate() {
  return new ChromeAccessibilityDelegate;
}

std::unique_ptr<ash::BackGestureContextualNudgeDelegate>
ChromeShellDelegate::CreateBackGestureContextualNudgeDelegate(
    ash::BackGestureContextualNudgeController* controller) {
  return std::make_unique<BackGestureContextualNudgeDelegate>(controller);
}

std::unique_ptr<ash::NearbyShareDelegate>
ChromeShellDelegate::CreateNearbyShareDelegate(
    ash::NearbyShareController* controller) const {
  return std::make_unique<NearbyShareDelegateImpl>(controller);
}

std::unique_ptr<ash::DesksTemplatesDelegate>
ChromeShellDelegate::CreateDesksTemplatesDelegate() const {
  return std::make_unique<ChromeDesksTemplatesDelegate>();
}

bool ChromeShellDelegate::IsSessionRestoreInProgress() const {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  return SessionRestore::IsRestoring(profile);
}

void ChromeShellDelegate::SetUpEnvironmentForLockedFullscreen(bool locked) {
  // Reset the clipboard and kill dev tools when entering or exiting locked
  // fullscreen (security concerns).
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);
  content::DevToolsAgentHost::DetachAllClients();

  // TODO(crbug/1243104): This might be interesting for DLP to change.
  // Disable both screenshots and video screen captures via the capture mode
  // feature.
  ChromeCaptureModeDelegate::Get()->SetIsScreenCaptureLocked(locked);

  // Get the primary profile as that's what ARC and Assistant are attached to.
  const Profile* profile = ProfileManager::GetPrimaryUserProfile();
  // Commands below require profile.
  if (!profile) {
    return;
  }

  // Disable ARC while in the locked fullscreen mode.
  arc::ArcSessionManager* const arc_session_manager =
      arc::ArcSessionManager::Get();
  if (arc_session_manager && arc::IsArcAllowedForProfile(profile)) {
    if (locked) {
      // Disable ARC, preserve data.
      arc_session_manager->RequestDisable();
    } else {
      // Re-enable ARC if needed.
      if (arc::IsArcPlayStoreEnabledForProfile(profile))
        arc_session_manager->RequestEnable();
    }
  }

  if (assistant::IsAssistantAllowedForProfile(profile) ==
      chromeos::assistant::AssistantAllowedState::ALLOWED) {
    ash::AssistantState::Get()->NotifyLockedFullScreenStateChanged(locked);
  }
}

bool ChromeShellDelegate::IsUiDevToolsStarted() const {
  return ChromeBrowserMainExtraPartsViews::Get()->GetUiDevToolsServerInstance();
}

void ChromeShellDelegate::StartUiDevTools() {
  ChromeBrowserMainExtraPartsViews::Get()->CreateUiDevTools();
}

void ChromeShellDelegate::StopUiDevTools() {
  ChromeBrowserMainExtraPartsViews::Get()->DestroyUiDevTools();
}

int ChromeShellDelegate::GetUiDevToolsPort() const {
  return ChromeBrowserMainExtraPartsViews::Get()
      ->GetUiDevToolsServerInstance()
      ->port();
}

bool ChromeShellDelegate::IsLoggingRedirectDisabled() const {
  if (disable_logging_redirect_for_testing.has_value())
    return disable_logging_redirect_for_testing.value();

  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableLoggingRedirect);
}

base::FilePath ChromeShellDelegate::GetPrimaryUserDownloadsFolder() const {
  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user)
    return base::FilePath();

  Profile* user_profile =
      ash::ProfileHelper::Get()->GetProfileByUser(primary_user);
  if (user_profile)
    return file_manager::util::GetDownloadsFolderForProfile(user_profile);

  return base::FilePath();
}

void ChromeShellDelegate::OpenFeedbackPageForPersistentDesksBar() {
  chrome::OpenFeedbackDialog(/*browser=*/nullptr,
                             chrome::kFeedbackSourceBentoBar,
                             /*description_template=*/"#BentoBar\n\n");
}

// static
void ChromeShellDelegate::SetDisableLoggingRedirectForTesting(bool value) {
  disable_logging_redirect_for_testing = value;
}

// static
void ChromeShellDelegate::ResetDisableLoggingRedirectForTesting() {
  disable_logging_redirect_for_testing.reset();
}
