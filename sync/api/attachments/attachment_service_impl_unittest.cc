// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/attachments/attachment_service_impl.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "sync/internal_api/public/attachments/fake_attachment_downloader.h"
#include "sync/internal_api/public/attachments/fake_attachment_uploader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class MockAttachmentStore : public AttachmentStore,
                            public base::SupportsWeakPtr<MockAttachmentStore> {
 public:
  MockAttachmentStore() {}

  virtual void Read(const AttachmentIdList& ids,
                    const ReadCallback& callback) OVERRIDE {
    read_ids.push_back(ids);
    read_callbacks.push_back(callback);
  }

  virtual void Write(const AttachmentList& attachments,
                     const WriteCallback& callback) OVERRIDE {
    NOTREACHED();
  }

  virtual void Drop(const AttachmentIdList& ids,
                    const DropCallback& callback) OVERRIDE {
    NOTREACHED();
  }

  // Respond to Read request. Attachments found in local_attachments should be
  // returned, everything else should be reported unavailable.
  void RespondToRead(const AttachmentIdSet& local_attachments) {
    scoped_refptr<base::RefCountedString> data = new base::RefCountedString();
    ReadCallback callback = read_callbacks.back();
    AttachmentIdList ids = read_ids.back();
    read_callbacks.pop_back();
    read_ids.pop_back();

    scoped_ptr<AttachmentMap> attachments(new AttachmentMap());
    scoped_ptr<AttachmentIdList> unavailable_attachments(
        new AttachmentIdList());
    for (AttachmentIdList::const_iterator iter = ids.begin(); iter != ids.end();
         ++iter) {
      if (local_attachments.find(*iter) != local_attachments.end()) {
        Attachment attachment = Attachment::CreateWithId(*iter, data);
        attachments->insert(std::make_pair(*iter, attachment));
      } else {
        unavailable_attachments->push_back(*iter);
      }
    }
    Result result =
        unavailable_attachments->empty() ? SUCCESS : UNSPECIFIED_ERROR;

    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   result,
                   base::Passed(&attachments),
                   base::Passed(&unavailable_attachments)));
  }

  std::vector<AttachmentIdList> read_ids;
  std::vector<ReadCallback> read_callbacks;

  DISALLOW_COPY_AND_ASSIGN(MockAttachmentStore);
};

class MockAttachmentDownloader
    : public AttachmentDownloader,
      public base::SupportsWeakPtr<MockAttachmentDownloader> {
 public:
  MockAttachmentDownloader() {}

  virtual void DownloadAttachment(const AttachmentId& id,
                                  const DownloadCallback& callback) OVERRIDE {
    ASSERT_TRUE(download_requests.find(id) == download_requests.end());
    download_requests.insert(std::make_pair(id, callback));
  }

  // Multiple requests to download will be active at the same time.
  // RespondToDownload should respond to only one of them.
  void RespondToDownload(const AttachmentId& id, const DownloadResult& result) {
    ASSERT_TRUE(download_requests.find(id) != download_requests.end());
    scoped_ptr<Attachment> attachment;
    if (result == DOWNLOAD_SUCCESS) {
      scoped_refptr<base::RefCountedString> data = new base::RefCountedString();
      attachment.reset(new Attachment(Attachment::CreateWithId(id, data)));
    }
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(download_requests[id], result, base::Passed(&attachment)));

    download_requests.erase(id);
  }

  std::map<AttachmentId, DownloadCallback> download_requests;

  DISALLOW_COPY_AND_ASSIGN(MockAttachmentDownloader);
};

class AttachmentServiceImplTest : public testing::Test {
 protected:
  AttachmentServiceImplTest() {}

  virtual void SetUp() OVERRIDE {
    scoped_ptr<MockAttachmentStore> attachment_store(new MockAttachmentStore());
    scoped_ptr<AttachmentUploader> attachment_uploader(
        new FakeAttachmentUploader());
    scoped_ptr<MockAttachmentDownloader> attachment_downloader(
        new MockAttachmentDownloader());

    attachment_store_ = attachment_store->AsWeakPtr();
    attachment_downloader_ = attachment_downloader->AsWeakPtr();

    attachment_service_.reset(new AttachmentServiceImpl(
        attachment_store.PassAs<AttachmentStore>(),
        attachment_uploader.Pass(),
        attachment_downloader.PassAs<AttachmentDownloader>(),
        NULL));
  }

  virtual void TearDown() OVERRIDE {
    attachment_service_.reset();
    ASSERT_FALSE(attachment_store_);
    ASSERT_FALSE(attachment_downloader_);
  }

