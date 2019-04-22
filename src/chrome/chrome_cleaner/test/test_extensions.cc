// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/test_extensions.h"

#include "base/files/file.h"
#include "base/files/file_util.h"

namespace chrome_cleaner {

TestRegistryEntry::TestRegistryEntry(HKEY hkey,
                                     const base::string16& path,
                                     const base::string16& name,
                                     const base::string16& value)
    : hkey(hkey), path(path), name(name), value(value) {}
TestRegistryEntry::TestRegistryEntry(const TestRegistryEntry& other) = default;
TestRegistryEntry& TestRegistryEntry::operator=(
    const TestRegistryEntry& other) = default;

bool CreateProfileWithExtensionAndFiles(
    const base::FilePath& profile_path,
    const base::string16& extension_id,
    const std::vector<base::string16>& extension_files) {
  if (!base::CreateDirectory(profile_path))
    return false;

  base::FilePath extensions_folder_path = profile_path.Append(L"Extensions");
  if (!base::CreateDirectory(extensions_folder_path))
    return false;

  base::FilePath extension_path = extensions_folder_path.Append(extension_id);
  if (!base::CreateDirectory(extension_path))
    return false;

  for (const base::string16& file_name : extension_files) {
    base::File extension_file(
        extension_path.Append(file_name),
        base::File::Flags::FLAG_CREATE | base::File::Flags::FLAG_READ);

    if (!extension_file.IsValid())
      return false;
  }

  return true;
}

}  // namespace chrome_cleaner
