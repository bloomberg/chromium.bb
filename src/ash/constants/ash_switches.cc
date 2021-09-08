// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"

namespace chromeos {
namespace switches {

namespace {

// The memory pressure thresholds selection which is used to decide whether and
// when a memory pressure event needs to get fired.
const char kMemoryPressureExperimentName[] = "ChromeOSMemoryPressureHandling";
const char kMemoryPressureHandlingOff[] = "memory-pressure-off";

// Controls CrOS GaiaId migration for tests ("" is default).
const char kTestCrosGaiaIdMigration[] = "test-cros-gaia-id-migration";

// Value for kTestCrosGaiaIdMigration indicating that migration is started (i.e.
// all stored user keys will be converted to GaiaId)
const char kTestCrosGaiaIdMigrationStarted[] = "started";

}  // namespace

// Please keep the order of these switches synchronized with the header file
// (i.e. in alphabetical order).

const char kAggressiveCacheDiscardThreshold[] = "aggressive-cache-discard";

// If this flag is passed, failed policy fetches will not cause profile
// initialization to fail. This is useful for tests because it means that
// tests don't have to mock out the policy infrastructure.
const char kAllowFailedPolicyFetchForTest[] =
    "allow-failed-policy-fetch-for-test";

// When this flag is set, the OS installation UI can be accessed. This
// allows the user to install from USB to disk.
const char kAllowOsInstall[] = "allow-os-install";

// Allows remote attestation (RA) in dev mode for testing purpose. Usually RA
// is disabled in dev mode because it will always fail. However, there are cases
// in testing where we do want to go through the permission flow even in dev
// mode. This can be enabled by this flag.
const char kAllowRAInDevMode[] = "allow-ra-in-dev-mode";

// Specifies whether an app launched in kiosk mode was auto launched with zero
// delay. Used in order to properly restore auto-launched state during session
// restore flow.
const char kAppAutoLaunched[] = "app-auto-launched";

// Path for app's OEM manifest file.
const char kAppOemManifestFile[] = "app-mode-oem-manifest";

// Signals ARC support status on this device. This can take one of the
// following three values.
// - none: ARC is not installed on this device. (default)
// - installed: ARC is installed on this device, but not officially supported.
//   Users can enable ARC only when Finch experiment is turned on.
// - officially-supported: ARC is installed and supported on this device. So
//   users can enable ARC via settings etc.
const char kArcAvailability[] = "arc-availability";

// DEPRECATED: Please use --arc-availability=installed.
// Signals the availability of the ARC instance on this device.
const char kArcAvailable[] = "arc-available";

// Flag that forces ARC data be cleaned on each start.
const char kArcDataCleanupOnStart[] = "arc-data-cleanup-on-start";

// Flag that disables ARC app sync flow that installs some apps silently. Used
// in autotests to resolve racy conditions.
const char kArcDisableAppSync[] = "arc-disable-app-sync";

// Flag that disables ARC download provider that prevents extra content to be
// downloaded and installed in context of Play Store and GMS Core.
const char kArcDisableDownloadProvider[] = "arc-disable-download-provider";

// Flag to enables an experiment to allow users to turn on 64-bit support in
// native bridge on systems that have such support available but not yet enabled
// by default.
const char kArcEnableNativeBridge64BitSupportExperiment[] =
    "arc-enable-native-bridge-64bit-support-experiment";

// Used in autotest to disable GMS-core caches which is on by default.
const char kArcDisableGmsCoreCache[] = "arc-disable-gms-core-cache";

// Flag that disables ARC locale sync with Android container. Used in autotest
// to prevent conditions when certain apps, including Play Store may get
// restarted. Restarting Play Store may cause random test failures. Enabling
// this flag would also forces ARC container to use 'en-US' as a locale and
// 'en-US,en' as preferred languages.
const char kArcDisableLocaleSync[] = "arc-disable-locale-sync";

// Used to disable GMS scheduling of media store periodic indexing and corpora
// maintenance tasks. Used in performance tests to prevent running during
// testing which can cause unstable results or CPU not idle pre-test failures.
const char kArcDisableMediaStoreMaintenance[] =
    "arc-disable-media-store-maintenance";

// Flag that disables ARC Play Auto Install flow that installs set of predefined
// apps silently. Used in autotests to resolve racy conditions.
const char kArcDisablePlayAutoInstall[] = "arc-disable-play-auto-install";

// Used for development of Android app that are included into ARC as system
// default apps in order to be able to install them via adb.
const char kArcDisableSystemDefaultApps[] = "arc-disable-system-default-apps";

// Flag that forces the OptIn ui to be shown. Used in tests.
const char kArcForceShowOptInUi[] = "arc-force-show-optin-ui";

// Flag that enables developer options needed to generate an ARC Play Auto
// Install roster. Used manually by developers.
const char kArcGeneratePlayAutoInstall[] = "arc-generate-play-auto-install";

// Write ARC++ install events to chrome log for integration test.
const char kArcInstallEventChromeLogForTests[] =
    "arc-install-event-chrome-log-for-tests";

// Used in autotest to specifies how to handle packages cache. Can be
// copy - copy resulting packages.xml to the temporary directory.
// skip-copy - skip initial packages cache setup and copy resulting packages.xml
//             to the temporary directory.
const char kArcPackagesCacheMode[] = "arc-packages-cache-mode";

// Used in autotest to forces Play Store auto-update state. Can be
// on - auto-update is forced on.
// off - auto-update is forced off.
const char kArcPlayStoreAutoUpdate[] = "arc-play-store-auto-update";

// Set the scale for ARC apps. This is in DPI. e.g. 280 DPI is ~ 1.75 device
// scale factor.
// See
// https://source.android.com/compatibility/android-cdd#3_7_runtime_compatibility
// for list of supported DPI values.
const char kArcScale[] = "arc-scale";

// Defines how to start ARC. This can take one of the following values:
// - always-start automatically start with Play Store UI support.
// - always-start-with-no-play-store automatically start without Play Store UI.
// If it is not set, then ARC is started in default mode.
const char kArcStartMode[] = "arc-start-mode";

// Sets ARC Terms Of Service hostname url for testing.
const char kArcTosHostForTests[] = "arc-tos-host-for-tests";

// Sets the mode of operation for ureadahead during ARCVM boot. If this switch
// is not set, ARCVM ureadahead will check for the presence and age of pack
// file and reads ahead files to page cache for improved boot performance.
// generate - used during Android PFQ data collector to pre-generate pack file
//            and upload to Google Cloud as build artifact for CrOS build image.
// disabled - used for test purpose to disable ureadahead during ARCVM boot.
const char kArcVmUreadaheadMode[] = "arcvm-ureadahead-mode";

// If this flag is set, it indicates that this device is a "Cellular First"
// device. Cellular First devices use cellular telephone data networks as
// their primary means of connecting to the internet.
// Setting this flag has two consequences:
// 1. Cellular data roaming will be enabled by default.
// 2. UpdateEngine will be instructed to allow auto-updating over cellular
//    data connections.
const char kCellularFirst[] = "cellular-first";

// Default large wallpaper to use for kids accounts (as path to trusted,
// non-user-writable JPEG file).
const char kChildWallpaperLarge[] = "child-wallpaper-large";

// Default small wallpaper to use for kids accounts (as path to trusted,
// non-user-writable JPEG file).
const char kChildWallpaperSmall[] = "child-wallpaper-small";

// Forces CrOS region value.
const char kCrosRegion[] = "cros-region";

// Control regions data load ("" is default).
const char kCrosRegionsMode[] = "cros-regions-mode";

// "Hide" value for kCrosRegionsMode (VPD values are hidden).
const char kCrosRegionsModeHide[] = "hide";

// "Override" value for kCrosRegionsMode (region's data is read first).
const char kCrosRegionsModeOverride[] = "override";

// Controls if AuthSession API should be used when interacting with cryptohomed.
const char kCryptohomeUseAuthSession[] = "cryptohome-use-authsession";

// Indicates that the wallpaper images specified by
// kAshDefaultWallpaper{Large,Small} are OEM-specific (i.e. they are not
// downloadable from Google).
const char kDefaultWallpaperIsOem[] = "default-wallpaper-is-oem";

// Default large wallpaper to use (as path to trusted, non-user-writable JPEG
// file).
const char kDefaultWallpaperLarge[] = "default-wallpaper-large";

// Default small wallpaper to use (as path to trusted, non-user-writable JPEG
// file).
const char kDefaultWallpaperSmall[] = "default-wallpaper-small";

// App ID to use for highlights app in demo mode.
const char kDemoModeHighlightsApp[] = "demo-mode-highlights-extension";

// App ID to use for screensaver app in demo mode.
const char kDemoModeScreensaverApp[] = "demo-mode-screensaver-extension";

// Time in seconds before a machine at OOBE is considered derelict.
const char kDerelictDetectionTimeout[] = "derelict-detection-timeout";

// Time in seconds before a derelict machines starts demo mode.
const char kDerelictIdleTimeout[] = "derelict-idle-timeout";

// Disables android user data wipe on opt out.
const char kDisableArcDataWipe[] = "disable-arc-data-wipe";

// Disables ARC Opt-in verification process and ARC is enabled by default.
const char kDisableArcOptInVerification[] = "disable-arc-opt-in-verification";

// Disables the Chrome OS demo.
const char kDisableDemoMode[] = "disable-demo-mode";

// If this switch is set, the device cannot be remotely disabled by its owner.
const char kDisableDeviceDisabling[] = "disable-device-disabling";

// Disable encryption migration for user's cryptohome to run latest Arc.
const char kDisableEncryptionMigration[] = "disable-encryption-migration";

// Disables fine grained time zone detection.
const char kDisableFineGrainedTimeZoneDetection[] =
    "disable-fine-grained-time-zone-detection";

// Disables GAIA services such as enrollment and OAuth session restore. Used by
// 'fake' telemetry login.
const char kDisableGaiaServices[] = "disable-gaia-services";

// Disables HID-detection OOBE screen.
const char kDisableHIDDetectionOnOOBEForTesting[] =
    "disable-hid-detection-on-oobe";

// Avoid doing expensive animations upon login.
const char kDisableLoginAnimations[] = "disable-login-animations";

// Disables requests for an enterprise machine certificate during attestation.
const char kDisableMachineCertRequest[] = "disable-machine-cert-request";

// Disables the ChromeVox hint idle detection in OOBE, which can lead to
// unexpected behavior during tests.
const char kDisableOOBEChromeVoxHintTimerForTesting[] =
    "disable-oobe-chromevox-hint-timer-for-testing";

// Enables the ChromeVox hint in OOBE for dev mode. This flag is used
// to override the default dev mode behavior of disabling the feature.
// If both kEnableOOBEChromeVoxHintForDevMode and
// kDisableOOBEChromeVoxHintTimerForTesting are present, the ChromeVox hint
// will be disabled, since the latter flag takes precedence over the former.
const char kEnableOOBEChromeVoxHintForDevMode[] =
    "enable-oobe-chromevox-hint-timer-for-dev-mode";

// Disables per-user timezone.
const char kDisablePerUserTimezone[] = "disable-per-user-timezone";

// Disables rollback option on reset screen.
const char kDisableRollbackOption[] = "disable-rollback-option";

// Disables client certificate authentication on the sign-in frame on the Chrome
// OS sign-in profile.
// TODO(https://crbug.com/844022): Remove this flag when reaching endpoints that
// request client certs does not hang anymore when there is no system token yet.
const char kDisableSigninFrameClientCerts[] =
    "disable-signin-frame-client-certs";

// Disables volume adjust sound.
const char kDisableVolumeAdjustSound[] = "disable-volume-adjust-sound";

// DEPRECATED. Please use --arc-availability=officially-supported.
// Enables starting the ARC instance upon session start.
const char kEnableArc[] = "enable-arc";

// Enables ARCVM.
const char kEnableArcVm[] = "enable-arcvm";

// Enables ARCVM realtime VCPU feature.
const char kEnableArcVmRtVcpu[] = "enable-arcvm-rt-vcpu";

// Madvises the kernel to use Huge Pages for guest memory.
const char kArcVmUseHugePages[] = "arcvm-use-hugepages";

// Enables the Cast Receiver.
const char kEnableCastReceiver[] = "enable-cast-receiver";

// Enables consumer kiosk mode for Chrome OS.
const char kEnableConsumerKiosk[] = "enable-consumer-kiosk";

// Enables encryption migration for user's cryptohome to run latest Arc.
const char kEnableEncryptionMigration[] = "enable-encryption-migration";

// Enables sharing assets for installed default apps.
const char kEnableExtensionAssetsSharing[] = "enable-extension-assets-sharing";

// Enables the use of 32-bit Houdini library for ARM binary translation.
const char kEnableHoudini[] = "enable-houdini";

// Enables the use of 64-bit Houdini library for ARM binary translation.
const char kEnableHoudini64[] = "enable-houdini64";

// Enables the use of 32-bit NDK translation library for ARM binary translation.
const char kEnableNdkTranslation[] = "enable-ndk-translation";

// Enables the use of 64-bit NDK translation library for ARM binary translation.
const char kEnableNdkTranslation64[] = "enable-ndk-translation64";

// Enables request of tablet site (via user agent override).
const char kEnableRequestTabletSite[] = "enable-request-tablet-site";

// Enables tablet form factor.
const char kEnableTabletFormFactor[] = "enable-tablet-form-factor";

// Enables the touch calibration option in MD settings UI for valid touch
// displays.
const char kEnableTouchCalibrationSetting[] =
    "enable-touch-calibration-setting";

// Enables touchpad three-finger-click as middle button.
const char kEnableTouchpadThreeFingerClick[] =
    "enable-touchpad-three-finger-click";

// Disables ARC for managed accounts.
const char kEnterpriseDisableArc[] = "enterprise-disable-arc";

// Whether to enable forced enterprise re-enrollment.
const char kEnterpriseEnableForcedReEnrollment[] =
    "enterprise-enable-forced-re-enrollment";

// Whether to enable initial enterprise enrollment.
const char kEnterpriseEnableInitialEnrollment[] =
    "enterprise-enable-initial-enrollment";

// Whether to enable PSM (private set membership) queries.
const char kEnterpriseEnablePsm[] = "enterprise-enable-psm";

// Enables the zero-touch enterprise enrollment flow.
const char kEnterpriseEnableZeroTouchEnrollment[] =
    "enterprise-enable-zero-touch-enrollment";

// Power of the power-of-2 initial modulus that will be used by the
// auto-enrollment client. E.g. "4" means the modulus will be 2^4 = 16.
const char kEnterpriseEnrollmentInitialModulus[] =
    "enterprise-enrollment-initial-modulus";

// Power of the power-of-2 maximum modulus that will be used by the
// auto-enrollment client.
const char kEnterpriseEnrollmentModulusLimit[] =
    "enterprise-enrollment-modulus-limit";

// Interval in seconds between Chrome reading external metrics from
// /var/lib/metrics/uma-events.
const char kExternalMetricsCollectionInterval[] =
    "external-metrics-collection-interval";

// Name of a subdirectory of the main external web apps directory which
// additional web apps configs should be loaded from. Used to load
// device-specific web apps.
const char kExtraWebAppsDir[] = "extra-web-apps-dir";

// An absolute path to the chroot hosting the DriveFS to use. This is only used
// when running on Linux, i.e. when IsRunningOnChromeOS() returns false.
const char kFakeDriveFsLauncherChrootPath[] =
    "fake-drivefs-launcher-chroot-path";

// A relative path to socket to communicat with the fake DriveFS launcher within
// the chroot specified by kFakeDriveFsLauncherChrootPath. This is only used
// when running on Linux, i.e. when IsRunningOnChromeOS() returns false.
const char kFakeDriveFsLauncherSocketPath[] =
    "fake-drivefs-launcher-socket-path";

// Specifies number of recommended (fake) ARC apps during user onboarding.
// App descriptions are generated locally instead of being fetched from server.
// Limited to ChromeOS-on-linux and test images only.
const char kFakeArcRecommendedAppsForTesting[] =
    "fake-arc-recommended-apps-for-testing";

// Fingerprint sensor location indicates the physical sensor's location. The
// value is a string with possible values: "power-button-top-left",
// "keyboard-bottom-left", keyboard-bottom-right", "keyboard-top-right".
const char kFingerprintSensorLocation[] = "fingerprint-sensor-location";

// Specifies the device's form factor. If provided, this flag overrides the
// value from the LSB release info. Possible values are: "CHROMEBASE",
// "CHROMEBIT", "CHROMEBOOK", "REFERENCE", "CHROMEBOX"
const char kFormFactor[] = "form-factor";

// Passed to Chrome the first time that it's run after the system boots.
// Not passed on restart after sign out.
const char kFirstExecAfterBoot[] = "first-exec-after-boot";

// Forces developer tools availability, no matter what values the enterprise
// policies DeveloperToolsDisabled and DeveloperToolsAvailability are set to.
const char kForceDevToolsAvailable[] = "force-devtools-available";

// Forces first-run UI to be shown for every login.
const char kForceFirstRunUI[] = "force-first-run-ui";

// Force enables the Happiness Tracking System for the device. This ignores
// user profile check and time limits and shows the notification every time
// for any type of user. Should be used only for testing.
const char kForceHappinessTrackingSystem[] = "force-happiness-tracking-system";

// Forces Hardware ID check (happens during OOBE) to fail. Should be used only
// for testing.
const char kForceHWIDCheckFailureForTest[] =
    "force-hwid-check-failure-for-test";

// Usually in browser tests the usual login manager bringup is skipped so that
// tests can change how it's brought up. This flag disables that.
const char kForceLoginManagerInTests[] = "force-login-manager-in-tests";

// Force system compositor mode when set.
const char kForceSystemCompositorMode[] = "force-system-compositor-mode";

// Indicates that the browser is in "browse without sign-in" (Guest session)
// mode. Should completely disable extensions, sync and bookmarks.
const char kGuestSession[] = "bwsi";

// Large wallpaper to use in guest mode (as path to trusted, non-user-writable
// JPEG file).
const char kGuestWallpaperLarge[] = "guest-wallpaper-large";

// Small wallpaper to use in guest mode (as path to trusted, non-user-writable
// JPEG file).
const char kGuestWallpaperSmall[] = "guest-wallpaper-small";

// If set, the system is a Chromebook with a "standard Chrome OS keyboard",
// which generally means one with a Search key in the standard Caps Lock
// location above the Left Shift key. It should be unset for Chromebooks with
// both Search and Caps Lock keys (e.g. stout) and for devices like Chromeboxes
// that only use external keyboards.
const char kHasChromeOSKeyboard[] = "has-chromeos-keyboard";

// Defines user homedir. This defaults to primary user homedir.
const char kHomedir[] = "homedir";

// If set, the "ignore_dev_conf" field in StartArcVmRequest message will
// consequently be set such that all development configuration directives in
// /usr/local/vms/etc/arcvm_dev.conf will be ignored during ARCVM start.
const char kIgnoreArcVmDevConf[] = "ignore-arcvm-dev-conf";

// If true, profile selection in UserManager will always return active user's
// profile.
// TODO(nkostlyev): http://crbug.com/364604 - Get rid of this switch after we
// turn on multi-profile feature on ChromeOS.
const char kIgnoreUserProfileMappingForTests[] =
    "ignore-user-profile-mapping-for-tests";

// Decreases delay in uploading installation event logs for integration test.
const char kInstallLogFastUploadForTests[] =
    "install-log-fast-upload-for-tests";

// If set, the Chrome settings will not expose the option to enable crostini
// unless the enable-experimental-kernel-vm-support flag is set in
// chrome://flags
const char kKernelnextRestrictVMs[] = "kernelnext-restrict-vms";

// If this switch is set, then ash-chrome will pass additional arguments when
// launching lacros-chrome. The string '####' is used as a delimiter. Example:
// --lacros-chrome-additional-args="--foo=5####--bar=/tmp/dir name". Will
// result in two arguments passed to lacros-chrome:
//   --foo=5
//   --bar=/tmp/dir name
const char kLacrosChromeAdditionalArgs[] = "lacros-chrome-additional-args";

// Additional environment variables set for lacros-chrome. The string '####' is
// used as a delimiter. For example:
// --lacros-chrome-additional-env=WAYLAND_DEBUG=client####FOO=bar
// will enable Wayland protocol logging and set FOO=bar.
const char kLacrosChromeAdditionalEnv[] = "lacros-chrome-additional-env";

// If this switch is set, then ash-chrome will exec the lacros-chrome binary
// from the indicated path rather than from component updater. Note that the
// path should be to a directory that contains a binary named 'chrome'.
const char kLacrosChromePath[] = "lacros-chrome-path";

// If set, ash-chrome will drop a Unix domain socket to wait for a process to
// connect to it, and the connection will be used to request file descriptors
// from ash-chrome, and when the process forks to start a lacros-chrome, the
// obtained file descriptor will be used by lacros-chrome to set up the mojo
// connection with ash-chrome. There are mainly two use cases:
// 1. Test launcher to run browser tests in testing environment.
// 2. A terminal to start lacros-chrome with a debugger.
const char kLacrosMojoSocketForTesting[] = "lacros-mojo-socket-for-testing";

// Enables Chrome-as-a-login-manager behavior.
const char kLoginManager[] = "login-manager";

// Specifies the profile to use once a chromeos user is logged in.
// This parameter is ignored if user goes through login screen since user_id
// hash defines which profile directory to use.
// In case of browser restart within active session this parameter is used
// to pass user_id hash for primary user.
const char kLoginProfile[] = "login-profile";

// Specifies the user which is already logged in.
const char kLoginUser[] = "login-user";

// Determines the URL to be used when calling the backend.
const char kMarketingOptInUrl[] = "marketing-opt-in-url";

// Enables natural scroll by default.
const char kNaturalScrollDefault[] = "enable-natural-scroll-default";

// An optional comma-separated list of IDs of apps that can be used to take
// notes. If unset, a hardcoded list is used instead.
const char kNoteTakingAppIds[] = "note-taking-app-ids";

// Allows the eula url to be overridden for tests.
const char kOobeEulaUrlForTests[] = "oobe-eula-url-for-tests";

// Indicates that the first user run flow (sequence of OOBE screens after the
// first user login) should show tablet mode centric screens, even if the device
// is not in tablet mode.
const char kOobeForceTabletFirstRun[] = "oobe-force-tablet-first-run";

// Indicates that OOBE should be scaled for big displays similar to how Meets
// app scales UI.
// TODO(crbug.com/1205364): Remove after adding new scheme.
const char kOobeLargeScreenSpecialScaling[] =
    "oobe-large-screen-special-scaling";

// Skips all other OOBE pages after user login.
const char kOobeSkipPostLogin[] = "oobe-skip-postlogin";

// Skip to login screen.
const char kOobeSkipToLogin[] = "oobe-skip-to-login";

// Interval at which we check for total time on OOBE.
const char kOobeTimerInterval[] = "oobe-timer-interval";

// Allows the timezone to be overridden on the marketing opt-in screen.
const char kOobeTimezoneOverrideForTests[] = "oobe-timezone-override-for-tests";

// SAML assertion consumer URL, used to detect when Gaia-less SAML flows end
// (e.g. for SAML managed guest sessions)
// TODO(984021): Remove when URL is sent by DMServer.
const char kPublicAccountsSamlAclUrl[] = "public-accounts-saml-acl-url";

// If set to "true", the profile requires policy during restart (policy load
// must succeed, otherwise session restart should fail).
const char kProfileRequiresPolicy[] = "profile-requires-policy";

// The rlz ping delay (in seconds) that overwrites the default value.
const char kRlzPingDelay[] = "rlz-ping-delay";

// The switch added by session_manager daemon when chrome crashes 3 times or
// more within the first 60 seconds on start.
// See BrowserJob::ExportArgv in platform2/login_manager/browser_job.cc.
const char kSafeMode[] = "safe-mode";

// Password change url for SAML users.
// TODO(941489): Remove when the bug is fixed.
const char kSamlPasswordChangeUrl[] = "saml-password-change-url";

// Used for overriding the time limit imposed by the policies
// SAMLOfflineSigninTimeLimit & GaiaOfflineSigninTimeLimitDays when testing.
// TODO(crbug.com/1177416): Clean up once testing is complete
const char kOfflineSignInTimeLimitInSecondsOverrideForTesting[] =
    "offline-signin-timelimit-in-seconds-override-for-testing";

// Used for overriding the required user activity time before running the
// onboarding survey.
const char kTimeBeforeOnboardingSurveyInSecondsForTesting[] =
    "time-before-onboarding-survey-in-seconds-for-testing";

// Used for overriding the preference set by the policy
// kSamlLockScreenReauthenticationEnabled to true.
// TODO(crbug.com/1177416): Clean up once testing is complete
const char kSamlLockScreenReauthenticationEnabledOverrideForTesting[] =
    "saml-lockscreen-reauthentication-enabled-override-for-testing";

// New modular design for the shelf with apps separated into a hotseat UI and
// smaller shelf in clamshell mode.
const char kShelfHotseat[] = "shelf-hotseat";

// App window previews when hovering over the shelf.
const char kShelfHoverPreviews[] = "shelf-hover-previews";

// The name of the per-model directory which contains per-region
// subdirectories with regulatory label files for this model.
// The per-model directories (if there are any) are located under
// "/usr/share/chromeos-assets/regulatory_labels/".
const char kRegulatoryLabelDir[] = "regulatory-label-dir";

// If true, the developer tool overlay will be shown for the login/lock screen.
// This makes it easier to test layout logic.
const char kShowLoginDevOverlay[] = "show-login-dev-overlay";

// Enables OOBE UI Debugger for ease of navigation between screens during manual
// testing. Limited to ChromeOS-on-linux and test images only.
const char kShowOobeDevOverlay[] = "show-oobe-dev-overlay";

// Enables OOBE testing API for tast tests.
const char kEnableOobeTestAPI[] = "enable-oobe-test-api";

// Specifies directory for screenshots taken with OOBE UI Debugger.
const char kOobeScreenshotDirectory[] = "oobe-screenshot-dir";

// Disables online sign-in enforcement in tast tests.
const char kSkipForceOnlineSignInForTesting[] =
    "skip-force-online-signin-for-testing";

// Specifies directory for the Telemetry System Web Extension.
const char kTelemetryExtensionDirectory[] = "telemetry-extension-dir";

// Enables testing for encryption migration UI.
const char kTestEncryptionMigrationUI[] = "test-encryption-migration-ui";

// Enables the wallpaper picker to fetch images from the test server.
const char kTestWallpaperServer[] = "test-wallpaper-server";

// Overrides Tether with stub service. Provide integer arguments for the number
// of fake networks desired, e.g. 'tether-stub=2'.
const char kTetherStub[] = "tether-stub";

// Tells the Chromebook to scan for a tethering host even if there is already a
// wired connection. This allows end-to-end tests to be deployed over ethernet
// without that connection preventing scans and thereby blocking the testing of
// cases with no preexisting connection. Should be used only for testing.
const char kTetherHostScansIgnoreWiredConnections[] =
    "tether-host-scans-ignore-wired-connections";

// Shows all Bluetooth devices in UI (System Tray/Settings Page.)
const char kUnfilteredBluetoothDevices[] = "unfiltered-bluetooth-devices";

// Used to tell the policy infrastructure to not let profile initialization
// complete until policy is manually set by a test. This is used to provide
// backward compatibility with a few tests that incorrectly use the
// synchronously-initialized login profile to run their tests - do not add new
// uses of this flag.
const char kWaitForInitialPolicyFetchForTest[] =
    "wait-for-initial-policy-fetch-for-test";

// Prevents any CPU restrictions being set on the ARC container. Only meant to
// be used by tests as some tests may time out if the ARC container is
// throttled.
const char kDisableArcCpuRestriction[] = "disable-arc-cpu-restriction";

// If this switch is passed, the device policy DeviceMinimumVersion
// assumes that the device has reached Auto Update Expiration. This is useful
// for testing the policy behaviour on the DUT.
const char kUpdateRequiredAueForTest[] = "aue-reached-for-update-required-test";

// Uses fake ml service impl to simulate CrOS ml service daemon. This should
// only be used for lacros_chrome_browsertests that requires ml service.
const char kUseFakeMLServiceForTest[] = "use-fake-ml-service-for-test";

// Enables configuring the OEM Device Requsition in the OOBE.
const char kEnableRequisitionEdits[] = "enable-requisition-edits";

bool MemoryPressureHandlingEnabled() {
  if (base::FieldTrialList::FindFullName(kMemoryPressureExperimentName) ==
      kMemoryPressureHandlingOff) {
    return false;
  }
  return true;
}

bool IsAuthSessionCryptohomeEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kCryptohomeUseAuthSession);
}

