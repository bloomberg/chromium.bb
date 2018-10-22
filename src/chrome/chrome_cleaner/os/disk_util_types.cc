// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/disk_util_types.h"

namespace chrome_cleaner {

namespace internal {

FileInformation::FileInformation() = default;

FileInformation::FileInformation(const base::string16& path,
                                 const std::string& creation_date,
                                 const std::string& last_modified_date,
                                 const std::string& sha256,
                                 int64_t size,
                                 const base::string16& company_name,
                                 const base::string16& company_short_name,
                                 const base::string16& product_name,
                                 const base::string16& product_short_name,
                                 const base::string16& internal_name,
                                 const base::string16& original_filename,
                                 const base::string16& file_description,
                                 const base::string16& file_version,
                                 bool active_file)
    : path(path),
      creation_date(creation_date),
      last_modified_date(last_modified_date),
      sha256(sha256),
      size(size),
      company_name(company_name),
      company_short_name(company_short_name),
      product_name(product_name),
      product_short_name(product_short_name),
      internal_name(internal_name),
      original_filename(original_filename),
      file_description(file_description),
      file_version(file_version),
      active_file(active_file) {}

FileInformation::FileInformation(const FileInformation& other) = default;

FileInformation::~FileInformation() = default;

FileInformation& FileInformation::operator=(const FileInformation& other) =
    default;

}  // namespace internal

}  // namespace chrome_cleaner
