// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CONSTANTS_CHROMEOS_SWITCHES_H_
#define CHROMEOS_CONSTANTS_CHROMEOS_SWITCHES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/memory/memory_pressure_monitor_chromeos.h"
#include "chromeos/dbus/constants/dbus_switches.h"

namespace chromeos {
namespace switches {

// Switches that are used in src/chromeos must go here.
// Other switches that apply just to chromeos code should go here also (along
// with any code that is specific to the chromeos system). Chrome OS specific
// UI should be in src/ash.

// Note: If you add a switch, consider if it needs to be copied to a subsequent
// command line if the process executes a new copy of itself.  (For example,
// see chromeos::LoginUtil::GetOffTheRecordCommandLine().)

// Please keep alphabetized.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kAggressiveCacheDiscardThreshold[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kAggressiveTabDiscardThreshold[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kAggressiveThreshold[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kAllowFailedPolicyFetchForTest[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kAllowRAInDevMode[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kAppAutoLaunched[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kAppOemManifestFile[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kArcAvailability[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kArcAvailable[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kArcDataCleanupOnStart[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kArcDisableAppSync[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kArcDisableLocaleSync[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kArcDisablePlayAutoInstall[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kArcForceCacheAppIcons[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kArcForceShowOptInUi[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kArcPackagesCacheMode[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kArcPlayStoreAutoUpdate[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kArcStartMode[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kArcTransitionMigrationRequired[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kCellularFirst[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kChildWallpaperLarge[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kChildWallpaperSmall[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kConservativeThreshold[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kCrosGaiaApiV1[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kCrosRegion[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kCrosRegionsMode[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kCrosRegionsModeHide[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kCrosRegionsModeOverride[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kDefaultWallpaperIsOem[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kDefaultWallpaperLarge[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kDefaultWallpaperSmall[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDerelictDetectionTimeout[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kDerelictIdleTimeout[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kDisableArcDataWipe[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableArcOptInVerification[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kDisableDemoMode[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableDeviceDisabling[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableEncryptionMigration[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableEolNotification[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableFineGrainedTimeZoneDetection[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kDisableGaiaServices[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableHIDDetectionOnOOBE[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableLoginAnimations[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableMachineCertRequest[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableMtpWriteSupport[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableMultiDisplayLayout[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kDisableNewZIPUnpacker[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableOfficeEditingComponentApp[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisablePerUserTimezone[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisablePhysicalKeyboardAutocorrect[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kDisableRollbackOption[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableSigninFrameClientCerts[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableSigninFrameClientCertUserSelection[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableVolumeAdjustSound[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kDisableWakeOnWifi[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kEnableArc[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnableArcOobeOptinNoSkip[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kEnableArcVm[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kEnableCastReceiver[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnableChromevoxDeveloperOption[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kEnableConsumerKiosk[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnableEncryptionMigration[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnableExtensionAssetsSharing[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnableFirstRunUITransitions[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnableMarketingOptInScreen[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnablePhysicalKeyboardAutocorrect[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnableRequestTabletSite[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnableTouchCalibrationSetting[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnableTouchpadThreeFingerClick[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kEnterpriseDisableArc[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnterpriseDisableLicenseTypeSelection[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnterpriseEnableForcedReEnrollment[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnterpriseEnableInitialEnrollment[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnterpriseEnableZeroTouchEnrollment[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnterpriseEnrollmentInitialModulus[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnterpriseEnrollmentModulusLimit[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kExternalMetricsCollectionInterval[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kFirstExecAfterBoot[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kFakeDriveFsLauncherChrootPath[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kFakeDriveFsLauncherSocketPath[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kForceDevToolsAvailable[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kForceFirstRunUI[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kForceHappinessTrackingSystem[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kForceLoginManagerInTests[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kForceSystemCompositorMode[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kForceUseChromeCamera[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kGuestSession[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kGuestWallpaperLarge[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kGuestWallpaperSmall[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kHasChromeOSKeyboard[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kHideAndroidFilesInFilesApp[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kHomedir[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kIgnoreUserProfileMappingForTests[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kKioskNextHomeUrlPrefix[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kLoginManager[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kLoginProfile[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kLoginUser[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kMemoryPressureThresholds[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kNaturalScrollDefault[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kNeedArcMigrationPolicyCheck[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kNoteTakingAppIds[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kOobeForceShowScreen[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kOobeGuestSession[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kOobeSkipPostLogin[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kOobeSkipToLogin[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kOobeTimerInterval[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableArcCpuRestriction[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kProfileRequiresPolicy[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kRedirectLibassistantLogging[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kRegulatoryLabelDir[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kRlzPingDelay[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kSamlPasswordChangeUrl[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kShelfHoverPreviews[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kShowAndroidFilesInFilesApp[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kShowLoginDevOverlay[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kTestEncryptionMigrationUI[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kTestWallpaperServer[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kTetherStub[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kTetherHostScansIgnoreWiredConnections[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kWaitForInitialPolicyFetchForTest[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kWakeOnWifiPacket[];

// Controls whether to enable Chrome OS Account Manager.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kAccountManager;

// Controls whether to enable Google Assistant feature.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAssistantFeature;

// Controls whether to show the system tray language toggle in Demo Mode.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kShowLanguageToggleInDemoMode;

// Controls whether to show the Play Store icon in Demo Mode.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kShowPlayInDemoMode;

// Controls whether to show a static splash screen instead of the user pods
// before demo sessions log in.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kShowSplashScreenInDemoMode;

// Controls whether to support country-level customization in Demo Mode.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kSupportCountryCustomizationInDemoMode;

// Returns true if the system should wake in response to wifi traffic.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool WakeOnWifiEnabled();

// Returns true if memory pressure handling is enabled.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool MemoryPressureHandlingEnabled();

// Returns thresholds for determining if the system is under memory pressure.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
base::chromeos::MemoryPressureMonitor::MemoryPressureThresholds
GetMemoryPressureThresholds();

// Returns true if flags are set indicating that stored user keys are being
// converted to GAIA IDs.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsGaiaIdMigrationStarted();

// Returns true if this is a Cellular First device.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsCellularFirstDevice();

// Returns true if Chrome OS Account Manager is enabled.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsAccountManagerEnabled();

// Returns true if Google Assistant flags are enabled.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsAssistantFlagsEnabled();

// Returns true if Google Assistant is enabled.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsAssistantEnabled();

// Returns true if client certificate authentication for the sign-in frame on
// the Chrome OS sign-in screen is enabled.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsSigninFrameClientCertsEnabled();

// Returns true if user selection of certificates is enabled for the sign-in
// frame on the Chrome OS sign-in screen.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
bool IsSigninFrameClientCertUserSelectionEnabled();

// Returns true if we should show window previews when hovering over an app
// on the shelf.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool ShouldShowShelfHoverPreviews();

// Returns true if Instant Tethering should support hosts which use the
// background advertisement model.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
bool IsInstantTetheringBackgroundAdvertisingSupported();

// Returns true if the Chromebook should ignore its wired connections when
// deciding whether to run scans for tethering hosts. Should be used only for
// testing.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
bool ShouldTetherHostScansIgnoreWiredConnections();

// Returns true if Play Store should be available in Demo Mode.
// TODO(michaelpg): Remove after M71 branch to re-enable Play Store by default.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool ShouldShowPlayStoreInDemoMode();

// Returns true if we should skip all other OOBE pages after user login.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool ShouldSkipOobePostLogin();

// Returns true if GAIA services has been disabled.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsGaiaServicesDisabled();

// Returns true if |kDisableArcCpuRestriction| is true.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsArcCpuRestrictionDisabled();

}  // namespace switches
}  // namespace chromeos

#endif  // CHROMEOS_CONSTANTS_CHROMEOS_SWITCHES_H_
