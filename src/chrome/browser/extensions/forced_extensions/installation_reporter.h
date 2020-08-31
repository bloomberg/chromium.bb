// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_REPORTER_H_
#define CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_REPORTER_H_

#include <map>
#include <utility>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/optional.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/install/sandboxed_unpacker_failure_reason.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "extensions/browser/updater/safe_manifest_parser.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

// Helper class to save and retrieve extension installation stage and failure
// reasons.
class InstallationReporter : public KeyedService {
 public:
  // Stage of extension installing process. Typically forced extensions from
  // policies should go through all stages in this order, other extensions skip
  // CREATED stage.
  // Note: enum used for UMA. Do NOT reorder or remove entries. Don't forget to
  // update enums.xml (name: ExtensionInstallationStage) when adding new
  // entries.
  enum class Stage {
    // Extension found in ForceInstall policy and added to
    // ExtensionManagement::settings_by_id_.
    CREATED = 0,

    // TODO(crbug.com/989526): stages from NOTIFIED_FROM_MANAGEMENT to
    // SEEN_BY_EXTERNAL_PROVIDER are temporary ones for investigation. Remove
    // then after investigation will complete and we'll be confident in
    // extension handling between CREATED and PENDING.

    // ExtensionManagement class is about to pass extension with
    // INSTALLATION_FORCED mode to its observers.
    NOTIFIED_FROM_MANAGEMENT = 5,

    // ExtensionManagement class is about to pass extension with other mode to
    // its observers.
    NOTIFIED_FROM_MANAGEMENT_NOT_FORCED = 6,

    // ExternalPolicyLoader with FORCED type fetches extension from
    // ExtensionManagement.
    SEEN_BY_POLICY_LOADER = 7,

    // ExternalProviderImpl receives extension.
    SEEN_BY_EXTERNAL_PROVIDER = 8,

    // Extension added to PendingExtensionManager.
    PENDING = 1,

    // Extension added to ExtensionDownloader.
    DOWNLOADING = 2,

    // Extension archive downloaded and is about to be unpacked/checked/etc.
    INSTALLING = 3,

    // Extension installation finished (either successfully or not).
    COMPLETE = 4,

    // Magic constant used by the histogram macros.
    // Always update it to the max value.
    kMaxValue = SEEN_BY_EXTERNAL_PROVIDER,
  };

  // Enum used for UMA. Do NOT reorder or remove entries. Don't forget to
  // update enums.xml (name: ExtensionInstallationFailureReason) when adding new
  // entries.
  enum class FailureReason {
    // Reason for the failure is not reported. Typically this should not happen,
    // because if we know that we need to install an extension, it should
    // immediately switch to CREATED stage leading to IN_PROGRESS failure
    // reason, not UNKNOWN.
    UNKNOWN = 0,

    // Invalid id of the extension.
    INVALID_ID = 1,

    // Error during parsing extension individual settings.
    MALFORMED_EXTENSION_SETTINGS = 2,

    // The extension is marked as replaced by ARC app.
    REPLACED_BY_ARC_APP = 3,

    // Malformed extension dictionary for the extension.
    MALFORMED_EXTENSION_DICT = 4,

    // The extension format from extension dict is not supported.
    NOT_SUPPORTED_EXTENSION_DICT = 5,

    // Invalid file path in the extension dict.
    MALFORMED_EXTENSION_DICT_FILE_PATH = 6,

    // Invalid version in the extension dict.
    MALFORMED_EXTENSION_DICT_VERSION = 7,

    // Invalid updated URL in the extension dict.
    MALFORMED_EXTENSION_DICT_UPDATE_URL = 8,

    // The extension doesn't support browser locale.
    LOCALE_NOT_SUPPORTED = 9,

    // The extension marked as it shouldn't be installed.
    NOT_PERFORMING_NEW_INSTALL = 10,

    // Profile is older than supported by the extension.
    TOO_OLD_PROFILE = 11,

    // The extension can't be installed for enterprise.
    DO_NOT_INSTALL_FOR_ENTERPRISE = 12,

    // The extension is already installed.
    ALREADY_INSTALLED = 13,

    // The download of the crx failed.
    CRX_FETCH_FAILED = 14,

    // Failed to fetch the manifest for this extension.
    MANIFEST_FETCH_FAILED = 15,

    // The manifest couldn't be parsed.
    MANIFEST_INVALID = 16,

    // The manifest was fetched and parsed, and there are no updates for this
    // extension.
    NO_UPDATE = 17,

    // The crx was downloaded, but failed to install.
    // Corresponds to CrxInstallErrorType.
    CRX_INSTALL_ERROR_DECLINED = 18,
    CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE = 19,
    CRX_INSTALL_ERROR_OTHER = 20,

    // Extensions without update url should receive a default one, but somewhy
    // this didn't work. Internal error, should never happen.
    NO_UPDATE_URL = 21,

    // Extension failed to add to PendingExtensionManager.
    PENDING_ADD_FAILED = 22,

    // ExtensionDownloader refuses to start downloading this extensions
    // (possible reasons: invalid ID/URL).
    DOWNLOADER_ADD_FAILED = 23,

    // Extension (at the moment of check) is not installed nor some installation
    // error reported, so extension is being installed now, stuck in some stage
    // or some failure was not reported. See enum Stage for more details.
    // This option is a failure only in the sense that we failed to install
    // extension in required time.
    IN_PROGRESS = 24,

    // The download of the crx failed.
    CRX_FETCH_URL_EMPTY = 25,

    // The download of the crx failed.
    CRX_FETCH_URL_INVALID = 26,

