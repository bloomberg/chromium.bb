// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_suite.h"
#include "env_chromium.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace leveldb_env;
using namespace leveldb;

TEST(ErrorEncoding, OnlyAMethod) {
  const MethodID in_method = kSequentialFileRead;
  const Status s = MakeIOError("Somefile.txt", "message", in_method);
  int method = -50;
  int error = -75;
  EXPECT_TRUE(ParseMethodAndError(s.ToString().c_str(), &method, &error));
  EXPECT_EQ(in_method, method);
  EXPECT_EQ(-75, error);
}

TEST(ErrorEncoding, PlatformFileError) {
  const MethodID in_method = kWritableFileClose;
  const base::PlatformFileError pfe =
      base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  const Status s = MakeIOError("Somefile.txt", "message", in_method, pfe);
  int method;
  int error;
  EXPECT_TRUE(ParseMethodAndError(s.ToString().c_str(), &method, &error));
  EXPECT_EQ(in_method, method);
  EXPECT_EQ(pfe, error);
}

TEST(ErrorEncoding, Errno) {
  const MethodID in_method = kWritableFileFlush;
  const int some_errno = ENOENT;
  const Status s =
      MakeIOError("Somefile.txt", "message", in_method, some_errno);
  int method;
  int error;
  EXPECT_TRUE(ParseMethodAndError(s.ToString().c_str(), &method, &error));
  EXPECT_EQ(in_method, method);
  EXPECT_EQ(some_errno, error);
}

TEST(ErrorEncoding, NoEncodedMessage) {
  Status s = Status::IOError("Some message", "from leveldb itself");
  int method = 3;
  int error = 4;
  EXPECT_FALSE(ParseMethodAndError(s.ToString().c_str(), &method, &error));
  EXPECT_EQ(3, method);
  EXPECT_EQ(4, error);
}

int main(int argc, char** argv) { return base::TestSuite(argc, argv).Run(); }
