// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <limits>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "webkit/plugins/ppapi/mock_plugin_delegate.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppapi_unittest.h"
#include "webkit/plugins/ppapi/quota_file_io.h"

using base::MessageLoopProxy;
using base::PlatformFile;
using base::PlatformFileError;

namespace webkit {
namespace ppapi {

namespace {
class QuotaMockPluginDelegate : public MockPluginDelegate {
 public:
  typedef PluginDelegate::AvailableSpaceCallback Callback;

  QuotaMockPluginDelegate()
      : available_space_(0),
        will_update_count_(0),
        file_thread_(MessageLoopProxy::current()),
        ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  }
  virtual ~QuotaMockPluginDelegate() {}

  virtual scoped_refptr<MessageLoopProxy> GetFileThreadMessageLoopProxy() {
    return file_thread_;
  }

  virtual void QueryAvailableSpace(
      const GURL& origin,
      quota::StorageType type,
      const Callback& callback) OVERRIDE {
    DCHECK_EQ(false, callback.is_null());
    MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(
            &QuotaMockPluginDelegate::RunAvailableSpaceCallback,
            weak_factory_.GetWeakPtr(), callback));
  }

  virtual void WillUpdateFile(const GURL& file_path) OVERRIDE {
    file_path_ = file_path;
    ++will_update_count_;
  }

  virtual void DidUpdateFile(const GURL& file_path, int64_t delta) OVERRIDE {
    ASSERT_EQ(file_path_, file_path);
    ASSERT_GT(will_update_count_, 0);
    --will_update_count_;
    available_space_ -= delta;
  }

  void set_available_space(int64 available) { available_space_ = available; }
  int64_t available_space() const { return available_space_; }

 private:
  void RunAvailableSpaceCallback(const Callback& callback) {
    callback.Run(available_space_);
  }

  int64_t available_space_;
  int will_update_count_;
  GURL file_path_;
  scoped_refptr<MessageLoopProxy> file_thread_;
  base::WeakPtrFactory<QuotaMockPluginDelegate> weak_factory_;
};
}  // namespace

class QuotaFileIOTest : public PpapiUnittest {
 public:
  QuotaFileIOTest()
      : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {}

  virtual void SetUp() OVERRIDE {
    PpapiUnittest::SetUp();
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    base::FilePath path;
    ASSERT_TRUE(file_util::CreateTemporaryFileInDir(dir_.path(), &path));
    int file_flags = base::PLATFORM_FILE_OPEN |
                     base::PLATFORM_FILE_READ |
                     base::PLATFORM_FILE_WRITE |
                     base::PLATFORM_FILE_WRITE_ATTRIBUTES;
    bool created = false;
    file_ = base::kInvalidPlatformFileValue;
    PlatformFileError error = base::PLATFORM_FILE_OK;
    file_ = base::CreatePlatformFile(path, file_flags, &created, &error);
    ASSERT_EQ(base::PLATFORM_FILE_OK, error);
    ASSERT_NE(base::kInvalidPlatformFileValue, file_);
    ASSERT_FALSE(created);
    quota_file_io_.reset(new QuotaFileIO(
        instance()->pp_instance(), file_, GURL(),
        PP_FILESYSTEMTYPE_LOCALTEMPORARY));
  }

  virtual void TearDown() OVERRIDE {
    quota_file_io_.reset();
    if (file_ != base::kInvalidPlatformFileValue)
      base::ClosePlatformFile(file_);
    PpapiUnittest::TearDown();
  }

 protected:
  virtual MockPluginDelegate* NewPluginDelegate() OVERRIDE {
    return static_cast<MockPluginDelegate*>(new QuotaMockPluginDelegate);
  }

