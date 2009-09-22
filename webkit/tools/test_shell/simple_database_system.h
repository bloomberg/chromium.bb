// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_SIMPLE_DATABASE_SYSTEM_H_
#define WEBKIT_TOOLS_TEST_SHELL_SIMPLE_DATABASE_SYSTEM_H_

#include "base/file_path.h"
#include "base/platform_file.h"
#include "base/scoped_temp_dir.h"

class SimpleDatabaseSystem {
 public:
  static SimpleDatabaseSystem* GetInstance();
  SimpleDatabaseSystem();
  ~SimpleDatabaseSystem();

  base::PlatformFile OpenFile(
      const FilePath& file_name, int desired_flags,
      base::PlatformFile* dir_handle);
  int DeleteFile(const FilePath& file_name, bool sync_dir);
  long GetFileAttributes(const FilePath& file_name);
  long long GetFileSize(const FilePath& file_name);
  void ClearAllDatabases();

 private:
  FilePath GetDBDir();
  FilePath GetDBFileFullPath(const FilePath& file_name);

  static SimpleDatabaseSystem* instance_;

  ScopedTempDir temp_dir_;

  // HACK: see OpenFile's implementation
  base::PlatformFile hack_main_db_handle_;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_SIMPLE_DATABASE_SYSTEM_H_
