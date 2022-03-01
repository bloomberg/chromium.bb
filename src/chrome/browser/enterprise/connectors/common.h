// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_COMMON_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_COMMON_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/supports_user_data.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "content/public/browser/download_manager_delegate.h"
#include "url/gurl.h"

namespace enterprise_connectors {

// Alias to reduce verbosity when using TriggeredRule::Actions.
using TriggeredRule = ContentAnalysisResponse::Result::TriggeredRule;

// Keys used to read a connector's policy values.
constexpr char kKeyServiceProvider[] = "service_provider";
constexpr char kKeyEnable[] = "enable";
constexpr char kKeyDisable[] = "disable";
constexpr char kKeyUrlList[] = "url_list";
constexpr char kKeyTags[] = "tags";
constexpr char kKeyBlockUntilVerdict[] = "block_until_verdict";
constexpr char kKeyBlockPasswordProtected[] = "block_password_protected";
constexpr char kKeyBlockLargeFiles[] = "block_large_files";
constexpr char kKeyBlockUnsupportedFileTypes[] = "block_unsupported_file_types";
constexpr char kKeyMinimumDataSize[] = "minimum_data_size";
constexpr char kKeyEnabledEventNames[] = "enabled_event_names";
constexpr char kKeyCustomMessages[] = "custom_messages";
constexpr char kKeyCustomMessagesTag[] = "tag";
constexpr char kKeyCustomMessagesMessage[] = "message";
constexpr char kKeyCustomMessagesLearnMoreUrl[] = "learn_more_url";
constexpr char kKeyMimeTypes[] = "mime_types";
constexpr char kKeyEnterpriseId[] = "enterprise_id";
constexpr char kKeyDomain[] = "domain";

// A MIME type string that matches all MIME types.
constexpr char kWildcardMimeType[] = "*";

enum class ReportingConnector {
  SECURITY_EVENT,
};

enum class FileSystemConnector {
  SEND_DOWNLOAD_TO_CLOUD,
};

// Enum representing if an analysis should block further interactions with the
// browser until its verdict is obtained.
enum class BlockUntilVerdict {
  NO_BLOCK = 0,
  BLOCK = 1,
};

// A struct representing a custom message and associated "learn more" URL. These
// are scoped to a tag.
struct CustomMessageData {
  std::u16string message;
  GURL learn_more_url;
};

// Structs representing settings to be used for an analysis or a report. These
// settings should only be kept and considered valid for the specific
// analysis/report they were obtained for.
struct AnalysisSettings {
  AnalysisSettings();
  AnalysisSettings(AnalysisSettings&&);
  AnalysisSettings& operator=(AnalysisSettings&&);
  ~AnalysisSettings();

  GURL analysis_url;
  std::set<std::string> tags;
  BlockUntilVerdict block_until_verdict = BlockUntilVerdict::NO_BLOCK;
  bool block_password_protected_files = false;
  bool block_large_files = false;
  bool block_unsupported_file_types = false;
  std::map<std::string, CustomMessageData> custom_message_data;

  // Minimum text size for BulkDataEntry scans. 0 means no minimum.
  size_t minimum_data_size = 100;

  // The DM token to be used for scanning. May be empty, for example if this
  // scan is initiated by APP.
  std::string dm_token = "";

  // Indicates if the scan is made at the profile level, or at the browser level
  // if false.
  bool per_profile = false;

  // ClientMetadata to include in the scanning request(s). This is populated
  // based on OnSecurityEvent and the affiliation state of the browser.
  std::unique_ptr<ClientMetadata> client_metadata;
};

struct ReportingSettings {
  ReportingSettings();
  ReportingSettings(GURL url, const std::string& dm_token, bool per_profile);
  ReportingSettings(ReportingSettings&&);
  ReportingSettings& operator=(ReportingSettings&&);
  ~ReportingSettings();

  GURL reporting_url;
  std::set<std::string> enabled_event_names;
  std::string dm_token;

  // Indicates if the report should be made for the profile, or the browser if
  // false.
  bool per_profile = false;
};

struct FileSystemSettings {
  FileSystemSettings();
  FileSystemSettings(const FileSystemSettings&);
  FileSystemSettings(FileSystemSettings&&);
  FileSystemSettings& operator=(const FileSystemSettings&);
  FileSystemSettings& operator=(FileSystemSettings&&);
  ~FileSystemSettings();

  std::string service_provider;
  GURL home;
  GURL authorization_endpoint;
  GURL token_endpoint;
  std::string enterprise_id;
  std::string email_domain;
  std::string client_id;
  std::string client_secret;
  std::vector<std::string> scopes;
  size_t max_direct_size;
  std::set<std::string> mime_types;
  // Indicates whether `mime_types` is to be used for enabling or disabling.
  bool enable_with_mime_types;
};

// Returns the pref path corresponding to a connector.
const char* ConnectorPref(AnalysisConnector connector);
const char* ConnectorPref(ReportingConnector connector);
const char* ConnectorPref(FileSystemConnector connector);
const char* ConnectorScopePref(AnalysisConnector connector);
const char* ConnectorScopePref(ReportingConnector connector);

// Returns the highest precedence action in the given parameters. Writes the tag
// field of the result containing the highest precedence action into |tag|.
TriggeredRule::Action GetHighestPrecedenceAction(
    const ContentAnalysisResponse& response,
    std::string* tag);
TriggeredRule::Action GetHighestPrecedenceAction(
    const TriggeredRule::Action& action_1,
    const TriggeredRule::Action& action_2);

// Struct used to persist metadata about a file in base::SupportsUserData
// through ScanResult.
struct FileMetadata {
  FileMetadata(
      const std::string& filename,
      const std::string& sha256,
      const std::string& mime_type,
      int64_t size,
      const ContentAnalysisResponse& scan_response = ContentAnalysisResponse());
  FileMetadata(FileMetadata&&);
  FileMetadata(const FileMetadata&);
  FileMetadata& operator=(const FileMetadata&);
  ~FileMetadata();

  std::string filename;
  std::string sha256;
  std::string mime_type;
  int64_t size;
  ContentAnalysisResponse scan_response;
};

// User data class to persist scanning results for multiple files corresponding
// to a single base::SupportsUserData object.
struct ScanResult : public base::SupportsUserData::Data {
  explicit ScanResult(FileMetadata metadata);
  ~ScanResult() override;
  static const char kKey[];

  std::vector<FileMetadata> file_metadata;
};

// User data to persist a save package's final callback allowing/denying
// completion. This is used since the callback can be called either when
// scanning completes on a block/allow verdict, when the user cancels the scan,
// or when the user bypasses scanning.
struct SavePackageScanningData : public base::SupportsUserData::Data {
  explicit SavePackageScanningData(
      content::SavePackageAllowedCallback callback);
  ~SavePackageScanningData() override;
  static const char kKey[];

  content::SavePackageAllowedCallback callback;
};

// Checks `item` for a SavePackageScanningData, and run it's callback with
// `allowed` if there is one.
void RunSavePackageScanningCallback(download::DownloadItem* item, bool allowed);

// Checks if |response| contains a negative malware verdict.
bool ContainsMalwareVerdict(const ContentAnalysisResponse& response);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_COMMON_H_
