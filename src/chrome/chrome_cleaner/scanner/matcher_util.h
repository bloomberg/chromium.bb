// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_SCANNER_MATCHER_UTIL_H_
#define CHROME_CHROME_CLEANER_SCANNER_MATCHER_UTIL_H_

#include <set>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "chrome/chrome_cleaner/settings/matching_options.h"

namespace base {
class FilePath;
}  // namespace base

namespace chrome_cleaner {

class SignatureMatcherAPI;

extern const size_t kKiloByte;
extern const size_t kMegaByte;
extern const size_t kMaxFilesInFolderToLog;  // Exposed for unittests.

// Return whether the file |path| has a smaller size than |size|.
bool IsFileSizeLessThan(const base::FilePath& path, size_t size);

// Find a single matching file for |pattern| located in |root_path|. Wild-cards
// are allowed in |pattern| only. |include_folders| flag controls whether or not
// the folders should be taken into account. |match| is set to the matching file
// path. If none or more than one file are found, the function returns false.
bool MatchSingleFileWithPattern(const base::FilePath& root_path,
                                const base::string16& pattern,
                                bool include_folders,
                                base::FilePath* match);

void DeleteSoftwareRegistryKeys(const base::string16& software_key_path,
                                PUPData::PUP* pup);

void CollectSinglePath(const base::FilePath& path, PUPData::PUP* pup);

void CollectPathRecursively(const base::FilePath& path, PUPData::PUP* pup);

void CollectDiskFootprintRecursively(int csidl,
                                     const base::char16* path,
                                     PUPData::PUP* pup);

bool IsKnownFileByDigest(const base::FilePath& path,
                         const SignatureMatcherAPI* signature_matcher,
                         const char* const digests[],
                         size_t digests_length);

// A pair of filesize and digest. The filesize is used to avoid computing the
// digest of a file.
struct FileDigestInfo {
  const char* const digest;
  size_t filesize;
};

// Check whether the checksum (sha256) of a given file is part of a sorted
// array of |FileDigestInfo|.
bool IsKnownFileByDigestInfo(const base::FilePath& path,
                             const SignatureMatcherAPI* signature_matcher,
                             const FileDigestInfo* digests,
                             size_t digests_length);

bool IsKnownFileByOriginalFilename(const base::FilePath& path,
                                   const SignatureMatcherAPI* signature_matcher,
                                   const base::char16* const names[],
                                   size_t names_length);

bool IsKnownFileByCompanyName(const base::FilePath& path,
                              const SignatureMatcherAPI* signature_matcher,
                              const base::char16* const names[],
                              size_t names_length);

// Log the found digest of |file_path| described by |prefix|.
void LogFoundDigest(const base::FilePath file_path,
                    const char* prefix,
                    PUPData::PUP* pup);

// This is equivalent to the above LogFoundDigest(file_path, prefix, nullptr).
void LogFoundDigest(const base::FilePath file_path, const char* prefix);

// This is equivalent to the above LogFolderContent(folder_path, nullptr).
void LogFolderContent(const base::FilePath& folder_path);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_SCANNER_MATCHER_UTIL_H_
