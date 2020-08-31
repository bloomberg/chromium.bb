// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_CONSTANTS_H_
#define EXTENSIONS_COMMON_CONSTANTS_H_

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_piece_forward.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/version_info/channel.h"
#include "ui/base/layout.h"

namespace extensions {

// Scheme we serve extension content from.
extern const char kExtensionScheme[];

// The name of the manifest inside an extension.
extern const base::FilePath::CharType kManifestFilename[];

// The name of the differential fingerprint file inside an extension.
extern const base::FilePath::CharType kDifferentialFingerprintFilename[];

// The name of locale folder inside an extension.
extern const base::FilePath::CharType kLocaleFolder[];

// The name of the messages file inside an extension.
extern const base::FilePath::CharType kMessagesFilename[];

// The name of the gzipped messages file inside an extension.
extern const base::FilePath::CharType kGzippedMessagesFilename[];

// The base directory for subdirectories with platform-specific code.
extern const base::FilePath::CharType kPlatformSpecificFolder[];

// A directory reserved for metadata, generated either by the webstore
// or chrome.
extern const base::FilePath::CharType kMetadataFolder[];

// Name of the verified contents file within the metadata folder.
extern const base::FilePath::CharType kVerifiedContentsFilename[];

// Name of the computed hashes file within the metadata folder.
extern const base::FilePath::CharType kComputedHashesFilename[];

// Name of the indexed ruleset directory for the Declarative Net Request API.
extern const base::FilePath::CharType kIndexedRulesetDirectory[];

// The name of the directory inside the profile where extensions are
// installed to.
extern const char kInstallDirectoryName[];

// The name of a temporary directory to install an extension into for
// validation before finalizing install.
extern const char kTempExtensionName[];

// The file to write our decoded message catalogs to, relative to the
// extension_path.
extern const char kDecodedMessageCatalogsFilename[];

// The filename to use for a background page generated from
// background.scripts.
extern const char kGeneratedBackgroundPageFilename[];

// Path to imported modules.
extern const char kModulesDir[];

// The file extension (.crx) for extensions.
extern const base::FilePath::CharType kExtensionFileExtension[];

// The file extension (.pem) for private key files.
extern const base::FilePath::CharType kExtensionKeyFileExtension[];

// Default frequency for auto updates, if turned on.
extern const int kDefaultUpdateFrequencySeconds;

// The name of the directory inside the profile where per-app local settings
// are stored.
extern const char kLocalAppSettingsDirectoryName[];

// The name of the directory inside the profile where per-extension local
// settings are stored.
extern const char kLocalExtensionSettingsDirectoryName[];

// The name of the directory inside the profile where per-app synced settings
// are stored.
extern const char kSyncAppSettingsDirectoryName[];

// The name of the directory inside the profile where per-extension synced
// settings are stored.
extern const char kSyncExtensionSettingsDirectoryName[];

// The name of the directory inside the profile where per-extension persistent
// managed settings are stored.
extern const char kManagedSettingsDirectoryName[];

// The name of the database inside the profile where chrome-internal
// extension state resides.
extern const char kStateStoreName[];

// The name of the database inside the profile where declarative extension
// rules are stored.
extern const char kRulesStoreName[];

// The URL query parameter key corresponding to multi-login user index.
extern const char kAuthUserQueryKey[];

// Mime type strings
extern const char kMimeTypeJpeg[];
extern const char kMimeTypePng[];

// The extension id of the Web Store component application.
extern const char kWebStoreAppId[];

// The key used for signing some pieces of data from the webstore.
extern const uint8_t kWebstoreSignaturesPublicKey[];
extern const size_t kWebstoreSignaturesPublicKeySize;

// Thread identifier for the main renderer thread (as opposed to a service
// worker thread).
// This is the default thread id used for extension event listeners registered
// from a non-service worker context
extern const int kMainThreadId;

using apps::mojom::AppLaunchSource;
using apps::mojom::LaunchContainer;

// This enum is used for the launch type the user wants to use for an
// application.
// Do not remove items or re-order this enum as it is used in preferences
// and histograms.
enum LaunchType {
  LAUNCH_TYPE_INVALID = -1,
  LAUNCH_TYPE_FIRST = 0,
  LAUNCH_TYPE_PINNED = LAUNCH_TYPE_FIRST,
  LAUNCH_TYPE_REGULAR = 1,
  LAUNCH_TYPE_FULLSCREEN = 2,
  LAUNCH_TYPE_WINDOW = 3,
  NUM_LAUNCH_TYPES,

  // Launch an app in the in the way a click on the NTP would,
  // if no user pref were set.  Update this constant to change
  // the default for the NTP and chrome.management.launchApp().
  LAUNCH_TYPE_DEFAULT = LAUNCH_TYPE_REGULAR
};

// The origin of injected CSS.
enum CSSOrigin { CSS_ORIGIN_AUTHOR, CSS_ORIGIN_USER };
static const CSSOrigin CSS_ORIGIN_LAST = CSS_ORIGIN_USER;

}  // namespace extensions

