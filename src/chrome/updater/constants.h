// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_CONSTANTS_H_
#define CHROME_UPDATER_CONSTANTS_H_

#include "build/build_config.h"
#include "components/update_client/update_client_errors.h"

namespace updater {

// The updater specific app ID.
extern const char kUpdaterAppId[];

// The app ID used to qualify the updater.
extern const char kQualificationAppId[];

// A suffix appended to the updater executable name before any file extension.
extern const char kExecutableSuffix[];

// "0.0.0.0". Historically, a null version has been used to indicate a
// new install.
extern const char kNullVersion[];

// Command line switches.
//
// This switch starts the COM server. This switch is invoked by the COM runtime
// when CoCreate is called on one of several CLSIDs that the server supports.
// We expect to use the COM server for the following scenarios:
// * The Server for the UI when installing User applications, and as a fallback
//   if the Service fails when installing Machine applications.
// * The On-Demand COM Server when running for User, and as a fallback if the
//   Service fails.
// * COM Server for launching processes at Medium Integrity, i.e., de-elevating.
// * The On-Demand COM Server when the client requests read-only interfaces
//   (say, Policy queries) at Medium Integrity.
// * A COM broker when running modes such as On-Demand for Machine. A COM broker
//   is something that acts as an intermediary, allowing creating one of several
//   possible COM objects that can service a particular COM request, ranging
//   from Privileged Services that can offer full functionality for a particular
//   set of interfaces, to Medium Integrity processes that can offer limited
//   (say, read-only) functionality for that same set of interfaces.
extern const char kServerSwitch[];

// This switch specifies the XPC service the server registers to listen to.
extern const char kServerServiceSwitch[];

// Valid values for the kServerServiceSwitch.
extern const char kServerUpdateServiceInternalSwitchValue[];
extern const char kServerUpdateServiceSwitchValue[];

// This switch starts the Windows service. This switch is invoked by the SCM
// either as a part of system startup (`SERVICE_AUTO_START`) or when `CoCreate`
// is called on one of several CLSIDs that the server supports.
extern const char kWindowsServiceSwitch[];

// This switch indicates that the Windows service is in the COM server mode.
// This switch is passed to `ServiceMain` by the SCM when CoCreate is called on
// one of several CLSIDs that the server supports. We expect to use the COM
// service for the following scenarios:
// * The Server for the UI when installing Machine applications.
// * The On-Demand COM Server for Machine applications.
// * COM Server for launching processes at System Integrity, i.e., an Elevator.
extern const char kComServiceSwitch[];

// Crash the program for testing purposes.
extern const char kCrashMeSwitch[];

// Runs as the Crashpad handler.
extern const char kCrashHandlerSwitch[];

// Updates the updater.
extern const char kUpdateSwitch[];

// Installs the updater.
extern const char kInstallSwitch[];

// Contains the meta installer tag. The tag is a string of arguments, separated
// by a delimiter (in this case, the delimiter is =). The tag is typically
// embedded in the program image of the metainstaller, but for testing purposes,
// the tag could be passed directly as a command line argument. The tag is
// currently encoded as a ASCII string.
extern const char kTagSwitch[];

#if BUILDFLAG(IS_WIN)
// A debug switch to indicate that --install is running from the `out` directory
// of the build. When this switch is present, the setup picks up the run time
// dependencies of the updater from the `out` directory instead of using the
// metainstaller uncompressed archive.
extern const char kInstallFromOutDir[];
#endif  // BUILDFLAG(IS_WIN)

// Uninstalls the updater.
extern const char kUninstallSwitch[];

// Uninstalls this version of the updater.
extern const char kUninstallSelfSwitch[];

// Uninstalls the updater if no apps are managed by it.
extern const char kUninstallIfUnusedSwitch[];

// Kicks off the update service. This switch is typically used for by a
// scheduled to invoke the updater periodically.
extern const char kWakeSwitch[];

// The updater needs to operate in the system context.
extern const char kSystemSwitch[];

// Runs in test mode. Currently, it exits right away.
extern const char kTestSwitch[];

// Run in recovery mode.
extern const char kRecoverSwitch[];

// The version of the program triggering recovery.
extern const char kBrowserVersionSwitch[];

// The session ID of the Omaha session triggering recovery.
extern const char kSessionIdSwitch[];

// The app ID of the program triggering recovery.
extern const char kAppGuidSwitch[];

// Disables throttling for the crash reported until the following bug is fixed:
// https://bugs.chromium.org/p/crashpad/issues/detail?id=23
extern const char kNoRateLimitSwitch[];

// The handle of an event to signal when the initialization of the main process
// is complete.
extern const char kInitDoneNotifierSwitch[];

// Enables logging.
extern const char kEnableLoggingSwitch[];

// Specifies the logging module filter and its value. Note that some call sites
// may still use different values for the logging module filter.
extern const char kLoggingModuleSwitch[];
extern const char kLoggingModuleSwitchValue[];

// Specifies the application that the Updater needs to install.
extern const char kAppIdSwitch[];

// Specifies the version of the application that the updater needs to register.
extern const char kAppVersionSwitch[];

// Specifies that the Updater should perform some minimal checks to verify that
// it is operational/healthy. This is for backward compatibility with Omaha 3.
// Omaha 3 runs "GoogleUpdate.exe /healthcheck" and expects an exit code of
// HRESULT SUCCESS, i.e., S_OK, in which case it will hand off the installation
// to Omaha 4.
extern const char kHealthCheckSwitch[];

// Specifies the handoff request argument. On Windows, the request may
// be from legacy updaters which pass the argument in the format of
// `/handoff <install-args-details>`. Manual argument parsing is needed for that
// scenario.
extern const char kHandoffSwitch[];

// File system paths.
//
// The directory name where CRX apps get installed. This is provided for demo
// purposes, since products installed by this updater will be installed in
// their specific locations.
extern const char kAppsDir[];

// The name of the uninstall script which is invoked by the --uninstall switch.
extern const char kUninstallScript[];

// Developer override keys.
extern const char kDevOverrideKeyUrl[];
extern const char kDevOverrideKeyUseCUP[];
extern const char kDevOverrideKeyInitialDelay[];
extern const char kDevOverrideKeyServerKeepAliveSeconds[];
extern const char kDevOverrideKeyCrxVerifierFormat[];

// File name of developer overrides file.
extern const char kDevOverrideFileName[];

// Timing constants.
#if BUILDFLAG(IS_WIN)
// How long to wait for an application installer (such as
// chrome_installer.exe) to complete.
constexpr int kWaitForAppInstallerSec = 60;

// How often the installer progress from registry is sampled. This value may
// be changed to provide a smoother progress experience (crbug.com/1067475).
constexpr int kWaitForInstallerProgressSec = 1;
#elif BUILDFLAG(IS_MAC)
// How long to wait for launchd changes to be reported by launchctl.
constexpr int kWaitForLaunchctlUpdateSec = 5;
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_MAC)
// The user defaults suite name.
extern const char kUserDefaultsSuiteName[];
#endif  // BUILDFLAG(IS_MAC)

