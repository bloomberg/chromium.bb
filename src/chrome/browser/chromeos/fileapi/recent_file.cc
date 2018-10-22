// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/recent_file.h"

namespace chromeos {

RecentFile::RecentFile() = default;

RecentFile::RecentFile(const storage::FileSystemURL& url,
                       const base::Time& last_modified)
    : url_(url), last_modified_(last_modified) {}

RecentFile::RecentFile(const RecentFile& other) = default;

RecentFile::~RecentFile() = default;

RecentFile& RecentFile::operator=(const RecentFile& other) = default;

bool RecentFileComparator::operator()(const RecentFile& a,
                                      const RecentFile& b) {
  if (a.last_modified() != b.last_modified())
    return a.last_modified() > b.last_modified();
  return storage::FileSystemURL::Comparator()(a.url(), b.url());
}

}  // namespace chromeos