  void WriteTestBody(bool will_operation) {
    // Attempt to write zero bytes.
    EXPECT_FALSE(quota_file_io_->Write(
        0, "data", 0,
        base::Bind(&QuotaFileIOTest::DidWrite, weak_factory_.GetWeakPtr())));
    // Attempt to write negative number of bytes.
    EXPECT_FALSE(quota_file_io_->Write(
        0, "data", std::numeric_limits<int32_t>::min(),
        base::Bind(&QuotaFileIOTest::DidWrite, weak_factory_.GetWeakPtr())));

    quota_plugin_delegate()->set_available_space(100);
    std::string read_buffer;

    // Write 8 bytes at offset 0 (-> length=8).
    std::string data("12345678");
    Write(0, data, will_operation);
    MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(1U, num_results());
    EXPECT_EQ(static_cast<int>(data.size()), bytes_written().front());
    EXPECT_EQ(base::PLATFORM_FILE_OK, status().front());
    EXPECT_EQ(100 - 8, quota_plugin_delegate()->available_space());
    reset_results();

    if (will_operation) {
      // WillWrite doesn't actually write.
      EXPECT_EQ(0, GetPlatformFileSize());
      // Adjust the actual file size to 'fake' write to proceed testing.
      SetPlatformFileSize(8);
    } else {
      EXPECT_EQ(8, GetPlatformFileSize());
      ReadPlatformFile(&read_buffer);
      EXPECT_EQ(data, read_buffer);
    }

    // Write 5 bytes at offset 5 (-> length=10).
    data = "55555";
    Write(5, data, will_operation);
    MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(1U, num_results());
    EXPECT_EQ(static_cast<int>(data.size()), bytes_written().front());
    EXPECT_EQ(base::PLATFORM_FILE_OK, status().front());
    EXPECT_EQ(100 - 10, quota_plugin_delegate()->available_space());
    reset_results();

    if (will_operation) {
      EXPECT_EQ(8, GetPlatformFileSize());
      SetPlatformFileSize(10);
    } else {
      EXPECT_EQ(10, GetPlatformFileSize());
      ReadPlatformFile(&read_buffer);
      EXPECT_EQ("1234555555", read_buffer);
    }

    // Write 7 bytes at offset 8 (-> length=15).
    data = "9012345";
    Write(8, data, will_operation);
    MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(1U, num_results());
    EXPECT_EQ(static_cast<int>(data.size()), bytes_written().front());
    EXPECT_EQ(base::PLATFORM_FILE_OK, status().front());
    EXPECT_EQ(100 - 15, quota_plugin_delegate()->available_space());
    reset_results();

    if (will_operation) {
      EXPECT_EQ(10, GetPlatformFileSize());
      SetPlatformFileSize(15);
    } else {
      EXPECT_EQ(15, GetPlatformFileSize());
      ReadPlatformFile(&read_buffer);
      EXPECT_EQ("123455559012345", read_buffer);
    }

    // Write 2 bytes at offset 2 (-> length=15).
    data = "33";
    Write(2, data, will_operation);
    MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(1U, num_results());
    EXPECT_EQ(static_cast<int>(data.size()), bytes_written().front());
    EXPECT_EQ(base::PLATFORM_FILE_OK, status().front());
    EXPECT_EQ(100 - 15, quota_plugin_delegate()->available_space());
    reset_results();

    if (will_operation) {
      EXPECT_EQ(15, GetPlatformFileSize());
    } else {
      EXPECT_EQ(15, GetPlatformFileSize());
      ReadPlatformFile(&read_buffer);
      EXPECT_EQ("123355559012345", read_buffer);
    }

    // Write 4 bytes at offset 20 (-> length=24).
    data = "XXXX";
    Write(20, data, will_operation);
    MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(1U, num_results());
    EXPECT_EQ(static_cast<int>(data.size()), bytes_written().front());
    EXPECT_EQ(base::PLATFORM_FILE_OK, status().front());
    EXPECT_EQ(100 - 24, quota_plugin_delegate()->available_space());
    reset_results();

    if (will_operation) {
      EXPECT_EQ(15, GetPlatformFileSize());
      SetPlatformFileSize(24);
    } else {
      EXPECT_EQ(24, GetPlatformFileSize());
      ReadPlatformFile(&read_buffer);
      EXPECT_EQ(std::string("123355559012345\0\0\0\0\0XXXX", 24), read_buffer);
    }

    quota_plugin_delegate()->set_available_space(5);

    // Quota error case.  Write 7 bytes at offset 23 (-> length is unchanged)
    data = "ABCDEFG";
    Write(23, data, will_operation);
    MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(1U, num_results());
    EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, status().front());
    EXPECT_EQ(5, quota_plugin_delegate()->available_space());
    reset_results();

