// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ZIP_ARCHIVER_TEST_ZIP_ARCHIVER_UTIL_H_
#define CHROME_CHROME_CLEANER_ZIP_ARCHIVER_TEST_ZIP_ARCHIVER_UTIL_H_

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/win/scoped_handle.h"

namespace chrome_cleaner {

class ZipArchiverTestFile {
 public:
  ZipArchiverTestFile();
  ~ZipArchiverTestFile();

  void Initialize();

  const base::FilePath& GetSourceFilePath() const;
  const base::FilePath& GetTempDirPath() const;
  void ExpectValidZipFile(const base::FilePath& zip_file_path,
                          const std::string& filename_in_zip,
                          const std::string& password);

 private:
  bool initialized_;
  base::ScopedTempDir temp_dir_;
  base::FilePath src_file_path_;

  DISALLOW_COPY_AND_ASSIGN(ZipArchiverTestFile);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ZIP_ARCHIVER_TEST_ZIP_ARCHIVER_UTIL_H_
