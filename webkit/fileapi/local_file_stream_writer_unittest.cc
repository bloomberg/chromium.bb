// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/local_file_stream_writer.h"

#include <string>

#include "base/callback.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using fileapi::LocalFileStreamWriter;

class LocalFileStreamWriterTest : public testing::Test {
 public:
  LocalFileStreamWriterTest() : message_loop_(MessageLoop::TYPE_IO) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

 protected:
  base::FilePath Path(const std::string& name) {
    return temp_dir_.path().AppendASCII(name);
  }

  int WriteStringToWriter(LocalFileStreamWriter* writer,
                          const std::string& data) {
    scoped_refptr<net::StringIOBuffer> buffer(new net::StringIOBuffer(data));
    scoped_refptr<net::DrainableIOBuffer> drainable(
        new net::DrainableIOBuffer(buffer, buffer->size()));

    while (drainable->BytesRemaining() > 0) {
      net::TestCompletionCallback callback;
      int result = writer->Write(drainable, drainable->BytesRemaining(),
                                 callback.callback());
      if (result == net::ERR_IO_PENDING)
        result = callback.WaitForResult();
      if (result <= 0)
        return result;
      drainable->DidConsume(result);
    }
    return net::OK;
  }

  std::string GetFileContent(const base::FilePath& path) {
    std::string content;
    file_util::ReadFileToString(path, &content);
    return content;
  }

  base::FilePath CreateFileWithContent(const std::string& name,
                                 const std::string& data) {
    base::FilePath path = Path(name);
    file_util::WriteFile(path, data.c_str(), data.size());
    return path;
  }

 private:
  MessageLoop message_loop_;
  base::ScopedTempDir temp_dir_;
};

void NeverCalled(int unused) {
  ADD_FAILURE();
}

}  // namespace

TEST_F(LocalFileStreamWriterTest, Write) {
  base::FilePath path = CreateFileWithContent("file_a", "");
  scoped_ptr<LocalFileStreamWriter> writer(new LocalFileStreamWriter(path, 0));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "foo"));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "bar"));
  writer.reset();
  MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(file_util::PathExists(path));
  EXPECT_EQ("foobar", GetFileContent(path));
}

TEST_F(LocalFileStreamWriterTest, WriteMiddle) {
  base::FilePath path = CreateFileWithContent("file_a", "foobar");
  scoped_ptr<LocalFileStreamWriter> writer(new LocalFileStreamWriter(path, 2));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "xxx"));
  writer.reset();
  MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(file_util::PathExists(path));
  EXPECT_EQ("foxxxr", GetFileContent(path));
}

TEST_F(LocalFileStreamWriterTest, WriteEnd) {
  base::FilePath path = CreateFileWithContent("file_a", "foobar");
  scoped_ptr<LocalFileStreamWriter> writer(new LocalFileStreamWriter(path, 6));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "xxx"));
  writer.reset();
  MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(file_util::PathExists(path));
  EXPECT_EQ("foobarxxx", GetFileContent(path));
}

TEST_F(LocalFileStreamWriterTest, WriteFailForNonexistingFile) {
  base::FilePath path = Path("file_a");
  ASSERT_FALSE(file_util::PathExists(path));
  scoped_ptr<LocalFileStreamWriter> writer(new LocalFileStreamWriter(path, 0));
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, WriteStringToWriter(writer.get(), "foo"));
  writer.reset();
  MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(file_util::PathExists(path));
}

TEST_F(LocalFileStreamWriterTest, CancelBeforeOperation) {
  base::FilePath path = Path("file_a");
  scoped_ptr<LocalFileStreamWriter> writer(new LocalFileStreamWriter(path, 0));
  // Cancel immediately fails when there's no in-flight operation.
  int cancel_result = writer->Cancel(base::Bind(&NeverCalled));
  EXPECT_EQ(net::ERR_UNEXPECTED, cancel_result);
}

TEST_F(LocalFileStreamWriterTest, CancelAfterFinishedOperation) {
  base::FilePath path = CreateFileWithContent("file_a", "");
  scoped_ptr<LocalFileStreamWriter> writer(new LocalFileStreamWriter(path, 0));
  EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "foo"));

  // Cancel immediately fails when there's no in-flight operation.
  int cancel_result = writer->Cancel(base::Bind(&NeverCalled));
  EXPECT_EQ(net::ERR_UNEXPECTED, cancel_result);

  writer.reset();
  MessageLoop::current()->RunUntilIdle();
  // Write operation is already completed.
  EXPECT_TRUE(file_util::PathExists(path));
  EXPECT_EQ("foo", GetFileContent(path));
}

TEST_F(LocalFileStreamWriterTest, CancelWrite) {
  base::FilePath path = CreateFileWithContent("file_a", "foobar");
  scoped_ptr<LocalFileStreamWriter> writer(new LocalFileStreamWriter(path, 0));

  scoped_refptr<net::StringIOBuffer> buffer(new net::StringIOBuffer("xxx"));
  int result = writer->Write(buffer, buffer->size(), base::Bind(&NeverCalled));
  ASSERT_EQ(net::ERR_IO_PENDING, result);

  net::TestCompletionCallback callback;
  writer->Cancel(callback.callback());
  int cancel_result = callback.WaitForResult();
  EXPECT_EQ(net::OK, cancel_result);
}