  AttachmentService* attachment_service() { return attachment_service_.get(); }

  AttachmentService::GetOrDownloadCallback download_callback() {
    return base::Bind(&AttachmentServiceImplTest::DownloadDone,
                      base::Unretained(this));
  }

  void DownloadDone(const AttachmentService::GetOrDownloadResult& result,
                    scoped_ptr<AttachmentMap> attachments) {
    download_results_.push_back(result);
    last_download_attachments_ = attachments.Pass();
  }

  void RunLoop() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  const std::vector<AttachmentService::GetOrDownloadResult>&
  download_results() {
    return download_results_;
  }

  AttachmentMap* last_download_attachments() {
    return last_download_attachments_.get();
  }

  MockAttachmentStore* store() { return attachment_store_.get(); }

  MockAttachmentDownloader* downloader() {
    return attachment_downloader_.get();
  }

 private:
  base::MessageLoop message_loop_;
  base::WeakPtr<MockAttachmentStore> attachment_store_;
  base::WeakPtr<MockAttachmentDownloader> attachment_downloader_;
  scoped_ptr<AttachmentService> attachment_service_;

  std::vector<AttachmentService::GetOrDownloadResult> download_results_;
  scoped_ptr<AttachmentMap> last_download_attachments_;
};

TEST_F(AttachmentServiceImplTest, GetOrDownload_EmptyAttachmentList) {
  AttachmentIdList attachment_ids;
  attachment_service()->GetOrDownloadAttachments(attachment_ids,
                                                 download_callback());
  store()->RespondToRead(AttachmentIdSet());

  RunLoop();
  EXPECT_EQ(1U, download_results().size());
  EXPECT_EQ(0U, last_download_attachments()->size());
}

TEST_F(AttachmentServiceImplTest, GetOrDownload_Local) {
  AttachmentIdList attachment_ids;
  attachment_ids.push_back(AttachmentId::Create());
  attachment_service()->GetOrDownloadAttachments(attachment_ids,
                                                 download_callback());
  AttachmentIdSet local_attachments;
  local_attachments.insert(attachment_ids[0]);
  store()->RespondToRead(local_attachments);

  RunLoop();
  EXPECT_EQ(1U, download_results().size());
  EXPECT_EQ(1U, last_download_attachments()->size());
  EXPECT_TRUE(last_download_attachments()->find(attachment_ids[0]) !=
              last_download_attachments()->end());
}

TEST_F(AttachmentServiceImplTest, GetOrDownload_LocalRemoteUnavailable) {
  // Create attachment list with 3 ids.
  AttachmentIdList attachment_ids;
  attachment_ids.push_back(AttachmentId::Create());
  attachment_ids.push_back(AttachmentId::Create());
  attachment_ids.push_back(AttachmentId::Create());
  // Call attachment service.
  attachment_service()->GetOrDownloadAttachments(attachment_ids,
                                                 download_callback());
  // Ensure AttachmentStore is called.
  EXPECT_FALSE(store()->read_ids.empty());

  // make AttachmentStore return only attachment 0.
  AttachmentIdSet local_attachments;
  local_attachments.insert(attachment_ids[0]);
  store()->RespondToRead(local_attachments);
  RunLoop();
  // Ensure Downloader called with right attachment ids
  EXPECT_EQ(2U, downloader()->download_requests.size());

  // Make downloader return attachment 1.
  downloader()->RespondToDownload(attachment_ids[1],
                                  AttachmentDownloader::DOWNLOAD_SUCCESS);
  RunLoop();
  // Ensure consumer callback is not called.
  EXPECT_TRUE(download_results().empty());

  // Make downloader fail attachment 2.
  downloader()->RespondToDownload(
      attachment_ids[2], AttachmentDownloader::DOWNLOAD_UNSPECIFIED_ERROR);
  RunLoop();
  // Ensure callback is called
  EXPECT_FALSE(download_results().empty());
  // There should be only two attachments returned, 0 and 1.
  EXPECT_EQ(2U, last_download_attachments()->size());
  EXPECT_TRUE(last_download_attachments()->find(attachment_ids[0]) !=
              last_download_attachments()->end());
  EXPECT_TRUE(last_download_attachments()->find(attachment_ids[1]) !=
              last_download_attachments()->end());
  EXPECT_TRUE(last_download_attachments()->find(attachment_ids[2]) ==
              last_download_attachments()->end());
}

}  // namespace syncer