    // Magic constant used by the histogram macros.
    // Always update it to the max value.
    kMaxValue = CRX_FETCH_URL_INVALID,
  };
  // Status for the update check returned by server while fetching manifest.
  // Enum used for UMA. Do NOT reorder or remove entries. Don't forget to update
  // enums.xml (name: UpdateCheckStatus) when adding new entries.
  enum class UpdateCheckStatus {
    // Technically it may happen that update server return some unknown value or
    // no value.
    kUnknown = 0,

    // An update is available and should be applied.
    kOk = 1,

    // No update is available for this application at this time.
    kNoUpdate = 2,

    // Server encountered an unknown internal error.
    kErrorInternal = 3,

    // The server attempted to serve an update, but could not provide a valid
    // hash for the download.
    kErrorHash = 4,

    // The application is running on an incompatible operating system.
    kErrorOsNotSupported = 5,

    // The application is running on an incompatible hardware.
    kErrorHardwareNotSupported = 6,

    // This application is incompatible with this version of the protocol.
    kErrorUnsupportedProtocol = 7,

    // Magic constant used by the histogram macros.
    // Always update it to the max value.
    kMaxValue = kErrorUnsupportedProtocol,
  };

  // Contains information about extension installation: failure reason, if any
  // reported, specific details in case of CRX install error, current
  // installation stage if known.
  struct InstallationData {
    InstallationData();
    InstallationData(const InstallationData&);

    base::Optional<extensions::InstallationReporter::Stage> install_stage;
    base::Optional<extensions::ExtensionDownloaderDelegate::Stage>
        downloading_stage;
    base::Optional<extensions::ExtensionDownloaderDelegate::CacheStatus>
        downloading_cache_status;
    base::Optional<extensions::InstallationReporter::FailureReason>
        failure_reason;
    base::Optional<extensions::CrxInstallErrorDetail> install_error_detail;
    // Network error codes when failure_reason is CRX_FETCH_FAILED or
    // MANIFEST_FETCH_FAILED.
    base::Optional<int> network_error_code;
    base::Optional<int> response_code;
    // Number of fetch tries made when failure reason is CRX_FETCH_FAILED or
    // MANIFEST_FETCH_FAILED.
    base::Optional<int> fetch_tries;
    // Unpack failure reason in case of
    // CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE.
    base::Optional<extensions::SandboxedUnpackerFailureReason>
        unpacker_failure_reason;
    // Type of extension, assigned when CRX installation error detail is
    // DISALLOWED_BY_POLICY.
    base::Optional<Manifest::Type> extension_type;
    // Type of update check status received from server when manifest was
    // fetched.
    base::Optional<UpdateCheckStatus> update_check_status;
    // Error detail when the fetched manifest was invalid. This includes errors
    // occurred while parsing the manifest and errors occurred due to the
    // internal details of the parsed manifest.
    base::Optional<ManifestInvalidError> manifest_invalid_error;
  };

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;

    virtual void OnExtensionInstallationFailed(const ExtensionId& id,
                                               FailureReason reason) {}

    // Called when any change happens. For production please use more specific
    // methods (create one if necessary).
    virtual void OnExtensionDataChangedForTesting(
        const ExtensionId& id,
        const content::BrowserContext* context,
        const InstallationData& data) {}
  };

  explicit InstallationReporter(const content::BrowserContext* context);

  ~InstallationReporter() override;

  // Convenience function to get the InstallationReporter for a BrowserContext.
  static InstallationReporter* Get(content::BrowserContext* context);

  // Reports detailed error type when extension fails to install with failure
  // reason MANIFEST_INVALID. See InstallationData::manifest_invalid_error
  // for more details.
  void ReportManifestInvalidFailure(const ExtensionId& id,
                                    ManifestInvalidError error);

  // Remembers failure reason and in-progress stages in memory.
  void ReportInstallationStage(const ExtensionId& id, Stage stage);
  void ReportFetchError(
      const ExtensionId& id,
      FailureReason reason,
      const ExtensionDownloaderDelegate::FailureData& failure_data);
  void ReportFailure(const ExtensionId& id, FailureReason reason);
  void ReportDownloadingStage(const ExtensionId& id,
                              ExtensionDownloaderDelegate::Stage stage);
  void ReportDownloadingCacheStatus(
      const ExtensionId& id,
      ExtensionDownloaderDelegate::CacheStatus cache_status);
  void ReportManifestUpdateCheckStatus(const ExtensionId& id,
                                       const std::string& status);
  // Assigns the extension type. See InstallationData::extension_type for
  // more details.
  void ReportExtensionTypeForPolicyDisallowedExtension(
      const ExtensionId& id,
      Manifest::Type extension_type);
  void ReportCrxInstallError(const ExtensionId& id,
                             FailureReason reason,
                             CrxInstallErrorDetail crx_install_error);
  void ReportSandboxedUnpackerFailureReason(
      const ExtensionId& id,
      SandboxedUnpackerFailureReason unpacker_failure_reason);

  // Retrieves known information for installation of extension |id|.
  // Returns empty data if not found.
  InstallationData Get(const ExtensionId& id);
  static std::string GetFormattedInstallationData(const InstallationData& data);

  // Clears all collected failures and stages.
  void Clear();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Helper function to report installation failures to the observers.
  void NotifyObserversOfFailure(const ExtensionId& id,
                                FailureReason reason,
                                const InstallationData& data);

  const content::BrowserContext* browser_context_;

  std::map<ExtensionId, InstallationReporter::InstallationData>
      installation_data_map_;

  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(InstallationReporter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_REPORTER_H_
