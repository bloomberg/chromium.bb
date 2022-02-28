// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/tag.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#if defined(OS_MAC)
#import "chrome/updater/mac/mac_util.h"
#endif

namespace updater {
namespace {

const char kHexString[] = "0123456789ABCDEF";
inline char IntToHex(int i) {
  DCHECK_GE(i, 0) << i << " not a hex value";
  DCHECK_LE(i, 15) << i << " not a hex value";
  return kHexString[i];
}

// A fast bit-vector map for ascii characters.
//
// Internally stores 256 bits in an array of 8 ints.
// Does quick bit-flicking to lookup needed characters.
struct Charmap {
  bool Contains(unsigned char c) const {
    return ((map[c >> 5] & (1 << (c & 31))) != 0);
  }

  uint32_t map[8] = {};
};

// Everything except alphanumerics and !'()*-._~
// See RFC 2396 for the list of reserved characters.
static const Charmap kQueryCharmap = {{0xffffffffL, 0xfc00987dL, 0x78000001L,
                                       0xb8000001L, 0xffffffffL, 0xffffffffL,
                                       0xffffffffL, 0xffffffffL}};

// Given text to escape and a Charmap defining which values to escape,
// return an escaped string.  If use_plus is true, spaces are converted
// to +, otherwise, if spaces are in the charmap, they are converted to
// %20. And if keep_escaped is true, %XX will be kept as it is, otherwise, if
// '%' is in the charmap, it is converted to %25.
std::string Escape(base::StringPiece text,
                   const Charmap& charmap,
                   bool use_plus,
                   bool keep_escaped = false) {
  std::string escaped;
  escaped.reserve(text.length() * 3);
  for (unsigned int i = 0; i < text.length(); ++i) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    if (use_plus && ' ' == c) {
      escaped.push_back('+');
    } else if (keep_escaped && '%' == c && i + 2 < text.length() &&
               base::IsHexDigit(text[i + 1]) && base::IsHexDigit(text[i + 2])) {
      escaped.push_back('%');
    } else if (charmap.Contains(c)) {
      escaped.push_back('%');
      escaped.push_back(IntToHex(c >> 4));
      escaped.push_back(IntToHex(c & 0xf));
    } else {
      escaped.push_back(c);
    }
  }
  return escaped;
}

std::string EscapeQueryParamValue(base::StringPiece text, bool use_plus) {
  return Escape(text, kQueryCharmap, use_plus);
}

}  // namespace

absl::optional<base::FilePath> GetBaseDirectory(UpdaterScope scope) {
  absl::optional<base::FilePath> app_data_dir;
#if defined(OS_WIN)
  base::FilePath path;
  if (!base::PathService::Get(scope == UpdaterScope::kSystem
                                  ? base::DIR_PROGRAM_FILES
                                  : base::DIR_LOCAL_APP_DATA,
                              &path)) {
    LOG(ERROR) << "Can't retrieve app data directory.";
    return absl::nullopt;
  }
  app_data_dir = path;
#elif defined(OS_MAC)
  app_data_dir = GetApplicationSupportDirectory(scope);
  if (!app_data_dir) {
    LOG(ERROR) << "Can't retrieve app data directory.";
    return absl::nullopt;
  }
#endif
  const auto product_data_dir =
      app_data_dir->AppendASCII(COMPANY_SHORTNAME_STRING)
          .AppendASCII(PRODUCT_FULLNAME_STRING);
  if (!base::CreateDirectory(product_data_dir)) {
    LOG(ERROR) << "Can't create base directory: " << product_data_dir;
    return absl::nullopt;
  }
  return product_data_dir;
}

absl::optional<base::FilePath> GetVersionedDirectory(UpdaterScope scope) {
  const absl::optional<base::FilePath> product_dir = GetBaseDirectory(scope);
  if (!product_dir) {
    LOG(ERROR) << "Failed to get the base directory.";
    return absl::nullopt;
  }

  const auto versioned_dir = product_dir->AppendASCII(kUpdaterVersion);
  if (!base::CreateDirectory(versioned_dir)) {
    LOG(ERROR) << "Can't create versioned directory.";
    return absl::nullopt;
  }

  return versioned_dir;
}

absl::optional<base::FilePath> GetVersionedUpdaterFolderPathForVersion(
    UpdaterScope scope,
    const base::Version& version) {
  const absl::optional<base::FilePath> path = GetUpdaterFolderPath(scope);
  if (!path)
    return absl::nullopt;
  return path->AppendASCII(version.GetString());
}

absl::optional<base::FilePath> GetVersionedUpdaterFolderPath(
    UpdaterScope scope) {
  return GetVersionedUpdaterFolderPathForVersion(
      scope, base::Version(kUpdaterVersion));
}

absl::optional<tagging::TagArgs> GetTagArgs() {
  static const absl::optional<tagging::TagArgs> tag_args =
      []() -> absl::optional<tagging::TagArgs> {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    const std::string tag = command_line->GetSwitchValueASCII(kTagSwitch);
    if (tag.empty())
      return absl::nullopt;
    tagging::TagArgs tag_args;
    const tagging::ErrorCode error =
        tagging::Parse(tag, absl::nullopt, &tag_args);
    VLOG_IF(1, error != tagging::ErrorCode::kSuccess)
        << "Tag parsing returned " << error << ".";
    return error == tagging::ErrorCode::kSuccess ? absl::make_optional(tag_args)
                                                 : absl::nullopt;
  }();

  return tag_args;
}

base::CommandLine MakeElevated(base::CommandLine command_line) {
#if defined(OS_MAC)
  command_line.PrependWrapper("/usr/bin/sudo");
#endif
  return command_line;
}

// The log file is created in DIR_LOCAL_APP_DATA or DIR_ROAMING_APP_DATA.
void InitLogging(UpdaterScope updater_scope,
                 const base::FilePath::StringType& filename) {
  logging::LoggingSettings settings;
  const absl::optional<base::FilePath> log_dir =
      GetBaseDirectory(updater_scope);
  if (!log_dir) {
    LOG(ERROR) << "Error getting base dir.";
    return;
  }
  const base::FilePath log_file = log_dir->Append(filename);
  settings.log_file_path = log_file.value().c_str();
  settings.logging_dest = logging::LOG_TO_ALL;
  logging::InitLogging(settings);
  logging::SetLogItems(/*enable_process_id=*/true,
                       /*enable_thread_id=*/true,
                       /*enable_timestamp=*/true,
                       /*enable_tickcount=*/false);
  VLOG(1) << "Log file: " << settings.log_file_path;
}

// This function and the helper functions are copied from net/base/url_util.cc
// to avoid the dependency on //net.
GURL AppendQueryParameter(const GURL& url,
                          const std::string& name,
                          const std::string& value) {
  std::string query(url.query());

  if (!query.empty())
    query += "&";

  query += (EscapeQueryParamValue(name, true) + "=" +
            EscapeQueryParamValue(value, true));
  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  return url.ReplaceComponents(replacements);
}

#if defined(OS_LINUX)

// TODO(crbug.com/1276188) - implement the functions below.
absl::optional<base::FilePath> GetUpdaterFolderPath(UpdaterScope scope) {
  NOTIMPLEMENTED();
  return absl::nullopt;
}

base::FilePath GetExecutableRelativePath() {
  NOTIMPLEMENTED();
  return base::FilePath();
}

bool PathOwnedByUser(const base::FilePath& path) {
  NOTIMPLEMENTED();
  return false;
}

#endif  // OS_LINUX
}  // namespace updater
