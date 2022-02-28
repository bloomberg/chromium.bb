// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UTIL_H_
#define CHROME_UPDATER_UTIL_H_

#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/updater/updater_scope.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

// Externally-defined printers for base types.
namespace base {

class CommandLine;
class Version;

template <class T>
std::ostream& operator<<(std::ostream& os, const absl::optional<T>& opt) {
  if (opt.has_value()) {
    return os << opt.value();
  } else {
    return os << "absl::nullopt";
  }
}

}  // namespace base

namespace updater {

namespace tagging {
struct TagArgs;
}

enum class UpdaterScope;

// Returns the base directory common to all versions of the updater. For
// instance, this function may return %localappdata%\Chromium\ChromiumUpdater
// for a user install.
absl::optional<base::FilePath> GetBaseDirectory(UpdaterScope scope);

// Returns a versioned directory under which the running version of the updater
// stores its files and data. For instance, this function may return
// %localappdata%\Chromium\ChromiumUpdater\1.2.3.4 for a user install.
absl::optional<base::FilePath> GetVersionedDirectory(UpdaterScope scope);

// For user installations:
// ~/Library/Google/GoogleUpdater/88.0.4293.0
// For system installations:
// /Library/Google/GoogleUpdater/88.0.4293.0
absl::optional<base::FilePath> GetVersionedUpdaterFolderPathForVersion(
    UpdaterScope scope,
    const base::Version& version);

// The same as GetVersionedUpdaterFolderPathForVersion, where the version is
// kUpdaterVersion.
absl::optional<base::FilePath> GetVersionedUpdaterFolderPath(
    UpdaterScope scope);

// For user installations:
// ~/Library/Google/GoogleUpdater
// For system installations:
// /Library/Google/GoogleUpdater
absl::optional<base::FilePath> GetUpdaterFolderPath(UpdaterScope scope);

#if defined(OS_MAC)
// For example: ~/Library/Google/GoogleUpdater/88.0.4293.0/GoogleUpdater.app
absl::optional<base::FilePath> GetUpdaterAppBundlePath(UpdaterScope scope);
#endif  // defined(OS_MAC)

// For user installations:
// ~/Library/Google/GoogleUpdater/88.0.4293.0/GoogleUpdater.app/Contents/
//    MacOS/GoogleUpdater
// For system installations:
// /Library/Google/GoogleUpdater/88.0.4293.0/GoogleUpdater.app/Contents/
//    MacOS/GoogleUpdater
absl::optional<base::FilePath> GetUpdaterExecutablePath(UpdaterScope scope);

// For user installations:
// ~/Library/Google/GoogleUpdater/88.0.4293.0/GoogleUpdater.app/Contents/MacOS
// For system installations:
// /Library/Google/GoogleUpdater/88.0.4293.0/GoogleUpdater.app/Contents/MacOS
absl::optional<base::FilePath> GetExecutableFolderPathForVersion(
    UpdaterScope scope,
    const base::Version& version);

// Returns a relative path to the executable, such as
// "GoogleUpdater.app/Contents/MacOS/GoogleUpdater" on macOS.
// "updater.exe" on Win.
base::FilePath GetExecutableRelativePath();

// Returns the parsed values from --tag command line argument. The function
// implementation uses lazy initialization and caching to avoid reparsing
// the tag.
absl::optional<tagging::TagArgs> GetTagArgs();

// Returns true if the user running the updater also owns the `path`.
bool PathOwnedByUser(const base::FilePath& path);

// Initializes logging for an executable.
void InitLogging(UpdaterScope updater_scope,
                 const base::FilePath::StringType& filename);

// Wraps the 'command_line' to be executed in an elevated context.
// On macOS this is done with 'sudo'.
base::CommandLine MakeElevated(base::CommandLine command_line);

// Functor used by associative containers of strings as a case-insensitive ASCII
// compare. `StringT` could be either UTF-8 or UTF-16.
struct CaseInsensitiveASCIICompare {
 public:
  template <typename StringT>
  bool operator()(const StringT& x, const StringT& y) const {
    return base::CompareCaseInsensitiveASCII(x, y) > 0;
  }
};

// Returns a new GURL by appending the given query parameter name and the
// value. Unsafe characters in the name and the value are escaped like
// %XX%XX. The original query component is preserved if it's present.
//
// Examples:
//
// AppendQueryParameter(GURL("http://example.com"), "name", "value").spec()
// => "http://example.com?name=value"
// AppendQueryParameter(GURL("http://example.com?x=y"), "name", "value").spec()
// => "http://example.com?x=y&name=value"
GURL AppendQueryParameter(const GURL& url,
                          const std::string& name,
                          const std::string& value);

#if defined(OS_MAC)
// Uses the builtin unzip utility within macOS /usr/bin/unzip to unzip instead
// of using the configurator's UnzipperFactory. The UnzipperFactory utilizes the
// //third_party/zlib/google, which has a bug that does not preserve the
// permissions when it extracts the contents. For updates via zip or
// differentials, use UnzipWithExe.
bool UnzipWithExe(const base::FilePath& src_path,
                  const base::FilePath& dest_path);

absl::optional<base::FilePath> GetKeystoneFolderPath(UpdaterScope scope);

// Read the file at path to confirm that the file at the path has the same
// permissions as the given permissions mask.
bool ConfirmFilePermissions(const base::FilePath& root_path,
                            int kPermissionsMask);
#endif  // defined(OS_MAC)

}  // namespace updater

#endif  // CHROME_UPDATER_UTIL_H_