    // Overlapping write.  Write 6 bytes at offset 2 (-> length is unchanged)
    data = "ABCDEF";
    Write(2, data, will_operation);
    MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(1U, num_results());
    EXPECT_EQ(static_cast<int>(data.size()), bytes_written().front());
    EXPECT_EQ(base::PLATFORM_FILE_OK, status().front());
    EXPECT_EQ(5, quota_plugin_delegate()->available_space());
    reset_results();

    // Overlapping + extending the file size, but within the quota.
    // Write 6 bytes at offset 23 (-> length=29).
    Write(23, data, will_operation);
    MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(1U, num_results());
    EXPECT_EQ(static_cast<int>(data.size()), bytes_written().front());
    EXPECT_EQ(base::PLATFORM_FILE_OK, status().front());
    EXPECT_EQ(0, quota_plugin_delegate()->available_space());
    reset_results();

    if (!will_operation) {
      EXPECT_EQ(29, GetPlatformFileSize());
      ReadPlatformFile(&read_buffer);
      EXPECT_EQ(std::string("12ABCDEF9012345\0\0\0\0\0XXXABCDEF", 29),
                read_buffer);
    }
  }

  void SetLengthTestBody(bool will_operation) {
    quota_plugin_delegate()->set_available_space(100);

    SetLength(0, will_operation);
    MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(1U, num_results());
    EXPECT_EQ(base::PLATFORM_FILE_OK, status().front());
    EXPECT_EQ(0, GetPlatformFileSize());
    EXPECT_EQ(100, quota_plugin_delegate()->available_space());
    reset_results();

    SetLength(8, will_operation);
    MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(1U, num_results());
    EXPECT_EQ(base::PLATFORM_FILE_OK, status().front());
    EXPECT_EQ(100 - 8, quota_plugin_delegate()->available_space());
    reset_results();

    if (will_operation) {
      EXPECT_EQ(0, GetPlatformFileSize());
      SetPlatformFileSize(8);
    } else {
      EXPECT_EQ(8, GetPlatformFileSize());
    }

    SetLength(16, will_operation);
    MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(1U, num_results());
    EXPECT_EQ(base::PLATFORM_FILE_OK, status().front());
    EXPECT_EQ(100 - 16, quota_plugin_delegate()->available_space());
    reset_results();

    if (will_operation) {
      EXPECT_EQ(8, GetPlatformFileSize());
      SetPlatformFileSize(16);
    } else {
      EXPECT_EQ(16, GetPlatformFileSize());
    }

    SetLength(4, will_operation);
    MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(1U, num_results());
    EXPECT_EQ(base::PLATFORM_FILE_OK, status().front());
    EXPECT_EQ(100 - 4, quota_plugin_delegate()->available_space());
    reset_results();

    if (will_operation) {
      EXPECT_EQ(16, GetPlatformFileSize());
      SetPlatformFileSize(4);
    } else {
      EXPECT_EQ(4, GetPlatformFileSize());
    }

    SetLength(0, will_operation);
    MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(1U, num_results());
    EXPECT_EQ(base::PLATFORM_FILE_OK, status().front());
    EXPECT_EQ(100, quota_plugin_delegate()->available_space());
    reset_results();

    if (will_operation) {
      EXPECT_EQ(4, GetPlatformFileSize());
      SetPlatformFileSize(0);
    } else {
      EXPECT_EQ(0, GetPlatformFileSize());
    }

    quota_plugin_delegate()->set_available_space(5);

    // Quota error case.
    SetLength(7, will_operation);
    MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(1U, num_results());
    EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, status().front());
    EXPECT_EQ(5, quota_plugin_delegate()->available_space());
    reset_results();
  }

  QuotaMockPluginDelegate* quota_plugin_delegate() {
    return static_cast<QuotaMockPluginDelegate*>(delegate());
  }

  void Write(int64_t offset, const std::string& data, bool will_operation) {
    if (will_operation) {
      ASSERT_TRUE(quota_file_io_->WillWrite(
          offset, data.size(),
          base::Bind(&QuotaFileIOTest::DidWrite, weak_factory_.GetWeakPtr())));
    } else {
      ASSERT_TRUE(quota_file_io_->Write(
          offset, data.c_str(), data.size(),
          base::Bind(&QuotaFileIOTest::DidWrite, weak_factory_.GetWeakPtr())));
    }
  }

  void SetLength(int64_t length, bool will_operation) {
    if (will_operation) {
      ASSERT_TRUE(quota_file_io_->WillSetLength(
          length,
          base::Bind(&QuotaFileIOTest::DidSetLength,
                     weak_factory_.GetWeakPtr())));
    } else {
      ASSERT_TRUE(quota_file_io_->SetLength(
          length,
          base::Bind(&QuotaFileIOTest::DidSetLength,
                     weak_factory_.GetWeakPtr())));
    }
  }

  void DidWrite(PlatformFileError status, int bytes_written) {
    status_.push_back(status);
    bytes_written_.push_back(bytes_written);
  }

  void DidSetLength(PlatformFileError status) {
    status_.push_back(status);
  }

  size_t num_results() const { return status_.size(); }
  const std::deque<int>& bytes_written() const { return bytes_written_; }
  const std::deque<PlatformFileError>& status() const { return status_; }

  void reset_results() {
    bytes_written_.clear();
    status_.clear();
  }

  void pop_result() {
    bytes_written_.pop_front();
    status_.pop_front();
  }

  void ReadPlatformFile(std::string* data) {
    data->clear();
    char buf[256];
    int32_t read_offset = 0;
    for (;;) {
      int rv = base::ReadPlatformFile(file_, read_offset, buf, sizeof(buf));
      ASSERT_GE(rv, 0);
      if (rv == 0)
        break;
      read_offset += rv;
      data->append(buf, rv);
    }
  }

  int64_t GetPlatformFileSize() {
    base::PlatformFileInfo info;
    EXPECT_TRUE(base::GetPlatformFileInfo(file_, &info));
    return info.size;
  }

  void SetPlatformFileSize(int64_t length) {
    EXPECT_TRUE(base::TruncatePlatformFile(file_, length));
  }

 private:
  base::ScopedTempDir dir_;
  PlatformFile file_;
  scoped_ptr<QuotaFileIO> quota_file_io_;
  std::deque<int> bytes_written_;
  std::deque<PlatformFileError> status_;
  base::WeakPtrFactory<QuotaFileIOTest> weak_factory_;
};