namespace extension_misc {

// Matches chrome.tabs.TAB_ID_NONE.
const int kUnknownTabId = -1;

// Matches chrome.windows.WINDOW_ID_NONE.
const int kUnknownWindowId = -1;

// Matches chrome.windows.WINDOW_ID_CURRENT.
const int kCurrentWindowId = -2;

enum ExtensionIcons {
  EXTENSION_ICON_GIGANTOR = 512,
  EXTENSION_ICON_EXTRA_LARGE = 256,
  EXTENSION_ICON_LARGE = 128,
  EXTENSION_ICON_MEDIUM = 48,
  EXTENSION_ICON_SMALL = 32,
  EXTENSION_ICON_SMALLISH = 24,
  EXTENSION_ICON_BITTY = 16,
  EXTENSION_ICON_INVALID = 0,
};

// The extension id of the ChromeVox extension.
extern const char kChromeVoxExtensionId[];

// The extension id of the feedback component extension.
extern const char kFeedbackExtensionId[];

// The extension id of the PDF extension.
extern const char kPdfExtensionId[];

// The extension id of the Office Viewer component extension.
extern const char kQuickOfficeComponentExtensionId[];

// The extension id of the Office Viewer extension on the internal webstore.
extern const char kQuickOfficeInternalExtensionId[];

// The extension id of the Office Viewer extension.
extern const char kQuickOfficeExtensionId[];

// The extension id used for testing mimeHandlerPrivate.
extern const char kMimeHandlerPrivateTestExtensionId[];

// The extension id of the Camera application.
extern const char kCameraAppId[];

// The extension id of the devoloper version of Camera application.
extern const char kCameraAppDevId[];

// The extension id of the Chrome component application.
extern const char kChromeAppId[];

// Fake extension ID for the Lacros chrome browser application.
extern const char kLacrosAppId[];

// The extension id of the Files Manager application.
extern const char kFilesManagerAppId[];

// The extension id of the Calculator application.
extern const char kCalculatorAppId[];

// The extension id of the demo Calendar application.
extern const char kCalendarDemoAppId[];

// The extension id of the demo Google Docs application.
extern const char kGoogleDocsDemoAppId[];

// The extension id of the demo Google Sheets application.
extern const char kGoogleSheetsDemoAppId[];

// The extension id of the demo Google Slides application.
extern const char kGoogleSlidesDemoAppId[];

// The extension id of the Google Keep application.
extern const char kGoogleKeepAppId[];

// The extension id of the Youtube application.
extern const char kYoutubeAppId[];

// The extension id of the genius (Get Help) app.
extern const char kGeniusAppId[];

#if defined(OS_CHROMEOS)
// The extension id of the default Demo Mode Highlights app.
extern const char kHighlightsAppId[];

// The extension id of the eve Demo Mode Highlights app.
extern const char kHighlightsEveAppId[];

// The extension id of the nocturne Demo Mode Highlights app.
extern const char kHighlightsNocturneAppId[];

// The extension id of the atlas Demo Mode Highlights app.
extern const char kHighlightsAtlasAppId[];

// The extension id of the default Demo Mode screensaver app.
extern const char kScreensaverAppId[];

// The extension id of the eve Demo Mode screensaver app.
extern const char kScreensaverEveAppId[];

// The extension id of the nocturne Demo Mode screensaver app.
extern const char kScreensaverNocturneAppId[];

// The extension id of the atlas Demo Mode screensaver app.
extern const char kScreensaverAtlasAppId[];

// The extension id of the kukui Demo Mode screensaver app.
extern const char kScreensaverKukuiAppId[];

// Returns true if this app is part of the "system UI". Generally this is UI
// that that on other operating systems would be considered part of the OS,
// for example the file manager.
bool IsSystemUIApp(base::StringPiece extension_id);
#endif

// The extension id for the production version of Hangouts.
extern const char kProdHangoutsExtensionId[];

// Extension ids used by Hangouts.
extern const char* const kHangoutsExtensionIds[6];

// Error message when enterprise policy blocks scripting of webpage.
extern const char kPolicyBlockedScripting[];

// The default block size for hashing used in content verification.
extern const int kContentVerificationDefaultBlockSize;

// The minimum severity of a log or error in order to report it to the browser.
extern const logging::LogSeverity kMinimumSeverityToReportError;

// The minimum channel where Service Worker based extensions can run.
constexpr version_info::Channel kMinChannelForServiceWorkerBasedExtension =
    version_info::Channel::DEV;

}  // namespace extension_misc

#endif  // EXTENSIONS_COMMON_CONSTANTS_H_
