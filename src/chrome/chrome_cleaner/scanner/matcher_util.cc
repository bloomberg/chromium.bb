// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/scanner/matcher_util.h"

#include <stdint.h>

#include <cctype>
#include <memory>
#include <set>
#include <vector>

#include "base/command_line.h"
#include "base/file_version_info.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/registry_util.h"
#include "chrome/chrome_cleaner/os/task_scheduler.h"
#include "chrome/chrome_cleaner/pup_data/pup_disk_util.h"
#include "chrome/chrome_cleaner/scanner/signature_matcher_api.h"
#include "chrome/chrome_cleaner/strings/string_util.h"

namespace chrome_cleaner {

namespace {
// The maximum file size for computing the digest of listed files.
const size_t kMaxFileSizeToLog = 2 * kMegaByte;

void LogFile(const base::FilePath& file_path) {
  if (IsFileSizeLessThan(file_path, kMaxFileSizeToLog)) {
    std::string digest;
    if (ComputeSHA256DigestOfPath(file_path, &digest)) {
      LOG(INFO) << " -- '" << SanitizePath(file_path)
                << "' digest = " << digest;
    } else {
      LOG(ERROR) << "Failed to get digest for : '" << SanitizePath(file_path)
                 << "'.";
    }
  } else {
    LOG(INFO) << " -- '" << SanitizePath(file_path) << "'";
  }
}

}  // namespace

const size_t kKiloByte = 1024;         // File size in bytes.
const size_t kMegaByte = 1024 * 1024;  // File size in bytes.
// The maximum number of files to log when listing the content of a folder.
const size_t kMaxFilesInFolderToLog = 10;

bool IsFileSizeLessThan(const base::FilePath& path, size_t size) {
  int64_t file_size = 0;
  return (base::GetFileSize(path, &file_size) &&
          static_cast<size_t>(file_size) < size);
}

bool MatchSingleFileWithPattern(const base::FilePath& root_path,
                                const base::string16& pattern,
                                bool include_folders,
                                base::FilePath* match) {
  DCHECK(match);

  base::FilePath match_found;
  bool found_one = false;

  int file_type = base::FileEnumerator::FILES;
  if (include_folders)
    file_type |= base::FileEnumerator::DIRECTORIES;

  if (NameContainsWildcards(pattern)) {
    base::FileEnumerator file_enum(root_path, false, file_type, pattern);
    for (base::FilePath file = file_enum.Next(); !file.empty();
         file = file_enum.Next()) {
      if (NameMatchesPattern(file.BaseName().value(), pattern, 0)) {
        if (found_one) {
          return false;
        } else {
          match_found = file;
          found_one = true;
        }
      }
    }
  } else {
    match_found = root_path.Append(pattern);
    if (base::PathExists(match_found))
      found_one = true;
  }

  if (found_one)
    *match = match_found;
  return found_one;
}

void DeleteSoftwareRegistryKeys(const base::string16& software_key_path,
                                PUPData::PUP* pup) {
  DCHECK(pup);
  PUPData::DeleteRegistryKeyIfPresent(
      RegKeyPath(HKEY_CURRENT_USER, software_key_path.c_str(), KEY_WOW64_32KEY),
      pup);
  PUPData::DeleteRegistryKeyIfPresent(
      RegKeyPath(HKEY_LOCAL_MACHINE, software_key_path.c_str(),
                 KEY_WOW64_32KEY),
      pup);
  PUPData::DeleteRegistryKeyIfPresent(
      RegKeyPath(HKEY_CURRENT_USER, software_key_path.c_str(), KEY_WOW64_64KEY),
      pup);
  PUPData::DeleteRegistryKeyIfPresent(
      RegKeyPath(HKEY_LOCAL_MACHINE, software_key_path.c_str(),
                 KEY_WOW64_64KEY),
      pup);
}

void CollectPathRecursively(const base::FilePath& path, PUPData::PUP* pup) {
  DCHECK(pup);
  if (base::PathExists(path))
    CollectPathsRecursively(path, pup);
}

void CollectSinglePath(const base::FilePath& path, PUPData::PUP* pup) {
  DCHECK(pup);
  if (base::PathExists(path) && !base::DirectoryExists(path))
    pup->AddDiskFootprint(path);
}

void CollectDiskFootprintRecursively(int csidl,
                                     const base::char16* path,
                                     PUPData::PUP* pup) {
  DCHECK_NE(csidl, PUPData::kInvalidCsidl);
  DCHECK(path);

  base::FilePath expanded_path(
      ExpandSpecialFolderPath(csidl, base::FilePath(path)));
  if (expanded_path.empty())
    return;
  CollectPathRecursively(expanded_path, pup);
}

bool IsKnownFileByDigest(const base::FilePath& path,
                         const SignatureMatcherAPI* signature_matcher,
                         const char* const digests[],
                         size_t digests_length) {
  DCHECK(signature_matcher);
  if (base::DirectoryExists(path) || !base::PathExists(path))
    return false;

  std::string path_digest;
  if (!signature_matcher->ComputeSHA256DigestOfPath(path, &path_digest)) {
    PLOG(ERROR) << "Can't compute file digest: '" << SanitizePath(path) << "'.";
    return false;
  }

  for (size_t index = 0; index < digests_length; ++index) {
    const char* expected_digest = digests[index];
    DCHECK(expected_digest);
    DCHECK_EQ(expected_digest, base::ToUpperASCII(expected_digest));
    DCHECK_EQ(64UL, strlen(expected_digest));
    if (path_digest.compare(expected_digest) == 0)
      return true;
  }

  return false;
}

bool IsKnownFileByDigestInfo(const base::FilePath& fullpath,
                             const SignatureMatcherAPI* signature_matcher,
                             const FileDigestInfo* digests,
                             size_t digests_length) {
  DCHECK(signature_matcher);
  DCHECK(digests);

  if (base::DirectoryExists(fullpath) || !base::PathExists(fullpath))
    return false;

  size_t filesize = 0;
  std::string digest;
  for (size_t index = 0; index < digests_length; ++index) {
    if (signature_matcher->MatchFileDigestInfo(fullpath, &filesize, &digest,
                                               digests[index])) {
      return true;
    }
  }
  return false;
}

bool IsKnownFileByOriginalFilename(const base::FilePath& path,
                                   const SignatureMatcherAPI* signature_matcher,
                                   const base::char16* const names[],
                                   size_t names_length) {
  DCHECK(signature_matcher);
  DCHECK(names);
  VersionInformation version_information;
  if (base::DirectoryExists(path) || !base::PathExists(path) ||
      !signature_matcher->RetrieveVersionInformation(path,
                                                     &version_information)) {
    return false;
  }

  for (size_t i = 0; i < names_length; ++i) {
    if (String16EqualsCaseInsensitive(version_information.original_filename,
                                      names[i])) {
      return true;
    }
  }
  return false;
}

bool IsKnownFileByCompanyName(const base::FilePath& path,
                              const SignatureMatcherAPI* signature_matcher,
                              const base::char16* const names[],
                              size_t names_length) {
  DCHECK(signature_matcher);
  DCHECK(names);
  VersionInformation version_information;
  if (base::DirectoryExists(path) || !base::PathExists(path) ||
      !signature_matcher->RetrieveVersionInformation(path,
                                                     &version_information)) {
    return false;
  }

  for (size_t i = 0; i < names_length; ++i) {
    if (String16EqualsCaseInsensitive(version_information.company_name,
                                      names[i])) {
      return true;
    }
  }
  return false;
}

void LogFoundDigest(const base::FilePath file_path,
                    const char* prefix,
                    PUPData::PUP* pup) {
  DCHECK(prefix);
  std::string digest;
  int64_t size = -1;
  if (!base::GetFileSize(file_path, &size))
    PLOG(ERROR) << "Failed to get size for: '" << SanitizePath(file_path)
                << "'.";

  if (ComputeSHA256DigestOfPath(file_path, &digest)) {
    LOG(INFO) << "Found digest for " << prefix << ", path: '"
              << SanitizePath(file_path) << "', digest: '" << digest
              << "', size: '" << size << "'.";
  } else {
    LOG(INFO) << "Failed to get digest for " << prefix << ", path: '"
              << SanitizePath(file_path) << "', size: '" << size << "'.";
  }
}

void LogFoundDigest(const base::FilePath file_path, const char* prefix) {
  LogFoundDigest(file_path, prefix, nullptr);
}

void LogFolderContent(const base::FilePath& folder_path) {
  base::FileEnumerator file_enum(folder_path, true,
                                 base::FileEnumerator::FILES);
  size_t nb_files = 0;
  for (base::FilePath file_path = file_enum.Next(); !file_path.empty();
       file_path = file_enum.Next()) {
    LogFile(file_path);
    if (++nb_files > kMaxFilesInFolderToLog) {
      LOG(INFO) << "The folder contains too many files.";
      return;
    }
  }
}

}  // namespace chrome_cleaner