// Install Errors.
//
// Specific install errors for the updater are reported in such a way that
// their range does not conflict with the range of generic errors defined by
// the |update_client| module.
constexpr int kCustomInstallErrorBase =
    static_cast<int>(update_client::InstallError::CUSTOM_ERROR_BASE);

// The install directory for the application could not be created.
constexpr int kErrorCreateAppInstallDirectory = kCustomInstallErrorBase;

// The install params are missing. This usually means that the update
// response does not include the name of the installer and its command line
// arguments.
constexpr int kErrorMissingInstallParams = kCustomInstallErrorBase + 1;

// The file specified by the manifest |run| attribute could not be found
// inside the CRX.
constexpr int kErrorMissingRunableFile = kCustomInstallErrorBase + 2;

// Running the application installer failed.
constexpr int kErrorApplicationInstallerFailed = kCustomInstallErrorBase + 3;

// Error codes.
//
// The server process may exit with any of these exit codes.
constexpr int kErrorOk = 0;

// The server could not acquire the lock needed to run.
constexpr int kErrorFailedToLockPrefsMutex = 1;

// The server candidate failed to promote itself to active.
constexpr int kErrorFailedToSwap = 2;

constexpr int kErrorRegistrationFailed = 3;
constexpr int kErrorPermissionDenied = 4;
constexpr int kErrorWaitFailedUninstall = 5;
constexpr int kErrorWaitFailedInstall = 6;
constexpr int kErrorPathServiceFailed = 7;
constexpr int kErrorComInitializationFailed = 8;
constexpr int kErrorUnknownCommandLine = 9;
constexpr int kErrorFailedToDeleteRegistryKeys = 10;
constexpr int kErrorNoVersionedDirectory = 11;
constexpr int kErrorNoBaseDirectory = 12;
constexpr int kErrorPathTooLong = 13;
constexpr int kErrorProcessLaunchFailed = 14;

