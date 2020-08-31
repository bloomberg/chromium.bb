// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_constants.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "extensions/common/constants.h"

namespace extension_urls {

const char kWebstoreSourceField[] = "utm_source";

const char kLaunchSourceAppList[] = "chrome-app-launcher";
const char kLaunchSourceAppListSearch[] = "chrome-app-launcher-search";
const char kLaunchSourceAppListInfoDialog[] = "chrome-app-launcher-info-dialog";

}  // namespace extension_urls

namespace extension_misc {

const char kCalendarAppId[] = "ejjicmeblgpmajnghnpcppodonldlgfn";
const char kChromeRemoteDesktopAppId[] = "gbchcmhmhahfdphkhkmpfmihenigjmpp";
const char kCloudPrintAppId[] = "mfehgcgbbipciphmccgaenjidiccnmng";
const char kDataSaverExtensionId[] = "pfmgfdlgomnbgkofeojodiodmgpgmkac";
const char kDocsOfflineExtensionId[] = "ghbmnnjooekpmoecnnnilnnbdlolhkhi";
const char kDriveHostedAppId[] = "apdfllckaahabafndbhieahigkjlhalf";
const char kEnterpriseWebStoreAppId[] = "afchcafgojfnemjkcbhfekplkmjaldaa";
const char kGmailAppId[] = "pjkljhegncpnkpknbcohdijeoejaedia";
const char kGoogleDocAppId[] = "aohghmighlieiainnegkcijnfilokake";
const char kGoogleMapsAppId[] = "lneaknkopdijkpnocmklfnjbeapigfbh";
const char kGooglePhotosAppId[] = "hcglmfcclpfgljeaiahehebeoaiicbko";
const char kGooglePlayBooksAppId[] = "mmimngoggfoobjdlefbcabngfnmieonb";
const char kGooglePlayMoviesAppId[] = "gdijeikdkaembjbdobgfkoidjkpbmlkd";
const char kGooglePlayMusicAppId[] = "icppfcnhkcmnfdhfhphakoifcfokfdhg";
const char kGooglePlusAppId[] = "dlppkpafhbajpcmmoheippocdidnckmm";
const char kGoogleSheetsAppId[] = "felcaaldnbdncclmgdcncolpebgiejap";
const char kGoogleSlidesAppId[] = "aapocclcgogkmnckokdopfmhonfmgoek";
const char kHTermAppId[] = "pnhechapfaindjhompbnflcldabbghjo";
const char kHTermDevAppId[] = "okddffdblfhhnmhodogpojmfkjmhinfp";
const char kIdentityApiUiAppId[] = "ahjaciijnoiaklcomgnblndopackapon";
const char kTextEditorAppId[] = "mmfbcljfglbokpmkimbfghdkjmjhdgbg";
const char kInAppPaymentsSupportAppId[] = "nmmhkkegccagdldgiimedpiccmgmieda";
const char kMediaRouterStableExtensionId[] = "pkedcjkdefgpdelpbcmbmeomcjbeemfm";

const char* const kBuiltInFirstPartyExtensionIds[] = {
    kCalculatorAppId,
    kCalendarAppId,
    kChromeRemoteDesktopAppId,
    kCloudPrintAppId,
    kDataSaverExtensionId,
    kDocsOfflineExtensionId,
    kDriveHostedAppId,
    kEnterpriseWebStoreAppId,
    kGmailAppId,
    kGoogleDocAppId,
    kGoogleMapsAppId,
    kGooglePhotosAppId,
    kGooglePlayBooksAppId,
    kGooglePlayMoviesAppId,
    kGooglePlayMusicAppId,
    kGooglePlusAppId,
    kGoogleSheetsAppId,
    kGoogleSlidesAppId,
    kHTermAppId,
    kHTermDevAppId,
    kIdentityApiUiAppId,
    kTextEditorAppId,
    kInAppPaymentsSupportAppId,
    kMediaRouterStableExtensionId,
#if defined(OS_CHROMEOS)
    kAssessmentAssistantExtensionId,
    kAutoclickExtensionId,
    kSelectToSpeakExtensionId,
    kSwitchAccessExtensionId,
    kFilesManagerAppId,
    kFirstRunDialogId,
    kEspeakSpeechSynthesisExtensionId,
    kGoogleSpeechSynthesisExtensionId,
    kWallpaperManagerId,
    kZipArchiverExtensionId,
#endif        // defined(OS_CHROMEOS)
    nullptr,  // Null-terminated array.
};

#if defined(OS_CHROMEOS)
const char kAssessmentAssistantExtensionId[] =
    "gndmhdcefbhlchkhipcnnbkcmicncehk";
const char kAutoclickExtensionId[] = "egfdjlfmgnehecnclamagfafdccgfndp";
const char kAutoclickExtensionPath[] = "chromeos/accessibility/autoclick";
const char kChromeVoxExtensionPath[] = "chromeos/accessibility";
const char kChromeVoxManifestFilename[] = "chromevox_manifest.json";
const char kChromeVoxGuestManifestFilename[] = "chromevox_manifest_guest.json";
const char kSelectToSpeakExtensionId[] = "klbcgckkldhdhonijdbnhhaiedfkllef";
const char kSelectToSpeakExtensionPath[] = "chromeos/accessibility";
const char kSelectToSpeakManifestFilename[] = "select_to_speak_manifest.json";
const char kSelectToSpeakGuestManifestFilename[] =
    "select_to_speak_manifest_guest.json";
const char kSwitchAccessExtensionId[] = "pmehocpgjmkenlokgjfkaichfjdhpeol";
const char kSwitchAccessExtensionPath[] = "chromeos/accessibility";
const char kSwitchAccessManifestFilename[] = "switch_access_manifest.json";
const char kSwitchAccessGuestManifestFilename[] =
    "switch_access_manifest_guest.json";
const char kGuestManifestFilename[] = "manifest_guest.json";
const char kConnectivityDiagnosticsPath[] =
    "/usr/share/chromeos-assets/connectivity_diagnostics";
const char kConnectivityDiagnosticsLauncherPath[] =
    "/usr/share/chromeos-assets/connectivity_diagnostics_launcher";
const char kFirstRunDialogId[] = "jdgcneonijmofocbhmijhacgchbihela";
const char kEspeakSpeechSynthesisExtensionPath[] =
    "/usr/share/chromeos-assets/speech_synthesis/espeak-ng";
const char kEspeakSpeechSynthesisExtensionId[] =
    "dakbfdmgjiabojdgbiljlhgjbokobjpg";
const char kGoogleSpeechSynthesisExtensionPath[] =
    "/usr/share/chromeos-assets/speech_synthesis/patts";
const char kGoogleSpeechSynthesisExtensionId[] =
    "gjjabgpgjpampikjhjpfhneeoapjbjaf";
const char kWallpaperManagerId[] = "obklkkbkpaoaejdabbfldmcfplpdgolj";
const char kZipArchiverExtensionId[] = "dmboannefpncccogfdikhmhpmdnddgoe";
const char kZipArchiverExtensionPath[] = "chromeos/zip_archiver";
const char kCameraAppPath[] = "chromeos/camera";
#endif  // defined(CHROME_OS)

const char kAppStateNotInstalled[] = "not_installed";
const char kAppStateInstalled[] = "installed";
const char kAppStateDisabled[] = "disabled";
const char kAppStateRunning[] = "running";
const char kAppStateCannotRun[] = "cannot_run";
const char kAppStateReadyToRun[] = "ready_to_run";

const char kMediaFileSystemPathPart[] = "_";
const char kExtensionRequestTimestamp[] = "timestamp";
}  // namespace extension_misc
