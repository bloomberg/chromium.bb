// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_path_manager.h"

#include "base/file_util.h"
#include "base/file_util_proxy.h"
#include "base/rand_util.h"
#include "base/logging.h"
#include "base/scoped_callback_factory.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileSystem.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSecurityOrigin.h"
#include "webkit/glue/webkit_glue.h"

// We use some of WebKit types for conversions between storage identifiers
// and origin URLs.
using WebKit::WebFileSystem;
using WebKit::WebSecurityOrigin;
using WebKit::WebString;

using base::FileUtilProxy;
using base::PlatformFileError;

namespace fileapi {

const FilePath::CharType FileSystemPathManager::kFileSystemDirectory[] =
    FILE_PATH_LITERAL("FileSystem");

const char FileSystemPathManager::kPersistentName[] = "Persistent";
const char FileSystemPathManager::kTemporaryName[] = "Temporary";

namespace {

// Restricted names.
// http://dev.w3.org/2009/dap/file-system/file-dir-sys.html#naming-restrictions
static const char* const kRestrictedNames[] = {
  "con", "prn", "aux", "nul",
  "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
  "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9",
};

// Restricted chars.
static const FilePath::CharType kRestrictedChars[] = {
  '/', '\\', '<', '>', ':', '?', '*', '"', '|',
};

inline std::string FilePathStringToASCII(
    const FilePath::StringType& path_string) {
#if defined(OS_WIN)
  return WideToASCII(path_string);
#elif defined(OS_POSIX)
  return path_string;
#endif
}

}  // anonymous namespace

FileSystemPathManager::FileSystemPathManager(
    const FilePath& data_path,
    bool is_incognito,
    bool allow_file_access_from_files)
    : base_path_(data_path.Append(kFileSystemDirectory)),
      is_incognito_(is_incognito),
      allow_file_access_from_files_(allow_file_access_from_files) {
}

bool FileSystemPathManager::GetFileSystemRootPath(
    const GURL& origin_url, fileapi::FileSystemType type,
    FilePath* root_path, std::string* name) const {
  // TODO(kinuko): should return an isolated temporary file system space.
  if (is_incognito_)
    return false;

  if (!IsAllowedScheme(origin_url))
    return false;

  std::string storage_identifier = GetStorageIdentifierFromURL(origin_url);
  switch (type) {
    case fileapi::kFileSystemTypeTemporary:
      if (root_path)
        *root_path = base_path_.AppendASCII(storage_identifier)
                               .AppendASCII(kTemporaryName);
      if (name)
        *name = storage_identifier + ":" + kTemporaryName;
      return true;
    case fileapi::kFileSystemTypePersistent:
      if (root_path)
        *root_path = base_path_.AppendASCII(storage_identifier)
                               .AppendASCII(kPersistentName);
      if (name)
        *name = storage_identifier + ":" + kPersistentName;
      return true;
  }
  LOG(WARNING) << "Unknown filesystem type is requested:" << type;
  return false;
}

bool FileSystemPathManager::CheckValidFileSystemPath(
    const FilePath& path) const {
  // Any paths that includes parent references are considered invalid.
  if (path.ReferencesParent())
    return false;

  // The path should be a child of the profile FileSystem path.
  FilePath relative;
  if (!base_path_.AppendRelativePath(path, &relative))
    return false;

  // The relative path from the profile FileSystem path should at least
  // contains two components, one for storage identifier and the other for type

  std::vector<FilePath::StringType> components;
  relative.GetComponents(&components);
  if (components.size() < 2)
    return false;

  // The second component of the relative path to the root directory
  // must be kPersistent or kTemporary.
  if (!IsStringASCII(components[1]))
    return false;

  std::string ascii_type_component = FilePathStringToASCII(components[1]);
  if (ascii_type_component != kPersistentName &&
      ascii_type_component != kTemporaryName)
    return false;

  return true;
}

bool FileSystemPathManager::GetOriginFromPath(
    const FilePath& path, GURL* origin_url) {
  DCHECK(origin_url);
  FilePath relative;
  if (!base_path_.AppendRelativePath(path, &relative)) {
    // The path should be a child of the profile's FileSystem path.
    return false;
  }
  std::vector<FilePath::StringType> components;
  relative.GetComponents(&components);
  if (components.size() < 2) {
    // The relative path should at least contain storage identifier and type.
    return false;
  }
  WebSecurityOrigin web_security_origin =
      WebSecurityOrigin::createFromDatabaseIdentifier(
          webkit_glue::FilePathStringToWebString(components[0]));
  *origin_url = GURL(web_security_origin.toString());

  // We need this work-around for file:/// URIs as
  // createFromDatabaseIdentifier returns empty origin_url for them.
  if (allow_file_access_from_files_ && origin_url->spec().empty() &&
      components[0].find(FILE_PATH_LITERAL("file")) == 0) {
    *origin_url = GURL("file:///");
    return true;
  }

  return IsAllowedScheme(*origin_url);
}

bool FileSystemPathManager::IsAllowedScheme(const GURL& url) const {
  // Basically we only accept http or https. We allow file:// URLs
  // only if --allow-file-access-from-files flag is given.
  return url.SchemeIs("http") || url.SchemeIs("https") ||
         (url.SchemeIsFile() && allow_file_access_from_files_);
}

bool FileSystemPathManager::IsRestrictedFileName(
    const FilePath& filename) const {
  if (filename.value().size() == 0)
    return false;

  if (IsWhitespace(filename.value()[filename.value().size() - 1]) ||
      filename.value()[filename.value().size() - 1] == '.')
    return true;

  std::string filename_lower = StringToLowerASCII(
      FilePathStringToASCII(filename.value()));

  for (size_t i = 0; i < arraysize(kRestrictedNames); ++i) {
    // Exact match.
    if (filename_lower == kRestrictedNames[i])
      return true;
    // Starts with "RESTRICTED_NAME.".
    if (filename_lower.find(std::string(kRestrictedNames[i]) + ".") == 0)
      return true;
  }

  for (size_t i = 0; i < arraysize(kRestrictedChars); ++i) {
    if (filename.value().find(kRestrictedChars[i]) !=
        FilePath::StringType::npos)
      return true;
  }

  return false;
}

std::string FileSystemPathManager::GetStorageIdentifierFromURL(
    const GURL& url) {
  WebKit::WebSecurityOrigin web_security_origin =
      WebKit::WebSecurityOrigin::createFromString(UTF8ToUTF16(url.spec()));
  return web_security_origin.databaseIdentifier().utf8();
}

}  // namespace fileapi

COMPILE_ASSERT(int(WebFileSystem::TypeTemporary) == \
               int(fileapi::kFileSystemTypeTemporary), mismatching_enums);
COMPILE_ASSERT(int(WebFileSystem::TypePersistent) == \
               int(fileapi::kFileSystemTypePersistent), mismatching_enums);
