// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_LOGGING_SCOPED_LOGGING_H_
#define CHROME_CHROME_CLEANER_LOGGING_SCOPED_LOGGING_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string16.h"

namespace chrome_cleaner {

// A utility to print the "Null" string for null wchar_t*.
base::string16 ConvertIfNull(const wchar_t* str);

// Un/Initialize the logging machinery. A |suffix| can be appended to the log
// file name if necessary.
class ScopedLogging {
 public:
  explicit ScopedLogging(base::FilePath::StringPieceType suffix);
  ~ScopedLogging();

  // Returns the path to the log file for the current process.
  static base::FilePath GetLogFilePath(base::FilePath::StringPieceType suffix);

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedLogging);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_LOGGING_SCOPED_LOGGING_H_
