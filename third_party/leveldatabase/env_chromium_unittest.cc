// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_suite.h"
#include "env_chromium.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace leveldb_env;
using namespace leveldb;

TEST(ErrorEncoding, OnlyAMethod) {
  const MethodID in_method = kSequentialFileRead;
  const Status s = MakeIOError("Somefile.txt", "message", in_method);
  MethodID method;
  int error = -75;
  EXPECT_EQ(METHOD_ONLY,
            ParseMethodAndError(s.ToString().c_str(), &method, &error));
  EXPECT_EQ(in_method, method);
  EXPECT_EQ(-75, error);
}

TEST(ErrorEncoding, PlatformFileError) {
  const MethodID in_method = kWritableFileClose;
  const base::PlatformFileError pfe =
      base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  const Status s = MakeIOError("Somefile.txt", "message", in_method, pfe);
  MethodID method;
  int error;
  EXPECT_EQ(METHOD_AND_PFE,
            ParseMethodAndError(s.ToString().c_str(), &method, &error));
  EXPECT_EQ(in_method, method);
  EXPECT_EQ(pfe, error);
}

TEST(ErrorEncoding, Errno) {
  const MethodID in_method = kWritableFileFlush;
  const int some_errno = ENOENT;
  const Status s =
      MakeIOError("Somefile.txt", "message", in_method, some_errno);
  MethodID method;
  int error;
  EXPECT_EQ(METHOD_AND_ERRNO,
            ParseMethodAndError(s.ToString().c_str(), &method, &error));
  EXPECT_EQ(in_method, method);
  EXPECT_EQ(some_errno, error);
}

TEST(ErrorEncoding, NoEncodedMessage) {
  Status s = Status::IOError("Some message", "from leveldb itself");
  MethodID method = kRandomAccessFileRead;
  int error = 4;
  EXPECT_EQ(NONE, ParseMethodAndError(s.ToString().c_str(), &method, &error));
  EXPECT_EQ(kRandomAccessFileRead, method);
  EXPECT_EQ(4, error);
}

class MyEnv : public ChromiumEnv {
 public:
  MyEnv() : directory_syncs_(0) {}
  int directory_syncs() { return directory_syncs_; }

 protected:
  virtual void DidSyncDir(const std::string& fname) {
    ++directory_syncs_;
    ChromiumEnv::DidSyncDir(fname);
  }

 private:
  int directory_syncs_;
};

TEST(ChromiumEnv, DirectorySyncing) {
  MyEnv env;
  base::ScopedTempDir dir;
  dir.CreateUniqueTempDir();
  base::FilePath dir_path = dir.path();
  std::string some_data = "some data";
  Slice data = some_data;

  std::string manifest_file_name =
      FilePathToString(dir_path.Append(FILE_PATH_LITERAL("MANIFEST-001")));
  WritableFile* manifest_file;
  Status s = env.NewWritableFile(manifest_file_name, &manifest_file);
  EXPECT_TRUE(s.ok());
  manifest_file->Append(data);
  EXPECT_EQ(0, env.directory_syncs());
  manifest_file->Append(data);
  EXPECT_EQ(0, env.directory_syncs());

  std::string sst_file_name =
      FilePathToString(dir_path.Append(FILE_PATH_LITERAL("000003.sst")));
  WritableFile* sst_file;
  s = env.NewWritableFile(sst_file_name, &sst_file);
  EXPECT_TRUE(s.ok());
  sst_file->Append(data);
  EXPECT_EQ(0, env.directory_syncs());

  manifest_file->Append(data);
  EXPECT_EQ(1, env.directory_syncs());
  manifest_file->Append(data);
  EXPECT_EQ(1, env.directory_syncs());
}

int main(int argc, char** argv) { return base::TestSuite(argc, argv).Run(); }
