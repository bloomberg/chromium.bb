// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/attachments/fake_attachment_uploader.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "sync/api/attachments/attachment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

const char kAttachmentData[] = "some data";

}  // namespace

class FakeAttachmentUploaderTest : public testing::Test {
 protected:
  virtual void SetUp() {
    upload_callback_count = 0;
    upload_callback = base::Bind(&FakeAttachmentUploaderTest::Increment,
                                 base::Unretained(this),
                                 &upload_callback_count);
  }
  base::MessageLoop message_loop;
  FakeAttachmentUploader uploader;
  int upload_callback_count;
  AttachmentUploader::UploadCallback upload_callback;

 private:
  void Increment(int* success_count,
                 const AttachmentUploader::UploadResult& result,
                 const AttachmentId& ignored) {
    if (result == AttachmentUploader::UPLOAD_SUCCESS) {
      ++(*success_count);
    }
  }
};

// Call upload attachment several times, see that the supplied callback is
// invoked the same number of times with a result of SUCCESS.
TEST_F(FakeAttachmentUploaderTest, UploadAttachment) {
  scoped_refptr<base::RefCountedString> some_data(new base::RefCountedString);
  some_data->data() = kAttachmentData;
  Attachment attachment1 = Attachment::Create(some_data);
  Attachment attachment2 = Attachment::Create(some_data);
  Attachment attachment3 = Attachment::Create(some_data);
  uploader.UploadAttachment(attachment1, upload_callback);
  uploader.UploadAttachment(attachment2, upload_callback);
  uploader.UploadAttachment(attachment3, upload_callback);
  message_loop.RunUntilIdle();
  EXPECT_EQ(upload_callback_count, 3);
}

}  // namespace syncer