TEST_F(QuotaFileIOTest, Write) {
  WriteTestBody(false);
}

TEST_F(QuotaFileIOTest, WillWrite) {
  WriteTestBody(true);
}

TEST_F(QuotaFileIOTest, SetLength) {
  SetLengthTestBody(false);
}

TEST_F(QuotaFileIOTest, WillSetLength) {
  SetLengthTestBody(true);
}

TEST_F(QuotaFileIOTest, ParallelWrites) {
  quota_plugin_delegate()->set_available_space(22);
  std::string read_buffer;

  const std::string data1[] = {
    std::string("12345678"),
    std::string("55555"),
    std::string("9012345"),
  };
  Write(0, data1[0], false);
  Write(5, data1[1], false);
  Write(8, data1[2], false);
  MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(ARRAYSIZE_UNSAFE(data1), num_results());
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data1); ++i) {
    EXPECT_EQ(static_cast<int>(data1[i].size()), bytes_written().front());
    EXPECT_EQ(base::PLATFORM_FILE_OK, status().front());
    pop_result();
  }

  EXPECT_EQ(22 - 15, quota_plugin_delegate()->available_space());
  EXPECT_EQ(15, GetPlatformFileSize());
  ReadPlatformFile(&read_buffer);
  EXPECT_EQ("123455559012345", read_buffer);

  // The second write will fail for quota error.
  const std::string data2[] = {
    std::string("33"),
    std::string("XXXX"),
  };
  Write(2, data2[0], false);
  Write(20, data2[1], false);
  MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(ARRAYSIZE_UNSAFE(data2), num_results());
  EXPECT_EQ(static_cast<int>(data2[0].size()), bytes_written().front());
  EXPECT_EQ(base::PLATFORM_FILE_OK, status().front());
  pop_result();
  EXPECT_EQ(0, bytes_written().front());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, status().front());
  pop_result();

  EXPECT_EQ(22 - 15, quota_plugin_delegate()->available_space());
  EXPECT_EQ(15, GetPlatformFileSize());
  ReadPlatformFile(&read_buffer);
  EXPECT_EQ("123355559012345", read_buffer);
}

}  // namespace ppapi
}  // namespace webkit
