// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_DISK_UTIL_TYPES_H_
#define CHROME_CHROME_CLEANER_OS_DISK_UTIL_TYPES_H_

#include <string>

#include "base/strings/string16.h"

namespace chrome_cleaner {

namespace internal {

// This represents the information gathered on a file.
//
// This struct is related to the FileInformation proto message that is sent in
// reports.  They are kept separate because the data manipulated in the
// cleaner/reporter isn't necessarily the same that we want to transmit to
// Google. Another reason is that protos store strings as UTF-8, whereas the
// functions in base to manipulate user data obtained via the Windows API use
// 16-bits strings.
struct FileInformation {
  FileInformation();

  FileInformation(const base::string16& path,
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
                  bool active_file = false);

  FileInformation(const FileInformation& other);

  ~FileInformation();

  FileInformation& operator=(const FileInformation& other);

  base::string16 path;
  std::string creation_date;
  std::string last_modified_date;
  std::string sha256;
  int64_t size = 0ULL;
  // The following are internal fields of the PE header.
  base::string16 company_name;
  base::string16 company_short_name;
  base::string16 product_name;
  base::string16 product_short_name;
  base::string16 internal_name;
  base::string16 original_filename;
  base::string16 file_description;
  base::string16 file_version;
  bool active_file = false;
};

}  // namespace internal

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_DISK_UTIL_TYPES_H_
