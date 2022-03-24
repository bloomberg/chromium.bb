// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_constants.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "extensions/common/constants.h"

namespace extension_urls {

const char kWebstoreSourceField[] = "utm_source";

const char kLaunchSourceAppList[] = "chrome-app-launcher";
const char kLaunchSourceAppListSearch[] = "chrome-app-launcher-search";
const char kLaunchSourceAppListInfoDialog[] = "chrome-app-launcher-info-dialog";

}  // namespace extension_urls

namespace extension_misc {

const char kCalendarAppId[] = "ejjicmeblgpmajnghnpcppodonldlgfn";
const char kCloudPrintAppId[] = "mfehgcgbbipciphmccgaenjidiccnmng";
const char kDataSaverExtensionId[] = "pfmgfdlgomnbgkofeojodiodmgpgmkac";
const char kDocsOfflineExtensionId[] = "ghbmnnjooekpmoecnnnilnnbdlolhkhi";
const char kEnterpriseWebStoreAppId[] = "afchcafgojfnemjkcbhfekplkmjaldaa";
const char kGoogleMapsAppId[] = "lneaknkopdijkpnocmklfnjbeapigfbh";
const char kGooglePhotosAppId[] = "hcglmfcclpfgljeaiahehebeoaiicbko";
const char kGooglePlayBooksAppId[] = "mmimngoggfoobjdlefbcabngfnmieonb";
const char kGooglePlayMoviesAppId[] = "gdijeikdkaembjbdobgfkoidjkpbmlkd";
const char kGooglePlayMusicAppId[] = "icppfcnhkcmnfdhfhphakoifcfokfdhg";
const char kGooglePlusAppId[] = "dlppkpafhbajpcmmoheippocdidnckmm";
const char kIdentityApiUiAppId[] = "ahjaciijnoiaklcomgnblndopackapon";
const char kTextEditorAppId[] = "mmfbcljfglbokpmkimbfghdkjmjhdgbg";
const char kInAppPaymentsSupportAppId[] = "nmmhkkegccagdldgiimedpiccmgmieda";

const char* const kBuiltInFirstPartyExtensionIds[] = {
    kCalculatorAppId,
    kCalendarAppId,
    kCloudPrintAppId,
    kDataSaverExtensionId,
    kDocsOfflineExtensionId,
    kGoogleDriveAppId,
    kEnterpriseWebStoreAppId,
    kGmailAppId,
    kGoogleDocsAppId,
    kGoogleMapsAppId,
    kGooglePhotosAppId,
    kGooglePlayBooksAppId,
    kGooglePlayMoviesAppId,
    kGooglePlayMusicAppId,
    kGooglePlusAppId,
    kGoogleSheetsAppId,
    kGoogleSlidesAppId,
    kIdentityApiUiAppId,
    kTextEditorAppId,
    kInAppPaymentsSupportAppId,
#if defined(OS_CHROMEOS)
    kAssessmentAssistantExtensionId,
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
    kAccessibilityCommonExtensionId,
    kSelectToSpeakExtensionId,
    kSwitchAccessExtensionId,
    kFilesManagerAppId,
    kFirstRunDialogId,
    kEspeakSpeechSynthesisExtensionId,
    kGoogleSpeechSynthesisExtensionId,
    kWallpaperManagerId,
#endif        // BUILDFLAG(IS_CHROMEOS_ASH)
    nullptr,  // Null-terminated array.
};

#if defined(OS_CHROMEOS)
const char kAssessmentAssistantExtensionId[] =
    "gndmhdcefbhlchkhipcnnbkcmicncehk";
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kAccessibilityCommonExtensionId[] =
    "egfdjlfmgnehecnclamagfafdccgfndp";
const char kAccessibilityCommonExtensionPath[] = "chromeos/accessibility";
const char kAccessibilityCommonManifestFilename[] =
    "accessibility_common_manifest.json";
const char kAccessibilityCommonGuestManifestFilename[] =
    "accessibility_common_manifest_guest.json";
const char kChromeVoxExtensionPath[] = "chromeos/accessibility";
const char kChromeVoxManifestFilename[] = "chromevox_manifest.json";
const char kChromeVoxGuestManifestFilename[] = "chromevox_manifest_guest.json";
const char kEnhancedNetworkTtsExtensionId[] =
    "jacnkoglebceckolkoapelihnglgaicd";
const char kEnhancedNetworkTtsExtensionPath[] = "chromeos/accessibility";
const char kEnhancedNetworkTtsManifestFilename[] =
    "enhanced_network_tts_manifest.json";
const char kEnhancedNetworkTtsGuestManifestFilename[] =
    "enhanced_network_tts_manifest_guest.json";
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const char kAppStateNotInstalled[] = "not_installed";
const char kAppStateInstalled[] = "installed";
const char kAppStateDisabled[] = "disabled";
const char kAppStateRunning[] = "running";
const char kAppStateCannotRun[] = "cannot_run";
const char kAppStateReadyToRun[] = "ready_to_run";

const char kMediaFileSystemPathPart[] = "_";
const char kExtensionRequestTimestamp[] = "timestamp";
const char kExtensionWorkflowJustification[] = "justification";
}  // namespace extension_misc
