// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {
namespace features {
namespace {

// Controls whether Instant Tethering supports hosts which use the background
// advertisement model.
const base::Feature kInstantTetheringBackgroundAdvertisementSupport{
    "InstantTetheringBackgroundAdvertisementSupport",
    base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace

// Adjusts portrait mode split view to avoid the input field in the bottom
// window being occluded by the virtual keyboard.
const base::Feature kAdjustSplitViewForVK{"AdjustSplitViewForVK",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the UI to support Ambient EQ if the device supports it.
// See https://crbug.com/1021193 for more details.
const base::Feature kAllowAmbientEQ{"AllowAmbientEQ",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether devices are updated before reboot after the first update.
const base::Feature kAllowRepeatedUpdates{"AllowRepeatedUpdates",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Shows settings for adjusting scroll acceleration/sensitivity for
// mouse/touchpad.
const base::Feature kAllowScrollSettings{"AllowScrollSettings",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable Ambient mode feature.
const base::Feature kAmbientModeFeature{"ChromeOSAmbientMode",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

constexpr base::FeatureParam<bool> kAmbientModeCapturedOnPixelAlbumEnabled{
    &kAmbientModeFeature, "CapturedOnPixelAlbumEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModeCapturedOnPixelPhotosEnabled{
    &kAmbientModeFeature, "CapturedOnPixelPhotosEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModeCulturalInstitutePhotosEnabled{
    &kAmbientModeFeature, "CulturalInstitutePhotosEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModeDefaultFeedEnabled{
    &kAmbientModeFeature, "DefaultFeedEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModeEarthAndSpaceAlbumEnabled{
    &kAmbientModeFeature, "EarthAndSpaceAlbumEnabled", true};

constexpr base::FeatureParam<bool> kAmbientModeFeaturedPhotoAlbumEnabled{
    &kAmbientModeFeature, "FeaturedPhotoAlbumEnabled", true};

constexpr base::FeatureParam<bool> kAmbientModeFeaturedPhotosEnabled{
    &kAmbientModeFeature, "FeaturedPhotosEnabled", true};

constexpr base::FeatureParam<bool> kAmbientModeFineArtAlbumEnabled{
    &kAmbientModeFeature, "FineArtAlbumEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModeGeoPhotosEnabled{
    &kAmbientModeFeature, "GeoPhotosEnabled", true};

constexpr base::FeatureParam<bool> kAmbientModePersonalPhotosEnabled{
    &kAmbientModeFeature, "PersonalPhotosEnabled", true};

constexpr base::FeatureParam<bool> kAmbientModeRssPhotosEnabled{
    &kAmbientModeFeature, "RssPhotosEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModeStreetArtAlbumEnabled{
    &kAmbientModeFeature, "StreetArtAlbumEnabled", false};

// Controls whether to launch the animated screensaver (as opposed to the
// existing photo slideshow) when entering ambient mode. Currently, there is
// only one animation theme available ("feel the breeze"), but a FeatureParam
// will be introduced that allows the user to select a specific theme when more
// become available.
const base::Feature kAmbientModeAnimationFeature{
    "ChromeOSAmbientModeAnimation", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to allow Dev channel to use Prod server feature.
const base::Feature kAmbientModeDevUseProdFeature{
    "ChromeOSAmbientModeDevChannelUseProdServer",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable Ambient mode album selection with photo previews.
const base::Feature kAmbientModePhotoPreviewFeature{
    "ChromeOSAmbientModePhotoPreview", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to fetch ambient mode images using new url format.
const base::Feature kAmbientModeNewUrl{"ChromeOSAmbientModeNewUrl",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable ARC account restrictions.
const base::Feature kArcAccountRestrictions{"ArcAccountRestrictions",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable ARC ADB sideloading support.
const base::Feature kArcAdbSideloadingFeature{
    "ArcAdbSideloading", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable support for ARC Input Overlay.
const base::Feature kArcInputOverlay{"ArcInputOverlay",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable support for ARC ADB sideloading for managed
// accounts and/or devices.
const base::Feature kArcManagedAdbSideloadingSupport{
    "ArcManagedAdbSideloadingSupport", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable assistive autocorrect.
const base::Feature kAssistAutoCorrect{"AssistAutoCorrect",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable enhanced assistive emoji suggestions.
const base::Feature kAssistEmojiEnhanced{"AssistEmojiEnhanced",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable assistive multi word suggestions.
const base::Feature kAssistMultiWord{"AssistMultiWord",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable assistive multi word suggestions on an expanded
// list of surfaces.
const base::Feature kAssistMultiWordExpanded{"AssistMultiWordExpanded",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable lacros support for the assistive multi word
// suggestions feature.
const base::Feature kAssistMultiWordLacrosSupport{
    "AssistMultiWordLacrosSupport", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable assistive personal information.
const base::Feature kAssistPersonalInfo{"AssistPersonalInfo",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to suggest addresses in assistive personal information. This
// is only effective when AssistPersonalInfo flag is enabled.
const base::Feature kAssistPersonalInfoAddress{
    "AssistPersonalInfoAddress", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to suggest emails in assistive personal information. This is
// only effective when AssistPersonalInfo flag is enabled.
const base::Feature kAssistPersonalInfoEmail{"AssistPersonalInfoEmail",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to suggest names in assistive personal information. This is
// only effective when AssistPersonalInfo flag is enabled.
const base::Feature kAssistPersonalInfoName{"AssistPersonalInfoName",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to suggest phone numbers in assistive personal information.
// This is only effective when AssistPersonalInfo flag is enabled.
const base::Feature kAssistPersonalInfoPhoneNumber{
    "AssistPersonalInfoPhoneNumber", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the Audio URL that is designed to help user debug or troubleshoot
// common issues on ChromeOS.
const base::Feature kAudioUrl{"AudioUrl", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the Auto Night Light feature which sets the default schedule type to
// sunset-to-sunrise until the user changes it to something else. This feature
// is not exposed to the end user, and is enabled only via cros_config for
// certain devices.
const base::Feature kAutoNightLight{"AutoNightLight",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables auto screen-brightness adjustment when ambient light
// changes.
const base::Feature kAutoScreenBrightness{"AutoScreenBrightness",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the persistent desks bar at the top of the screen in clamshell mode
// when there are more than one desk.
const base::Feature kBentoBar{"BentoBar", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the usage of fixed Bluetooth A2DP packet size to improve
// audio performance in noisy environment.
const base::Feature kBluetoothFixA2dpPacketSize{
    "BluetoothFixA2dpPacketSize", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the Chrome OS Bluetooth Revamp, which updates Bluetooth
// system UI and related infrastructure. See https://crbug.com/1010321.
const base::Feature kBluetoothRevamp{"BluetoothRevamp",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Bluetooth WBS microphone be selected as default
// audio input option.
const base::Feature kBluetoothWbsDogfood{"BluetoothWbsDogfood",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Enable Big GL when using Borealis.
const base::Feature kBorealisBigGl{"BorealisBigGl",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// Enable experimental disk management changes for Borealis.
const base::Feature kBorealisDiskManagement{"BorealisDiskManagement",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Force the client to be on its beta version. If not set, the client will be on
// its stable version.
const base::Feature kBorealisForceBetaClient{"BorealisForceBetaClient",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Prevent Borealis' client from exercising ChromeOS integrations, in this mode
// it functions more like the linux client.
const base::Feature kBorealisLinuxMode{"BorealisLinuxMode",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enable TermsOfServiceURL policy for managed users.
// https://crbug.com/1221342
const base::Feature kManagedTermsOfService{"ManagedTermsOfService",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Enable display of button on Arc provisioning failure dialog for network
// tests.
const base::Feature kButtonARCNetworkDiagnostics{
    "ButtonARCNetworkDiagnostics", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable or disable calendar view from the system tray. Also enables the system
// tray to show date in the shelf when the screen is sufficiently large.
const base::Feature kCalendarView{"CalendarView",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable manual crop document page to ChromeOS camera app. The flag
// will be deprecated after feature is fully launched: crbug.com/1259731.
const base::Feature kCameraAppDocumentManualCrop{
    "CameraAppDocumentManualCrop", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether the camera privacy switch toasts and notification should be
// displayed.
const base::Feature kCameraPrivacySwitchNotifications{
    "CameraPrivacySwitchNotifications", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether the selfie camera feature is enabled for Capture Mode.
const base::Feature kCaptureModeSelfieCamera{"CaptureModeSelfieCamera",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, allow eSIM installation bypass the non-cellular internet
// connectivity check.
const base::Feature kCellularBypassESimInstallationConnectivityCheck{
    "CellularBypassESimInstallationConnectivityCheck",
    base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the value of |kCellularUseAttachApn| should have no effect and
// and the LTE attach APN configuration will not be sent to the modem. This
// flag exists because the |kCellularUseAttachApn| flag can be enabled
// by command-line arguments via board overlays which takes precedence over
// server-side field trial config, which may be needed to turn off the Attach
// APN feature.
const base::Feature kCellularForbidAttachApn{"CellularForbidAttachApn",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, send the LTE attach APN configuration to the modem.
const base::Feature kCellularUseAttachApn{"CellularUseAttachApn",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, use external euicc in Cellular Setup and Settings.
const base::Feature kCellularUseExternalEuicc{
    "CellularUseExternalEuicc", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables pasting a few recently copied items in a menu when pressing search +
// v.
const base::Feature kClipboardHistory{"ClipboardHistory",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, a blue new nudge will show on the context menu option for
// clipboard history.
const base::Feature kClipboardHistoryContextMenuNudge{
    "ClipboardHistoryContextMenuNudge", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the clipboard nudge shown prefs will be reset at the start of
// each new user session.
const base::Feature kClipboardHistoryNudgeSessionReset{
    "ClipboardHistoryNudgeSessionReset", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the clipboard history shortcut will appear in screenshot
// notifications.
const base::Feature kClipboardHistoryScreenshotNudge{
    "ClipboardHistoryScreenshotNudge", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables compositing-based throttling to throttle appropriate frame sinks that
// do not need to be refreshed at high fps.
const base::Feature kCompositingBasedThrottling{
    "CompositingBasedThrottling", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables contextual nudges for gesture education.
const base::Feature kContextualNudges{"ContextualNudges",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Crosh System Web App. When enabled, crosh (Chrome OS
// Shell) will run as a tabbed System Web App rather than a normal browser tab.
const base::Feature kCroshSWA{"CroshSWA", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables upgrading the crostini container to debian bullseye.
const base::Feature kCrostiniBullseyeUpgrade{"CrostiniBullseyeUpgrade",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Crostini Disk Resizing.
const base::Feature kCrostiniDiskResizing{"CrostiniDiskResizing",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Crostini GPU support.
// Note that this feature can be overridden by login_manager based on
// whether a per-board build sets the USE virtio_gpu flag.
// Refer to: chromiumos/src/platform2/login_manager/chrome_setup.cc
const base::Feature kCrostiniGpuSupport{"CrostiniGpuSupport",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Force enable recreating the LXD DB at LXD launch.
const base::Feature kCrostiniResetLxdDb{"CrostiniResetLxdDb",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Do we use the default LXD version or try LXD 4?
const base::Feature kCrostiniUseLxd4{"CrostiniUseLxd4",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Use DLC instead of component updater for managing the Termina image if set
// (and component updater instead of DLC if not).
const base::Feature kCrostiniUseDlc{"CrostiniUseDlc",
                                    base::FEATURE_ENABLED_BY_DEFAULT};
// Enables experimental UI creating and managing multiple Crostini containers.
const base::Feature kCrostiniMultiContainer{"CrostiniMultiContainer",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Crostini IME support.
const base::Feature kCrostiniImeSupport{"CrostiniImeSupport",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Crostini Virtual Keyboard support.
const base::Feature kCrostiniVirtualKeyboardSupport{
    "CrostiniVirtualKeyboardSupport", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables always using device-activity-status data to filter
// eligible host phones.
const base::Feature kCryptAuthV2AlwaysUseActiveEligibleHosts{
    "kCryptAuthV2AlwaysUseActiveEligibleHosts",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables using Cryptauth's GetDevicesActivityStatus API.
const base::Feature kCryptAuthV2DeviceActivityStatus{
    "CryptAuthV2DeviceActivityStatus", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables use of the connectivity status from Cryptauth's
// GetDevicesActivityStatus API to sort devices.
const base::Feature kCryptAuthV2DeviceActivityStatusUseConnectivity{
    "CryptAuthV2DeviceActivityStatusUseConnectivity",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables use of last activity time to deduplicate eligible host
// phones in multidevice setup dropdown list. We assume that different copies
// of same device share the same last activity time but different last update
// time.
const base::Feature kCryptAuthV2DedupDeviceLastActivityTime{
    "CryptAuthV2DedupDeviceLastActivityTime", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables the CryptAuth v2 DeviceSync flow. Regardless of this
// flag, v1 DeviceSync will continue to operate until it is disabled via the
// feature flag kDisableCryptAuthV1DeviceSync.
const base::Feature kCryptAuthV2DeviceSync{"CryptAuthV2DeviceSync",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables the CryptAuth v2 Enrollment flow.
const base::Feature kCryptAuthV2Enrollment{"CryptAuthV2Enrollment",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the Cryptohome recovery feature, which allows users to recover access
// to their profile and Cryptohome after performing an online authentication.
const base::Feature kCryptohomeRecoveryFlow{"CryptohomeRecoveryFlow",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDemoModeSWA{"DemoModeSWA",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Sync for desk templates on Chrome OS.
const base::Feature kDeskTemplateSync{"DeskTemplateSync",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kDesksTemplates{"DesksTemplates",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the Diagnostics app.
const base::Feature kDiagnosticsApp{"DiagnosticsApp",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, the navigation panel will be shown in the diagnostics app.
const base::Feature kDiagnosticsAppNavigation{"DiagnosticsAppNavigation",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Disables the CryptAuth v1 DeviceSync flow. Note: During the first phase
// of the v2 DeviceSync rollout, v1 and v2 DeviceSync run in parallel. This flag
// is needed to disable the v1 service during the second phase of the rollout.
// kCryptAuthV2DeviceSync should be enabled before this flag is flipped.
const base::Feature kDisableCryptAuthV1DeviceSync{
    "DisableCryptAuthV1DeviceSync", base::FEATURE_ENABLED_BY_DEFAULT};

// Disable idle sockets closing on memory pressure for NetworkContexts that
// belong to Profiles. It only applies to Profiles because the goal is to
// improve perceived performance of web browsing within the Chrome OS user
// session by avoiding re-estabshing TLS connections that require client
// certificates.
const base::Feature kDisableIdleSocketsCloseOnMemoryPressure{
    "disable_idle_sockets_close_on_memory_pressure",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Disables "Office Editing for Docs, Sheets & Slides" component app so handlers
// won't be registered, making it possible to install another version for
// testing.
const base::Feature kDisableOfficeEditingComponentApp{
    "DisableOfficeEditingComponentApp", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables indicators to hint where displays are connected.
const base::Feature kDisplayAlignAssist{"DisplayAlignAssist",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the docked (a.k.a. picture-in-picture) magnifier.
// TODO(afakhry): Remove this after the feature is fully launched.
// https://crbug.com/709824.
const base::Feature kDockedMagnifier{"DockedMagnifier",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Enables dragging an unpinned open app to pinned app side to pin.
const base::Feature kDragUnpinnedAppToPin{"DragUnpinnedAppToPin",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables dragging and dropping an existing window to new desk in overview.
const base::Feature kDragWindowToNewDesk{"DragWindowToNewDesk",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, DriveFS will be used for Drive sync.
const base::Feature kDriveFs{"DriveFS", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables duplex native messaging between DriveFS and extensions.
const base::Feature kDriveFsBidirectionalNativeMessaging{
    "DriveFsBidirectionalNativeMessaging", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables DriveFS' experimental local files mirroring functionality.
const base::Feature kDriveFsMirroring{"DriveFsMirroring",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the Eche Phone Hub permission onboarding.
const base::Feature kEchePhoneHubPermissionsOnboarding{
    "EchePhoneHubPermissionsOnboarding", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the System Web App (SWA) version of Eche.
const base::Feature kEcheSWA{"EcheSWA", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the naive resize for the Eche window.
const base::Feature kEcheSWAResizing{"EcheSWAResizing",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the Debug Mode of Eche.
const base::Feature kEcheSWADebugMode{"EcheSWADebugMode",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the Eche SWA to run in the background.
const base::Feature kEcheSWAInBackground{"EcheSWAInBackground",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Enables background blur for the app list, shelf, unified system tray,
// autoclick menu, etc. Also enables the AppsGridView mask layer, slower devices
// may have choppier app list animations while in this mode. crbug.com/765292.
const base::Feature kEnableBackgroundBlur{"EnableBackgroundBlur",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables some trials aimed at improving user experiencing when using the
// trackpad to switch desks.
// TODO(https://crbug.com/1191545): Remove this after the feature is launched.
const base::Feature kEnableDesksTrackpadSwipeImprovements{
    "EnableDesksTrackpadSwipeImprovements", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the DNS proxy service providing support split and secure DNS
// for Chrome OS.
const base::Feature kEnableDnsProxy{"EnableDnsProxy",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables displaying additional OpenVPN configuration values on the network
// details page.
const base::Feature kExtendedOpenVpnSettings{"ExtendedOpenVpnSettings",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the device hostname.
const base::Feature kEnableHostnameSetting{"EnableHostnameSetting",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the Wayland idle-inhibit-unstable-v1 protocol is enabled.
// On Ash, it determines whether the idle inhibit manager is bound by the exo
// Wayland server. On Lacros, it determines whether the power save blocker is
// invoked via Ozone Wayland (if enabled) or via crosapi (if disabled).
const base::Feature kEnableIdleInhibit{"EnableIdleInhibit",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the input device cards will be shown in the diagnostics app.
const base::Feature kEnableInputInDiagnosticsApp{
    "EnableInputInDiagnosticsApp", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables noise cancellation UI toggle.
const base::Feature kEnableInputNoiseCancellationUi{
    "EnableInputNoiseCancellationUi", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables keyboard backlight toggle.
const base::Feature kEnableKeyboardBacklightToggle{
    "EnableKeyboardBacklightToggle", base::FEATURE_ENABLED_BY_DEFAULT};

// Login WebUI was always loaded for legacy reasons even when it was not needed.
// When enabled, it will make login WebUI loaded only before showing it.
const base::Feature kEnableLazyLoginWebUILoading{
    "EnableLazyLoginWebUILoading", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables LocalSearchService to be initialized.
const base::Feature kEnableLocalSearchService{"EnableLocalSearchService",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, the networking cards will be shown in the diagnostics app.
const base::Feature kEnableNetworkingInDiagnosticsApp{
    "EnableNetworkingInDiagnosticsApp", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables OAuth support when printing via the IPP protocol.
const base::Feature kEnableOAuthIpp{"EnableOAuthIpp",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the OOBE ChromeVox hint dialog and announcement feature.
const base::Feature kEnableOobeChromeVoxHint{"EnableOobeChromeVoxHint",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Enables Polymer3 for OOBE
const base::Feature kEnableOobePolymer3{"EnableOobePolymer3",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables skipping of network screen.
const base::Feature kEnableOobeNetworkScreenSkip{
    "EnableOobeNetworkScreenSkip", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables toggling Pciguard settings through Settings UI.
const base::Feature kEnablePciguardUi{"EnablePciguardUi",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// Enables showing notification after the password change for SAML users.
const base::Feature kEnableSamlNotificationOnPasswordChangeSuccess{
    "EnableSamlNotificationOnPasswordChangeSuccess",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables SAML re-authentication on the lock screen once the sign-in time
// limit expires.
const base::Feature kEnableSamlReauthenticationOnLockscreen{
    "EnableSamlReauthenticationOnLockScreen", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables WireGuard VPN, if running a compatible kernel.
const base::Feature kEnableWireGuard{"EnableWireGuard",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Enforces Ash extension keep-list. Only the extensions/Chrome apps in the
// keep-list are enabled in Ash.
const base::Feature kEnforceAshExtensionKeeplist{
    "EnforceAshExtensionKeeplist", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables Device End Of Lifetime warning notifications.
const base::Feature kEolWarningNotifications{"EolWarningNotifications",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables enterprise policy control for eSIM cellular networks.
// See https://crbug.com/1231305.
const base::Feature kESimPolicy{"ESimPolicy",
                                base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable bubble showing when an application gains any UI lock.
const base::Feature kExoLockNotification{"ExoLockNotification",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Enable or disable use of ordinal (unaccelerated) motion by Exo clients.
const base::Feature kExoOrdinalMotion{"ExoOrdinalMotion",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable pointer lock for Crostini windows.
const base::Feature kExoPointerLock{"ExoPointerLock",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables policy that controls feature to allow Family Link accounts on school
// owned devices.
const base::Feature kFamilyLinkOnSchoolDevice{"FamilyLinkOnSchoolDevice",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the Fast Pair feature.
const base::Feature kFastPair{"FastPair", base::FEATURE_DISABLED_BY_DEFAULT};

// Allows Fast Pair to use software scanning on devices which don't support
// hardware offloading of BLE scans.
const base::Feature kFastPairSoftwareScanning{
    "FastPairSoftwareScanning", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables mounting various archive formats (in two tiers) in Files App. This
// flag controls the first tier, whose support is very good.
// https://crbug.com/1216245
const base::Feature kFilesArchivemount{"FilesArchivemount",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enables mounting various archive formats (in two tiers) in Files App. This
// flag controls the second tier, whose support is more experimental.
// https://crbug.com/1216245
const base::Feature kFilesArchivemount2{"FilesArchivemount2",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enable the updated banner framework.
// https://crbug.com/1228128
const base::Feature kFilesBannerFramework{"FilesBannerFramework",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enable the simple archive extraction.
// https://crbug.com/953256
const base::Feature kFilesExtractArchive{"FilesExtractArchive",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the System Web App (SWA) version of file manager.
const base::Feature kFilesSWA{"FilesSWA", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables partitioning of removable disks in file manager.
const base::Feature kFilesSinglePartitionFormat{
    "FilesSinglePartitionFormat", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable files app trash.
const base::Feature kFilesTrash{"FilesTrash",
                                base::FEATURE_DISABLED_BY_DEFAULT};

// Enables filters in Files app Recents view.
const base::Feature kFiltersInRecents{"FiltersInRecents",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the firmware updater app.
const base::Feature kFirmwareUpdaterApp = {"FirmwareUpdaterApp",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to allow keeping full screen mode after unlock.
const base::Feature kFullscreenAfterUnlockAllowed = {
    "FullscreenAfterUnlockAllowed", base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, there will be an alert bubble showing up when the device
// returns from low brightness (e.g., sleep, closed cover) without a lock screen
// and the active window is in fullscreen.
// TODO(https://crbug.com/1107185): Remove this after the feature is launched.
const base::Feature kFullscreenAlertBubble{"EnableFullscreenBubble",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enable ChromeOS FuseBox service.
const base::Feature kFuseBox{"FuseBox", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables handle of `closeView` message from Gaia. The message is
// supposed to end the flow.
const base::Feature kGaiaCloseViewMessage{"GaiaCloseViewMessage",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the Gaia reauth endpoint with deleted user customization page.
const base::Feature kGaiaReauthEndpoint{"GaiaReauthEndpoint",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Controls gamepad vibration in Exo.
const base::Feature kGamepadVibration{"ExoGamepadVibration",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enable a D-Bus service for accessing gesture properties.
const base::Feature kGesturePropertiesDBusService{
    "GesturePropertiesDBusService", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables editing with handwriting gestures within the virtual keyboard.
const base::Feature kHandwritingGestureEditing{
    "HandwritingGestureEditing", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables new on-device recognition for legacy handwriting input.
const base::Feature kHandwritingLegacyRecognition{
    "HandwritingLegacyRecognition", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables new on-device recognition for legacy handwriting input in all
// supported languages.
const base::Feature kHandwritingLegacyRecognitionAllLang{
    "HandwritingLegacyRecognitionAllLang", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the Background Page in the help app.
const base::Feature kHelpAppBackgroundPage{"HelpAppBackgroundPage",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the Discover Tab in the help app.
const base::Feature kHelpAppDiscoverTab{"HelpAppDiscoverTab",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables the Help App Discover tab notifications on non-stable
// Chrome OS channels. Used for testing.
const base::Feature kHelpAppDiscoverTabNotificationAllChannels{
    "HelpAppDiscoverTabNotificationAllChannels",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enable showing search results from the help app in the launcher.
const base::Feature kHelpAppLauncherSearch{"HelpAppLauncherSearch",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the flag to synchronize launcher item colors. It is
// in effect only when kLauncherAppSort is enabled.
const base::Feature kLauncherItemColorSync{"LauncherItemColorSync",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enable the search service integration in the Help app.
const base::Feature kHelpAppSearchServiceIntegration{
    "HelpAppSearchServiceIntegration", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables a warning about connecting to hidden WiFi networks.
// https://crbug.com/903908
const base::Feature kHiddenNetworkWarning{"HiddenNetworkWarning",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables hiding of ARC media notifications. If this is enabled, all ARC
// notifications that are of the media type will not be shown. This
// is because they will be replaced by native media session notifications.
// TODO(beccahughes): Remove after launch. (https://crbug.com/897836)
const base::Feature kHideArcMediaNotifications{
    "HideArcMediaNotifications", base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, shelf navigation controls and the overview tray item will be
// removed from the shelf in tablet mode (unless otherwise specified by user
// preferences, or policy).
const base::Feature kHideShelfControlsInTabletMode{
    "HideShelfControlsInTabletMode", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables V2 implementation for visual representation of in-progress items in
// Tote, the productivity feature that aims to reduce context switching by
// enabling users to collect content and transfer or access it later.
const base::Feature kHoldingSpaceInProgressAnimationV2{
    "HoldingSpaceInProgressAnimationV2", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables in-progress downloads integration with the productivity feature that
// aims to reduce context switching by enabling users to collect content and
// transfer or access it later.
const base::Feature kHoldingSpaceInProgressDownloadsIntegration{
    "HoldingSpaceInProgressDownloadsIntegration",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables in-progress downloads notification suppression with the productivity
// feature that aims to reduce context switching by enabling users to collect
// content and transfer or access it later.
const base::Feature kHoldingSpaceInProgressDownloadsNotificationSuppression{
    "HoldingSpaceInProgressNotificationSuppression",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables incognito profile integration with the productivity feature that
// aims to reduce context switching by enabling users to collect content and
// transfer or access it later.
const base::Feature kHoldingSpaceIncognitoProfileIntegration{
    "HoldingSpaceIncognitoProfileIntegration",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether the snooping protection prototype is enabled.
const base::Feature kSnoopingProtection{"SnoopingProtection",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable dark mode support for the Chrome OS virtual keyboard.
const base::Feature kVirtualKeyboardDarkMode{"VirtualKeyboardDarkMode",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, options page for each input method will be opened in ChromeOS
// settings. Otherwise it will be opened in a new web page in Chrome browser.
const base::Feature kImeOptionsInSettings{"ImeOptionsInSettings",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, used to configure the heuristic rules for some advanced IME
// features (e.g. auto-correct).
const base::Feature kImeRuleConfig{"ImeRuleConfig",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable system emoji picker.
const base::Feature kImeSystemEmojiPicker{"SystemEmojiPicker",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enable or disable system emoji picker falling back to clipboard.
const base::Feature kImeSystemEmojiPickerClipboard{
    "SystemEmojiPickerClipboard", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable system emoji picker extension
const base::Feature kImeSystemEmojiPickerExtension{
    "SystemEmojiPickerExtension", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable a new UI for stylus writing on the virtual keyboard
const base::Feature kImeStylusHandwriting{"StylusHandwriting",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables improved keyboard shortcuts for activating desks at specified indices
// and toggling whether a window is assigned to all desks.
const base::Feature kImprovedDesksKeyboardShortcuts{
    "ImprovedDesksKeyboardShortcuts", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Instant Tethering on Chrome OS.
const base::Feature kInstantTethering{"InstantTethering",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// Enables using arrow keys for display arrangement in display settings page.
const base::Feature kKeyboardBasedDisplayArrangementInSettings{
    "KeyboardBasedDisplayArrangementInSettings",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables to use lacros-chrome as a primary web browser on Chrome OS.
// This works only when LacrosSupport below is enabled.
// NOTE: Use crosapi::browser_util::IsLacrosPrimary() instead of checking
// the feature directly. Similar to LacrosSupport, this may not be allowed
// depending on user types and/or policies.
const base::Feature kLacrosPrimary{"LacrosPrimary",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enables "Linux and Chrome OS" support. Allows a Linux version of Chrome
// ("lacros-chrome") to run as a Wayland client with this instance of Chrome
// ("ash-chrome") acting as the Wayland server and window manager.
// NOTE: Use crosapi::browser_util::IsLacrosEnabled() instead of checking the
// feature directly. Lacros is not allowed for certain user types and can be
// disabled by policy. These restrictions will be lifted when Lacros development
// is complete.
const base::Feature kLacrosSupport{"LacrosSupport",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Pretend that profile migration has been completed regardless of the actual
// state. Enabling this will allow users to use lacros without completing
// profile mgiration.
const base::Feature kForceProfileMigrationCompletion{
    "ForceProfileMigrationCompletion", base::FEATURE_DISABLED_BY_DEFAULT};

// Emergency switch to turn off profile migration.
const base::Feature kLacrosProfileMigrationForceOff{
    "LacrosProfileMigrationForceOff", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable this to turn on profile migration for non-googlers. Currently the
// feature is only limited to googlers only.
const base::Feature kLacrosProfileMigrationForAnyUser{
    "LacrosProfileMigrationForAnyUser", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Language Packs for Handwriting Recognition.
// This feature turns on the download of language-specific Handwriting models
// via DLC.
const base::Feature kLanguagePacksHandwriting{
    "LanguagePacksHandwriting", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the second language settings update.
const base::Feature kLanguageSettingsUpdate2{"LanguageSettingsUpdate2",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables sorting app icons shown on the launcher.
const base::Feature kLauncherAppSort{"LauncherAppSort",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Uses short intervals for launcher nudge for testing if enabled.
const base::Feature kLauncherNudgeShortInterval{
    "LauncherNudgeShortInterval", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables new flow for license packaged devices with enterprise license.
const base::Feature kLicensePackagedOobeFlow{"LicensePackagedOobeFlow",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Supports the feature to hide sensitive content in notifications on the lock
// screen. This option is effective when |kLockScreenNotification| is enabled.
const base::Feature kLockScreenHideSensitiveNotificationsSupport{
    "LockScreenHideSensitiveNotificationsSupport",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables inline reply on notifications on the lock screen.
// This option is effective when |kLockScreenNotification| is enabled.
const base::Feature kLockScreenInlineReply{"LockScreenInlineReply",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables notifications on the lock screen.
const base::Feature kLockScreenNotifications{"LockScreenNotifications",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enables lock screen media controls UI and use of media keys on the lock
// screen.
const base::Feature kLockScreenMediaControls{"LockScreenMediaControls",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the redesigned managed device info UI in the system tray.
const base::Feature kManagedDeviceUIRedesign{"ManagedDeviceUIRedesign",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Whether audio files are opened by default in the ChromeOS media app.
const base::Feature kMediaAppHandlesAudio{"MediaAppHandlesAudio",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Whether PDF files are opened by default in the ChromeOS media app.
const base::Feature kMediaAppHandlesPdf{"MediaAppHandlesPdf",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Feature to continuously log PSI memory pressure data to UMA.
const base::Feature kMemoryPressureMetricsDetail{
    "MemoryPressureMetricsDetail", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls how frequently memory pressure is logged
const base::FeatureParam<int> kMemoryPressureMetricsDetailLogPeriod{
    &kMemoryPressureMetricsDetail, "period", 10};

// Enables notification of when a microphone-using app is launched while the
// microphone is muted.
const base::Feature kMicMuteNotifications{"MicMuteNotifications",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable the requirement of a minimum chrome version on the
// device through the policy DeviceMinimumVersion. If the requirement is
// not met and the warning time in the policy has expired, the user is
// restricted from using the session.
const base::Feature kMinimumChromeVersion{"MinimumChromeVersion",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the use of Mojo by Chrome-process code to communicate with Power
// Manager. In order to use mojo, this feature must be turned on and a callsite
// must use PowerManagerMojoClient::Get().
const base::Feature kMojoDBusRelay{"MojoDBusRelay",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enables support for multilingual assistive typing on Chrome OS.
const base::Feature kMultilingualTyping{"MultilingualTyping",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables Nearby Connections to specificy KeepAlive interval and timeout while
// also making the Nearby Connections WebRTC defaults longer.
const base::Feature kNearbyKeepAliveFix{"NearbyKeepAliveFix",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether new Lockscreen reauth layout is shown or not.
const base::Feature kNewLockScreenReauthLayout{
    "NewLockScreenReauthLayout", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the Night Light feature.
const base::Feature kNightLight{"NightLight", base::FEATURE_ENABLED_BY_DEFAULT};

// Enabled notification expansion animation.
const base::Feature kNotificationExpansionAnimation{
    "NotificationExpansionAnimation", base::FEATURE_DISABLED_BY_DEFAULT};

// Shorten notification timeouts to 6 seconds.
const base::Feature kNotificationExperimentalShortTimeouts{
    "NotificationExperimentalShortTimeouts", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables notification scroll bar in UnifiedSystemTray.
const base::Feature kNotificationScrollBar{"NotificationScrollBar",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables notifications to be shown within context menus.
const base::Feature kNotificationsInContextMenu{
    "NotificationsInContextMenu", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables new notifications UI and grouped notifications.
const base::Feature kNotificationsRefresh{"NotificationsRefresh",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable on-device grammar check service.
const base::Feature kOnDeviceGrammarCheck{"OnDeviceGrammarCheck",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Whether the device supports on-device speech recognition.
const base::Feature kOnDeviceSpeechRecognition{
    "OnDeviceSpeechRecognition", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, EULA and ARC Terms of Service screens are skipped and merged
// into Consolidated Consent Screen.
const base::Feature kOobeConsolidatedConsent{"OobeConsolidatedConsent",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the Oobe quick start flow.
const base::Feature kOobeQuickStart{"OobeQuickStart",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the feedback tool new UX on Chrome OS.
// This tool under development will be rolled out via Finch.
// Enabling this flag will use the new feedback tool instead of the current
// tool on CrOS.
const base::Feature kOsFeedback{"OsFeedback",
                                base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, a new App Notifications subpage will appear in CrOS Apps section.
const base::Feature kOsSettingsAppNotificationsPage{
    "OsSettingsAppNotificationsPage", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kOverviewButton{"OverviewButton",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a notification warning users that their Thunderbolt device is not
// supported on their CrOS device.
const base::Feature kPcieBillboardNotification{
    "PcieBillboardNotification", base::FEATURE_DISABLED_BY_DEFAULT};

// Limits the items on the shelf to the ones associated with windows the
// currently active desk.
const base::Feature kPerDeskShelf{"PerDeskShelf",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Allows tablet mode split screen to resize by moving windows instead of
// resizing. This reduces jank on low end devices.
const base::Feature kPerformantSplitViewResizing{
    "PerformantSplitViewResizing", base::FEATURE_DISABLED_BY_DEFAULT};

// Provides a UI for users to customize their wallpapers, screensaver and
// avatars.
const base::Feature kPersonalizationHub{"PersonalizationHub",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Provides a UI for users to view information about their Android phone
// and perform phone-side actions within Chrome OS.
const base::Feature kPhoneHub{"PhoneHub", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the Camera Roll feature in Phone Hub, which allows users to access
// recent photos and videos taken on a connected Android device
const base::Feature kPhoneHubCameraRoll{"PhoneHubCameraRoll",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the incoming/ongoing call notification feature in Phone Hub.
const base::Feature kPhoneHubCallNotification{
    "PhoneHubCallNotification", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the Recent Apps feature in Phone Hub, which allows users to relaunch
// the streamed app.
const base::Feature kPhoneHubRecentApps{"PhoneHubRecentApps",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPinSetupForManagedUsers{"PinSetupForManagedUsers",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Enables rounded corners for the Picture-in-picture window.
const base::Feature kPipRoundedCorners{"PipRoundedCorners",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables the preference of using constant frame rate for camera
// when streaming.
const base::Feature kPreferConstantFrameRate{"PreferConstantFrameRate",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a bubble-based launcher in clamshell mode. Changes the suggestions
// that appear in the launcher in both clamshell and tablet modes. Removes pages
// from the apps grid. This feature was previously named "AppListBubble".
// https://crbug.com/1204551
const base::Feature kProductivityLauncher{"ProductivityLauncher",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables animation in the productivity launcher.
const base::Feature kProductivityLauncherAnimation{
    "ProductivityLauncherAnimation", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable Projector.
const base::Feature kProjector{"Projector", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable Projector for managed users.
const base::Feature kProjectorManagedUser{"ProjectorManagedUser",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable Projector annotator or marker tools.
// The annotator tools are newer and based on the ink library.
// The marker tools are older and based on fast ink.
// We are deprecating the old marker tools in favor of the annotator tools.
const base::Feature kProjectorAnnotator{"ProjectorAnnotator",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the quick dim prototype is enabled.
const base::Feature kQuickDim{"QuickDim", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the Quick Settings Network revamp, which updates Network
// Quick Settings UI and related infrastructure. See https://crbug.com/1169479.
const base::Feature kQuickSettingsNetworkRevamp{
    "QuickSettingsNetworkRevamp", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables fingerprint quick unlock.
const base::Feature kQuickUnlockFingerprint{"QuickUnlockFingerprint",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the PIN auto submit feature is enabled.
const base::Feature kQuickUnlockPinAutosubmit{"QuickUnlockPinAutosubmit",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// TODO(crbug.com/1104164) - Remove this once most
// users have their preferences backfilled.
// Controls whether the PIN auto submit backfill operation should be performed.
const base::Feature kQuickUnlockPinAutosubmitBackfill{
    "QuickUnlockPinAutosubmitBackfill", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables redirect to default IdP without interstitial step.
const base::Feature kRedirectToDefaultIdP{"RedirectToDefaultIdP",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables suppression of Displays notifications other than resolution change.
const base::Feature kReduceDisplayNotifications{
    "ReduceDisplayNotifications", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Release Notes notifications on non-stable Chrome OS
// channels. Used for testing.
const base::Feature kReleaseNotesNotificationAllChannels{
    "ReleaseNotesNotificationAllChannels", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Release Notes suggestion chip on Chrome OS.
const base::Feature kReleaseNotesSuggestionChip{
    "ReleaseNotesSuggestionChip", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Reven Log Source on Chrome OS. This adds hardware
// information to Feedback reports and chrome://system on CloudReady systems.
const base::Feature kRevenLogSource{"RevenLogSource",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, the overivew and desk reverse scrolling behaviors are changed
// and if the user performs the old gestures, a notification or toast will show
// up.
// TODO(https://crbug.com/1107183): Remove this after the feature is launched.
const base::Feature kReverseScrollGestures{"EnableReverseScrollGestures",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the system tray to show more information in larger screen.
const base::Feature kScalableStatusArea{"ScalableStatusArea",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables flatbed multi-page scanning.
const base::Feature kScanAppMultiPageScan{"ScanAppMultiPageScan",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables use of Searchable PDF file type in the Scan app.
const base::Feature kScanAppSearchablePdf{"ScanAppSearchablePdf",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Overrides semantic colors in Chrome OS for easier debugging.
const base::Feature kSemanticColorsDebugOverride{
    "SemanticColorDebugOverride", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables displaying separate network icons for different networks types.
// https://crbug.com/902409
const base::Feature kSeparateNetworkIcons{"SeparateNetworkIcons",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables long kill timeout for session manager daemon. When
// enabled, session manager daemon waits for a longer time (e.g. 12s) for chrome
// to exit before sending SIGABRT. Otherwise, it uses the default time out
// (currently 3s).
const base::Feature kSessionManagerLongKillTimeout{
    "SessionManagerLongKillTimeout", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the session manager daemon will abort the browser if its
// liveness checker detects a hang, i.e. the browser fails to acknowledge and
// respond sufficiently to periodic pings.  IMPORTANT NOTE: the feature name
// here must match exactly the name of the feature in the open-source ChromeOS
// file session_manager_service.cc.
const base::Feature kSessionManagerLivenessCheck{
    "SessionManagerLivenessCheck", base::FEATURE_ENABLED_BY_DEFAULT};

// Removes notifier settings from quick settings view.
const base::Feature kSettingsAppNotificationSettings{
    "SettingsAppNotificationSettings", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables launcher nudge that animates the home button to guide users to open
// the launcher.
const base::Feature kShelfLauncherNudge{"ShelfLauncherNudge",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the shelf party.
const base::Feature kShelfParty{"ShelfParty",
                                base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the new shimless rma flow.
const base::Feature kShimlessRMAFlow{"ShimlessRMAFlow",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables a toggle to enable Bluetooth debug logs.
const base::Feature kShowBluetoothDebugLogToggle{
    "ShowBluetoothDebugLogToggle", base::FEATURE_ENABLED_BY_DEFAULT};

// Shows the Play Store icon in Demo Mode.
const base::Feature kShowPlayInDemoMode{"ShowPlayInDemoMode",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Uses experimental component version for smart dim.
const base::Feature kSmartDimExperimentalComponent{
    "SmartDimExperimentalComponent", base::FEATURE_DISABLED_BY_DEFAULT};

// Replaces Smart Lock UI in lock screen password box with new UI similar to
// fingerprint auth. Adds Smart Lock to "Lock screen and sign-in" section of
// settings.
const base::Feature kSmartLockUIRevamp{"SmartLockUIRevamp",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// This feature:
// - Creates a new "Sync your settings" section in Chrome OS settings
// - Moves app, wallpaper and Wi-Fi sync to OS settings
// - Provides a separate toggle for OS preferences, distinct from browser
//   preferences
// - Makes the OS ModelTypes run in sync transport mode, controlled by a single
//   pref for the entire OS sync feature
const base::Feature kSyncSettingsCategorization{
    "SyncSettingsCategorization", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables battery indicator for styluses in the palette tray
const base::Feature kStylusBatteryStatus{"StylusBatteryStatus",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables using the system input engine for physical typing in
// Chinese.
const base::Feature kSystemChinesePhysicalTyping{
    "SystemChinesePhysicalTyping", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the System Extensions platform.
const base::Feature kSystemExtensions{"SystemExtensions",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables using the system input engine for physical typing in
// Japanese.
const base::Feature kSystemJapanesePhysicalTyping{
    "SystemJapanesePhysicalTyping", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables using the system input engine for physical typing in
// Korean.
const base::Feature kSystemKoreanPhysicalTyping{
    "SystemKoreanPhysicalTyping", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the Chrome OS system-proxy daemon, only for system services. This
// means that system services like tlsdate, update engine etc. can opt to be
// authenticated to a remote HTTP web proxy via system-proxy.
const base::Feature kSystemProxyForSystemServices{
    "SystemProxyForSystemServices", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the UI to show tab cluster info.
const base::Feature kTabClusterUI{"TabClusterUI",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Enables Chrome OS Telemetry Extension.
const base::Feature kTelemetryExtension{"TelemetryExtension",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables SSH tabs in the Terminal System App.
const base::Feature kTerminalSSH{"TerminalSSH",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

// Enables tmux integration in the Terminal System App.
const base::Feature kTerminalTmuxIntegration{"TerminalTmuxIntegration",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the Settings UI to show data usage for cellular networks.
const base::Feature kTrafficCountersSettingsUi{
    "TrafficCountersSettingsUi", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables trilinear filtering.
const base::Feature kTrilinearFiltering{"TrilinearFiltering",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Unblock the UsbPeripheralNotificationController class in ash to display
// USB related notifications from the type-c daemon in Chrome OS.
const base::Feature kUsbNotificationController{
    "UsbNotificationController", base::FEATURE_DISABLED_BY_DEFAULT};

// Uses new  AuthSession-based API in cryptohome to authenticate users during
// sign-in.
const base::Feature kUseAuthsessionAuthentication{
    "UseAuthsessionAuthentication", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables using the BluetoothSystem Mojo interface for Bluetooth operations.
const base::Feature kUseBluetoothSystemInAsh{"UseBluetoothSystemInAsh",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Use the staging URL as part of the "Messages" feature under "Connected
// Devices" settings.
const base::Feature kUseMessagesStagingUrl{"UseMessagesStagingUrl",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Remap search+click to right click instead of the legacy alt+click on
// Chrome OS.
const base::Feature kUseSearchClickForRightClick{
    "UseSearchClickForRightClick", base::FEATURE_DISABLED_BY_DEFAULT};

// Use the Stork Production SM-DS address to fetch pending ESim profiles
const base::Feature kUseStorkSmdsServerAddress{
    "UseStorkSmdsServerAddress", base::FEATURE_DISABLED_BY_DEFAULT};

// Use the staging server as part of the Wallpaper App to verify
// additions/removals of wallpapers.
const base::Feature kUseWallpaperStagingUrl{"UseWallpaperStagingUrl",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables user activity prediction for power management on
// Chrome OS.
// Defined here rather than in //chrome alongside other related features so that
// PowerPolicyController can check it.
const base::Feature kUserActivityPrediction{"UserActivityPrediction",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enable or disable bordered key for virtual keyboard on Chrome OS.
const base::Feature kVirtualKeyboardBorderedKey{
    "VirtualKeyboardBorderedKey", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to allow enabling wake on WiFi features in shill.
const base::Feature kWakeOnWifiAllowed{"WakeOnWifiAllowed",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enable new wallpaper experience SWA.
const base::Feature kWallpaperWebUI{"WallpaperWebUI",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Enable full screen wallpaper preview in new wallpaper experience. Requires
// |kWallpaperWebUI| to also be enabled.
const base::Feature kWallpaperFullScreenPreview{
    "WallpaperFullScreenPreview", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable Google Photos integration in the new wallpaper experience. Note that
// this feature flag does not have any effect if `kWallpaperWebUI` is disabled.
const base::Feature kWallpaperGooglePhotosIntegration{
    "WallpaperGooglePhotosIntegration", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable different wallpapers per desk.
const base::Feature kWallpaperPerDesk{"WallpaperPerDesk",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enables special handling of Chrome tab drags from a WebUI tab strip.
// These will be treated similarly to a window drag, showing split view
// indicators in tablet mode, etc. The functionality is behind a flag right now
// since it is under development.
const base::Feature kWebUITabStripTabDragIntegration{
    "WebUITabStripTabDragIntegration", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable MAC Address Randomization on WiFi connection.
const base::Feature kWifiConnectMacAddressRandomization{
    "WifiConnectMacAddressRandomization", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable the syncing of deletes of Wi-Fi configurations.
// This only controls sending delete events to the Chrome Sync server.
const base::Feature kWifiSyncAllowDeletes{"WifiSyncAllowDeletes",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable syncing of Wi-Fi configurations between
// ChromeOS and a connected Android phone.
const base::Feature kWifiSyncAndroid{"WifiSyncAndroid",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to apply incoming Wi-Fi configuration delete events from
// the Chrome Sync server.
const base::Feature kWifiSyncApplyDeletes{"WifiSyncApplyDeletes",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a window control menu to snap, float and move window to another desk.
// https://crbug.com/1240411
const base::Feature kWindowControlMenu{"WindowControlMenu",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Change window creation to be based on cursor position when there are multiple
// displays.
const base::Feature kWindowsFollowCursor{"WindowsFollowCursor",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Fresnel Device Active reporting on Chrome OS.
const base::Feature kDeviceActiveClient{"DeviceActiveClient",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables whether to store UMA logs per-user and whether metrics
// consent is per-user.
const base::Feature kPerUserMetrics{"PerUserMetricsConsent",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
////////////////////////////////////////////////////////////////////////////////

bool AreContextualNudgesEnabled() {
  if (!IsHideShelfControlsInTabletModeEnabled())
    return false;
  return base::FeatureList::IsEnabled(kContextualNudges);
}

bool AreDesksTemplatesEnabled() {
  return base::FeatureList::IsEnabled(kDesksTemplates);
}

bool AreDesksTrackpadSwipeImprovementsEnabled() {
  return base::FeatureList::IsEnabled(kEnableDesksTrackpadSwipeImprovements);
}

bool DoWindowsFollowCursor() {
  return base::FeatureList::IsEnabled(kWindowsFollowCursor);
}

bool IsAdjustSplitViewForVKEnabled() {
  return base::FeatureList::IsEnabled(kAdjustSplitViewForVK);
}

bool IsAllowAmbientEQEnabled() {
  return base::FeatureList::IsEnabled(kAllowAmbientEQ);
}

bool IsAmbientModeDevUseProdEnabled() {
  return base::FeatureList::IsEnabled(kAmbientModeDevUseProdFeature);
}

bool IsAmbientModeEnabled() {
  return base::FeatureList::IsEnabled(kAmbientModeFeature);
}

bool IsAmbientModePhotoPreviewEnabled() {
  return base::FeatureList::IsEnabled(kAmbientModePhotoPreviewFeature);
}

bool IsAmbientModeNewUrlEnabled() {
  return base::FeatureList::IsEnabled(kAmbientModeNewUrl);
}

bool IsAppNotificationsPageEnabled() {
  return base::FeatureList::IsEnabled(kOsSettingsAppNotificationsPage);
}

bool IsArcInputOverlayEnabled() {
  return base::FeatureList::IsEnabled(kArcInputOverlay);
}

bool IsArcNetworkDiagnosticsButtonEnabled() {
  return IsNetworkingInDiagnosticsAppEnabled() &&
         base::FeatureList::IsEnabled(kButtonARCNetworkDiagnostics);
}

bool IsAssistiveMultiWordEnabled() {
  return base::FeatureList::IsEnabled(kAssistMultiWord);
}

bool IsAutoNightLightEnabled() {
  return base::FeatureList::IsEnabled(kAutoNightLight);
}

bool IsBackgroundBlurEnabled() {
  bool enabled_by_feature_flag =
      base::FeatureList::IsEnabled(kEnableBackgroundBlur);
#if defined(ARCH_CPU_ARM_FAMILY)
  // Enable background blur on Mali when GPU rasterization is enabled.
  // See crbug.com/996858 for the condition.
  return enabled_by_feature_flag &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kAshEnableTabletMode);
#else
  return enabled_by_feature_flag;
#endif
}

bool IsBentoBarEnabled() {
  return base::FeatureList::IsEnabled(kBentoBar);
}

bool IsBluetoothRevampEnabled() {
  return base::FeatureList::IsEnabled(kBluetoothRevamp);
}

bool IsCalendarViewEnabled() {
  return base::FeatureList::IsEnabled(kCalendarView);
}

bool IsCaptureModeSelfieCameraEnabled() {
  return base::FeatureList::IsEnabled(kCaptureModeSelfieCamera);
}

bool IsClipboardHistoryContextMenuNudgeEnabled() {
  return base::FeatureList::IsEnabled(kClipboardHistoryContextMenuNudge);
}

bool IsClipboardHistoryEnabled() {
  return base::FeatureList::IsEnabled(kClipboardHistory);
}

bool IsClipboardHistoryNudgeSessionResetEnabled() {
  return base::FeatureList::IsEnabled(kClipboardHistoryNudgeSessionReset);
}

bool IsLauncherItemColorSyncEnabled() {
  return IsLauncherAppSortEnabled() &&
         base::FeatureList::IsEnabled(kLauncherItemColorSync);
}

bool IsClipboardHistoryScreenshotNudgeEnabled() {
  return base::FeatureList::IsEnabled(kClipboardHistoryScreenshotNudge);
}

bool IsCompositingBasedThrottlingEnabled() {
  return base::FeatureList::IsEnabled(kCompositingBasedThrottling);
}

bool IsCryptohomeRecoveryFlowEnabled() {
  return base::FeatureList::IsEnabled(kCryptohomeRecoveryFlow);
}

bool IsDarkLightModeEnabled() {
  return chromeos::features::IsDarkLightModeEnabled();
}

bool IsDemoModeSWAEnabled() {
  return base::FeatureList::IsEnabled(kDemoModeSWA);
}

bool IsDeskTemplateSyncEnabled() {
  return base::FeatureList::IsEnabled(kDeskTemplateSync);
}

bool IsDiagnosticsAppEnabled() {
  return base::FeatureList::IsEnabled(kDiagnosticsApp);
}

bool IsDisplayAlignmentAssistanceEnabled() {
  return base::FeatureList::IsEnabled(kDisplayAlignAssist);
}

bool IsDragUnpinnedAppToPinEnabled() {
  return base::FeatureList::IsEnabled(kDragUnpinnedAppToPin);
}

bool IsDragWindowToNewDeskEnabled() {
  return base::FeatureList::IsEnabled(kDragWindowToNewDesk);
}

bool IsEchePhoneHubPermissionsOnboarding() {
  return base::FeatureList::IsEnabled(kEchePhoneHubPermissionsOnboarding);
}

bool IsEcheSWAEnabled() {
  return base::FeatureList::IsEnabled(kEcheSWA);
}

bool IsEcheSWAResizingEnabled() {
  return base::FeatureList::IsEnabled(kEcheSWAResizing);
}

bool IsEcheSWADebugModeEnabled() {
  return base::FeatureList::IsEnabled(kEcheSWADebugMode);
}

bool IsEcheSWAInBackgroundEnabled() {
  return base::FeatureList::IsEnabled(kEcheSWAInBackground);
}

bool IsESimPolicyEnabled() {
  return base::FeatureList::IsEnabled(kESimPolicy);
}

bool IsExtendedOpenVpnSettingsEnabled() {
  return base::FeatureList::IsEnabled(kExtendedOpenVpnSettings);
}

bool IsFamilyLinkOnSchoolDeviceEnabled() {
  return base::FeatureList::IsEnabled(kFamilyLinkOnSchoolDevice);
}

bool IsFastPairEnabled() {
  return base::FeatureList::IsEnabled(kFastPair);
}

bool IsFastPairSoftwareScanningEnabled() {
  return base::FeatureList::IsEnabled(kFastPairSoftwareScanning);
}

bool IsFileManagerFuseBoxEnabled() {
  return base::FeatureList::IsEnabled(kFuseBox);
}

bool IsFileManagerSwaEnabled() {
  return base::FeatureList::IsEnabled(kFilesSWA);
}

bool IsFirmwareUpdaterAppEnabled() {
  return base::FeatureList::IsEnabled(kFirmwareUpdaterApp);
}

bool IsFullscreenAfterUnlockAllowed() {
  return base::FeatureList::IsEnabled(kFullscreenAfterUnlockAllowed);
}

bool IsFullscreenAlertBubbleEnabled() {
  return base::FeatureList::IsEnabled(kFullscreenAlertBubble);
}

bool IsGaiaCloseViewMessageEnabled() {
  return base::FeatureList::IsEnabled(kGaiaCloseViewMessage);
}

bool IsGaiaReauthEndpointEnabled() {
  return base::FeatureList::IsEnabled(kGaiaReauthEndpoint);
}

bool IsHideArcMediaNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kHideArcMediaNotifications);
}

bool IsHideShelfControlsInTabletModeEnabled() {
  return base::FeatureList::IsEnabled(kHideShelfControlsInTabletMode);
}

bool IsHoldingSpaceInProgressAnimationV2Enabled() {
  return base::FeatureList::IsEnabled(kHoldingSpaceInProgressAnimationV2);
}

bool IsHoldingSpaceInProgressDownloadsIntegrationEnabled() {
  return base::FeatureList::IsEnabled(
      kHoldingSpaceInProgressDownloadsIntegration);
}

bool IsHoldingSpaceInProgressDownloadsNotificationSuppressionEnabled() {
  return base::FeatureList::IsEnabled(
      kHoldingSpaceInProgressDownloadsNotificationSuppression);
}

bool IsHoldingSpaceIncognitoProfileIntegrationEnabled() {
  return base::FeatureList::IsEnabled(kHoldingSpaceIncognitoProfileIntegration);
}

bool IsHostnameSettingEnabled() {
  return base::FeatureList::IsEnabled(kEnableHostnameSetting);
}

bool IsSnoopingProtectionEnabled() {
  return base::FeatureList::IsEnabled(kSnoopingProtection);
}

bool IsIdleInhibitEnabled() {
  return base::FeatureList::IsEnabled(kEnableIdleInhibit);
}

bool IsImprovedDesksKeyboardShortcutsEnabled() {
  return base::FeatureList::IsEnabled(kImprovedDesksKeyboardShortcuts);
}

bool IsInputInDiagnosticsAppEnabled() {
  return base::FeatureList::IsEnabled(kEnableInputInDiagnosticsApp);
}

bool IsInputNoiseCancellationUiEnabled() {
  return base::FeatureList::IsEnabled(kEnableInputNoiseCancellationUi);
}

bool IsInstantTetheringBackgroundAdvertisingSupported() {
  return base::FeatureList::IsEnabled(
      kInstantTetheringBackgroundAdvertisementSupport);
}

bool IsKeyboardBacklightToggleEnabled() {
  return base::FeatureList::IsEnabled(kEnableKeyboardBacklightToggle);
}

bool IsKeyboardBasedDisplayArrangementInSettingsEnabled() {
  return base::FeatureList::IsEnabled(
      kKeyboardBasedDisplayArrangementInSettings);
}

bool IsLauncherAppSortEnabled() {
  return IsProductivityLauncherEnabled() &&
         base::FeatureList::IsEnabled(kLauncherAppSort);
}

bool IsLauncherNudgeShortIntervalEnabled() {
  return IsProductivityLauncherEnabled() &&
         base::FeatureList::IsEnabled(kLauncherNudgeShortInterval);
}

bool IsLicensePackagedOobeFlowEnabled() {
  return base::FeatureList::IsEnabled(kLicensePackagedOobeFlow);
}

bool IsLockScreenHideSensitiveNotificationsSupported() {
  return base::FeatureList::IsEnabled(
      kLockScreenHideSensitiveNotificationsSupport);
}

bool IsLockScreenInlineReplyEnabled() {
  return base::FeatureList::IsEnabled(kLockScreenInlineReply);
}

bool IsLockScreenNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kLockScreenNotifications);
}

bool IsManagedDeviceUIRedesignEnabled() {
  return base::FeatureList::IsEnabled(kManagedDeviceUIRedesign);
}

bool IsManagedTermsOfServiceEnabled() {
  return base::FeatureList::IsEnabled(kManagedTermsOfService);
}

bool IsMicMuteNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kMicMuteNotifications);
}

bool IsMinimumChromeVersionEnabled() {
  return base::FeatureList::IsEnabled(kMinimumChromeVersion);
}

bool IsNearbyKeepAliveFixEnabled() {
  return base::FeatureList::IsEnabled(kNearbyKeepAliveFix);
}

bool IsNetworkingInDiagnosticsAppEnabled() {
  return base::FeatureList::IsEnabled(kEnableNetworkingInDiagnosticsApp) &&
         base::FeatureList::IsEnabled(kDiagnosticsAppNavigation);
}

bool IsOAuthIppEnabled() {
  return base::FeatureList::IsEnabled(kEnableOAuthIpp);
}

bool IsNewLockScreenReauthLayoutEnabled() {
  return base::FeatureList::IsEnabled(kNewLockScreenReauthLayout);
}

bool IsNotificationExpansionAnimationEnabled() {
  return base::FeatureList::IsEnabled(kNotificationExpansionAnimation);
}

bool IsNotificationExperimentalShortTimeoutsEnabled() {
  return base::FeatureList::IsEnabled(kNotificationExperimentalShortTimeouts);
}

bool IsNotificationScrollBarEnabled() {
  return base::FeatureList::IsEnabled(kNotificationScrollBar);
}

bool IsNotificationsInContextMenuEnabled() {
  return base::FeatureList::IsEnabled(kNotificationsInContextMenu);
}

// True if `kNotificationsRefresh` or `kDarkLightMode` is enabled. Showing the
// new notifications UI if the D/L mode feature is enabled, since the new
// notifications UI supports D/L mode. These two features will be launched at
// the same time, or the new notifications UI will be launched earlier than D/L
// mode, so it is safe to do this.
bool IsNotificationsRefreshEnabled() {
  return base::FeatureList::IsEnabled(kNotificationsRefresh) ||
         IsDarkLightModeEnabled();
}

bool IsOobeChromeVoxHintEnabled() {
  return base::FeatureList::IsEnabled(kEnableOobeChromeVoxHint);
}

bool IsOobePolymer3Enabled() {
  return base::FeatureList::IsEnabled(kEnableOobePolymer3);
}

bool IsOobeNetworkScreenSkipEnabled() {
  return base::FeatureList::IsEnabled(kEnableOobeNetworkScreenSkip);
}

bool IsOobeConsolidatedConsentEnabled() {
  return base::FeatureList::IsEnabled(kOobeConsolidatedConsent);
}

bool IsOobeQuickStartEnabled() {
  return base::FeatureList::IsEnabled(kOobeQuickStart);
}

bool IsPcieBillboardNotificationEnabled() {
  return base::FeatureList::IsEnabled(kPcieBillboardNotification);
}
bool IsPciguardUiEnabled() {
  return base::FeatureList::IsEnabled(kEnablePciguardUi);
}

bool IsPerDeskShelfEnabled() {
  return base::FeatureList::IsEnabled(kPerDeskShelf);
}

bool IsPhoneHubCameraRollEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHubCameraRoll);
}

bool IsPerformantSplitViewResizingEnabled() {
  return base::FeatureList::IsEnabled(kPerformantSplitViewResizing);
}

bool IsPersonalizationHubEnabled() {
  return base::FeatureList::IsEnabled(kPersonalizationHub);
}

bool IsPhoneHubEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHub);
}

bool IsPhoneHubCallNotificationEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHubCallNotification);
}

bool IsPhoneHubRecentAppsEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHubRecentApps);
}

bool IsPinAutosubmitBackfillFeatureEnabled() {
  return base::FeatureList::IsEnabled(kQuickUnlockPinAutosubmitBackfill);
}

bool IsPinAutosubmitFeatureEnabled() {
  return base::FeatureList::IsEnabled(kQuickUnlockPinAutosubmit);
}

bool IsPinSetupForManagedUsersEnabled() {
  return base::FeatureList::IsEnabled(kPinSetupForManagedUsers);
}

bool IsPipRoundedCornersEnabled() {
  return base::FeatureList::IsEnabled(kPipRoundedCorners);
}

bool IsProductivityLauncherEnabled() {
  return base::FeatureList::IsEnabled(kProductivityLauncher);
}

bool IsProductivityLauncherAnimationEnabled() {
  return IsProductivityLauncherEnabled() &&
         base::FeatureList::IsEnabled(kProductivityLauncherAnimation);
}

bool IsProjectorEnabled() {
  return IsProjectorAllUserEnabled() || IsProjectorManagedUserEnabled();
}

bool IsProjectorAllUserEnabled() {
  return base::FeatureList::IsEnabled(kProjector);
}

bool IsProjectorManagedUserEnabled() {
  return base::FeatureList::IsEnabled(kProjectorManagedUser);
}

bool IsProjectorAnnotatorEnabled() {
  return IsProjectorEnabled() &&
         base::FeatureList::IsEnabled(kProjectorAnnotator);
}

bool IsQuickDimEnabled() {
  return base::FeatureList::IsEnabled(kQuickDim);
}

bool IsQuickSettingsNetworkRevampEnabled() {
  return base::FeatureList::IsEnabled(kQuickSettingsNetworkRevamp);
}

bool IsRedirectToDefaultIdPEnabled() {
  return base::FeatureList::IsEnabled(kRedirectToDefaultIdP);
}

bool IsReduceDisplayNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kReduceDisplayNotifications);
}

bool IsReverseScrollGesturesEnabled() {
  return base::FeatureList::IsEnabled(kReverseScrollGestures);
}

bool IsSamlNotificationOnPasswordChangeSuccessEnabled() {
  return base::FeatureList::IsEnabled(
      kEnableSamlNotificationOnPasswordChangeSuccess);
}

bool IsSamlReauthenticationOnLockscreenEnabled() {
  return base::FeatureList::IsEnabled(kEnableSamlReauthenticationOnLockscreen);
}

bool IsScalableStatusAreaEnabled() {
  return base::FeatureList::IsEnabled(kScalableStatusArea);
}

bool IsSeparateNetworkIconsEnabled() {
  return base::FeatureList::IsEnabled(kSeparateNetworkIcons);
}

bool IsSettingsAppNotificationSettingsEnabled() {
  return base::FeatureList::IsEnabled(kSettingsAppNotificationSettings);
}

bool IsShelfLauncherNudgeEnabled() {
  return base::FeatureList::IsEnabled(kShelfLauncherNudge);
}

bool IsShimlessRMAFlowEnabled() {
  return base::FeatureList::IsEnabled(kShimlessRMAFlow);
}

bool IsSyncSettingsCategorizationEnabled() {
  return base::FeatureList::IsEnabled(kSyncSettingsCategorization);
}

bool IsStylusBatteryStatusEnabled() {
  return base::FeatureList::IsEnabled(kStylusBatteryStatus);
}

bool IsSystemChinesePhysicalTypingEnabled() {
  return base::FeatureList::IsEnabled(kSystemChinesePhysicalTyping);
}

bool IsSystemJapanesePhysicalTypingEnabled() {
  return base::FeatureList::IsEnabled(kSystemJapanesePhysicalTyping);
}

bool IsSystemKoreanPhysicalTypingEnabled() {
  return base::FeatureList::IsEnabled(kSystemKoreanPhysicalTyping);
}

bool IsTabClusterUIEnabled() {
  return base::FeatureList::IsEnabled(kTabClusterUI);
}

bool IsTrilinearFilteringEnabled() {
  static bool use_trilinear_filtering =
      base::FeatureList::IsEnabled(kTrilinearFiltering);
  return use_trilinear_filtering;
}

bool IsUseStorkSmdsServerAddressEnabled() {
  return base::FeatureList::IsEnabled(kUseStorkSmdsServerAddress);
}

bool IsWallpaperWebUIEnabled() {
  return base::FeatureList::IsEnabled(kWallpaperWebUI);
}

bool IsWallpaperFullScreenPreviewEnabled() {
  return IsWallpaperWebUIEnabled() &&
         base::FeatureList::IsEnabled(kWallpaperFullScreenPreview);
}

bool IsWallpaperGooglePhotosIntegrationEnabled() {
  return IsWallpaperWebUIEnabled() &&
         base::FeatureList::IsEnabled(kWallpaperGooglePhotosIntegration);
}

bool IsWallpaperPerDeskEnabled() {
  return base::FeatureList::IsEnabled(kWallpaperPerDesk);
}

bool IsWebUITabStripTabDragIntegrationEnabled() {
  return base::FeatureList::IsEnabled(kWebUITabStripTabDragIntegration);
}

bool IsWifiSyncAndroidEnabled() {
  return base::FeatureList::IsEnabled(kWifiSyncAndroid);
}

bool IsWindowControlMenuEnabled() {
  return base::FeatureList::IsEnabled(kWindowControlMenu);
}

bool ShouldShowPlayStoreInDemoMode() {
  return base::FeatureList::IsEnabled(kShowPlayInDemoMode);
}

bool ShouldUseAttachApn() {
  // See comment on |kCellularForbidAttachApn| for details.
  return !base::FeatureList::IsEnabled(kCellularForbidAttachApn) &&
         base::FeatureList::IsEnabled(kCellularUseAttachApn);
}

bool ShouldUseV1DeviceSync() {
  return !ShouldUseV2DeviceSync() ||
         !base::FeatureList::IsEnabled(kDisableCryptAuthV1DeviceSync);
}

bool ShouldUseV2DeviceSync() {
  return base::FeatureList::IsEnabled(kCryptAuthV2Enrollment) &&
         base::FeatureList::IsEnabled(kCryptAuthV2DeviceSync);
}

namespace {

// The boolean flag indicating if "WebUITabStrip" feature is enabled in Chrome.
bool g_webui_tab_strip_enabled = false;

}  // namespace

void SetWebUITabStripEnabled(bool enabled) {
  g_webui_tab_strip_enabled = enabled;
}

bool IsWebUITabStripEnabled() {
  return g_webui_tab_strip_enabled;
}

}  // namespace features
}  // namespace ash