// Failed to copy the updater's bundle.
constexpr int kErrorFailedToCopyBundle = 15;

// Failed to delete the updater's install folder.
constexpr int kErrorFailedToDeleteFolder = 16;

// Failed to delete the updater's data folder.
constexpr int kErrorFailedToDeleteDataFolder = 17;

// Failed to get versioned updater folder path.
constexpr int kErrorFailedToGetVersionedUpdaterFolderPath = 18;

// Failed to get the installed app bundle path.
constexpr int kErrorFailedToGetAppBundlePath = 19;

// Failed to remove the active(unversioned) update service job from Launchd.
constexpr int kErrorFailedToRemoveActiveUpdateServiceJobFromLaunchd = 20;

// Failed to remove versioned update service job from Launchd.
constexpr int kErrorFailedToRemoveCandidateUpdateServiceJobFromLaunchd = 21;

// Failed to remove versioned update service internal job from Launchd.
constexpr int kErrorFailedToRemoveUpdateServiceInternalJobFromLaunchd = 22;

// Failed to remove versioned wake job from Launchd.
constexpr int kErrorFailedToRemoveWakeJobFromLaunchd = 23;

// Failed to create the active(unversioned) update service Launchd plist.
constexpr int kErrorFailedToCreateUpdateServiceLaunchdJobPlist = 24;

// Failed to create the versioned update service Launchd plist.
constexpr int kErrorFailedToCreateVersionedUpdateServiceLaunchdJobPlist = 25;

// Failed to create the versioned update service internal Launchd plist.
constexpr int kErrorFailedToCreateUpdateServiceInternalLaunchdJobPlist = 26;

// Failed to create the versioned wake Launchd plist.
constexpr int kErrorFailedToCreateWakeLaunchdJobPlist = 27;

// Failed to start the active(unversioned) update service job.
constexpr int kErrorFailedToStartLaunchdActiveServiceJob = 28;

// Failed to start the versioned update service job.
constexpr int kErrorFailedToStartLaunchdVersionedServiceJob = 29;

// Failed to start the update service internal job.
constexpr int kErrorFailedToStartLaunchdUpdateServiceInternalJob = 30;

// Failed to start the wake job.
constexpr int kErrorFailedToStartLaunchdWakeJob = 31;

// Timed out while awaiting launchctl to become aware of the update service
// internal job.
constexpr int kErrorFailedAwaitingLaunchdUpdateServiceInternalJob = 32;

// Policy Management constants.
// The maximum value allowed for policy AutoUpdateCheckPeriodMinutes.
constexpr int kMaxAutoUpdateCheckPeriodMinutes = 43200;

// The maximum value allowed for policy UpdatesSuppressedDurationMin.
constexpr int kMaxUpdatesSuppressedDurationMinutes = 960;

extern const char kProxyModeDirect[];
extern const char kProxyModeAutoDetect[];
extern const char kProxyModePacScript[];
extern const char kProxyModeFixedServers[];
extern const char kProxyModeSystem[];

extern const char kDownloadPreferenceCacheable[];

constexpr int kPolicyNotSet = -1;
constexpr int kPolicyDisabled = 0;
constexpr int kPolicyEnabled = 1;
constexpr int kPolicyEnabledMachineOnly = 4;
constexpr int kPolicyManualUpdatesOnly = 2;
constexpr int kPolicyAutomaticUpdatesOnly = 3;

constexpr bool kInstallPolicyDefault = kPolicyEnabled;
constexpr bool kUpdatePolicyDefault = kPolicyEnabled;

constexpr int kUninstallPingReasonUninstalled = 0;
constexpr int kUninstallPingReasonUserNotAnOwner = 1;

// The file downloaded to a temporary location could not be moved.
constexpr int kErrorFailedToMoveDownloadedFile = 5;

constexpr double kInitialDelay = 60;
constexpr int kServerKeepAliveSeconds = 10;

// The maximum number of server starts before the updater uninstalls itself
// while waiting for the first app registration.
constexpr int kMaxServerStartsBeforeFirstReg = 24;

// These are GoogleUpdate error codes, which must be retained by this
// implementation in order to be backward compatible with the existing update
// client code in Chrome.
constexpr int GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY = 0x80040812;
constexpr int GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY = 0x80040813;
constexpr int GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY_MANUAL = 0x8004081f;
constexpr int GOOPDATEINSTALL_E_INSTALLER_FAILED = 0x80040902;

}  // namespace updater

#endif  // CHROME_UPDATER_CONSTANTS_H_