bool IsGaiaIdMigrationStarted() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kTestCrosGaiaIdMigration))
    return false;

  return command_line->GetSwitchValueASCII(kTestCrosGaiaIdMigration) ==
         kTestCrosGaiaIdMigrationStarted;
}

bool IsCellularFirstDevice() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kCellularFirst);
}

bool IsSigninFrameClientCertsEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableSigninFrameClientCerts);
}

bool ShouldShowShelfHoverPreviews() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kShelfHoverPreviews);
}

bool ShouldTetherHostScansIgnoreWiredConnections() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kTetherHostScansIgnoreWiredConnections);
}

bool ShouldSkipOobePostLogin() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kOobeSkipPostLogin);
}

bool IsTabletFormFactor() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kEnableTabletFormFactor);
}

bool IsGaiaServicesDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableGaiaServices);
}

bool IsArcCpuRestrictionDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableArcCpuRestriction);
}

bool IsUnfilteredBluetoothDevicesEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kUnfilteredBluetoothDevices);
}

bool ShouldOobeUseTabletModeFirstRun() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kOobeForceTabletFirstRun);
}

bool ShouldScaleOobe() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kOobeLargeScreenSpecialScaling);
}

bool IsAueReachedForUpdateRequiredForTest() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kUpdateRequiredAueForTest);
}

bool IsOOBEChromeVoxHintTimerDisabledForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableOOBEChromeVoxHintTimerForTesting);
}

bool IsOOBEChromeVoxHintEnabledForDevMode() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kEnableOOBEChromeVoxHintForDevMode);
}

bool IsDeviceRequisitionConfigurable() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kEnableRequisitionEdits);
}

bool IsOsInstallAllowed() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kAllowOsInstall);
}

}  // namespace switches
}  // namespace chromeos
