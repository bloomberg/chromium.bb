// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_test.h"

static const char kHeapSampleBegin[] = "heap-sample-begin,";
static const char kHeapObjectLogEntryPrefix[] = "\nheap-js-cons-item,";
static const char kHeapObjectLogEntrySuffix[] = ",";

class ListenerLeakTest : public TestShellTest {
};

static std::string GetV8Log(int skip) {
  std::string v8_log;
  char buf[2048];
  int read_size;
  do {
    read_size = v8::V8::GetLogLines(skip + static_cast<int>(v8_log.size()),
                                    buf, sizeof(buf));
    v8_log.append(buf, read_size);
  } while (read_size > 0);
  return v8_log;
}

static int GetNumObjects(int log_skip, const std::string& constructor) {
  std::string v8_log = GetV8Log(log_skip);
  size_t sample_begin = v8_log.rfind(kHeapSampleBegin);
  CHECK(sample_begin != std::string::npos);
  std::string log_entry =
      kHeapObjectLogEntryPrefix + constructor + kHeapObjectLogEntrySuffix;
  size_t i = v8_log.find(log_entry, sample_begin);
  if (i == std::string::npos) return 0;
  i += log_entry.size();
  size_t j = v8_log.find(",", i);
  CHECK(j != std::string::npos);
  int num_objects;
  CHECK(base::StringToInt(v8_log.substr(i, j - i), &num_objects));
  return num_objects;
}

// This test tries to create a reference cycle between node and its listener.
// See http://crbug/17400.
TEST_F(ListenerLeakTest, ReferenceCycle) {
  const int log_skip = static_cast<int>(GetV8Log(0).size());
  FilePath listener_file;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &listener_file));
  listener_file = listener_file.Append(FILE_PATH_LITERAL("webkit"))
      .Append(FILE_PATH_LITERAL("data"))
      .Append(FILE_PATH_LITERAL("listener"))
      .Append(FILE_PATH_LITERAL("listener_leak1.html"));
  test_shell_->LoadFile(listener_file);
  test_shell_->WaitTestFinished();
  ASSERT_EQ(0, GetNumObjects(log_skip, "EventListenerLeakTestObject1"));
}

// This test sets node onclick many times to expose a possible memory
// leak where all listeners get referenced by the node.
TEST_F(ListenerLeakTest, HiddenReferences) {
  const int log_skip = static_cast<int>(GetV8Log(0).size());
  FilePath listener_file;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &listener_file));
  listener_file = listener_file.Append(FILE_PATH_LITERAL("webkit"))
      .Append(FILE_PATH_LITERAL("data"))
      .Append(FILE_PATH_LITERAL("listener"))
      .Append(FILE_PATH_LITERAL("listener_leak2.html"));
  test_shell_->LoadFile(listener_file);
  test_shell_->WaitTestFinished();
  ASSERT_EQ(1, GetNumObjects(log_skip, "EventListenerLeakTestObject2"));
}

