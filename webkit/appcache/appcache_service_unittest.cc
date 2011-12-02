// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "base/pickle.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/appcache/appcache_response.h"
#include "webkit/appcache/appcache_service.h"
#include "webkit/appcache/mock_appcache_storage.h"


namespace appcache {
namespace {

const int64 kMockGroupId = 1;
const int64 kMockCacheId = 1;
const int64 kMockResponseId = 1;
const int64 kMissingCacheId = 5;
const int64 kMissingResponseId = 5;
const char kMockHeaders[] =
    "HTTP/1.0 200 OK\0Content-Length: 5\0\0";
const char kMockBody[] = "Hello";
const int kMockBodySize = 5;

class MockResponseReader : public AppCacheResponseReader {
 public:
  MockResponseReader(int64 response_id,
                     net::HttpResponseInfo* info, int info_size,
                     const char* data, int data_size)
      : AppCacheResponseReader(response_id, 0, NULL),
        info_(info), info_size_(info_size),
        data_(data), data_size_(data_size) {
  }
  virtual void ReadInfo(HttpResponseInfoIOBuffer* info_buf,
                        net::OldCompletionCallback* callback) OVERRIDE {
    info_buffer_ = info_buf;
    user_callback_ = callback;  // Cleared on completion.

    int rv = info_.get() ? info_size_ : net::ERR_FAILED;
    info_buffer_->http_info.reset(info_.release());
    info_buffer_->response_data_size = data_size_;
    ScheduleUserCallback(rv);
  }
  virtual void ReadData(net::IOBuffer* buf, int buf_len,
                        net::OldCompletionCallback* callback) OVERRIDE {
    buffer_ = buf;
    buffer_len_ = buf_len;
    user_callback_ = callback;  // Cleared on completion.

    if (!data_) {
      ScheduleUserCallback(net::ERR_CACHE_READ_FAILURE);
      return;
    }
    DCHECK(buf_len >= data_size_);
    memcpy(buf->data(), data_, data_size_);
    ScheduleUserCallback(data_size_);
    data_size_ = 0;
  }

 private:
  void ScheduleUserCallback(int result) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MockResponseReader::InvokeUserOldCompletionCallback,
                   weak_factory_.GetWeakPtr(), result));
  }

  scoped_ptr<net::HttpResponseInfo> info_;
  int info_size_;
  const char* data_;
  int data_size_;
};

}  // namespace


class AppCacheServiceTest : public testing::Test {
 public:
  AppCacheServiceTest()
      : kOrigin("http://hello/"),
        kManifestUrl(kOrigin.Resolve("manifest")),
        service_(new AppCacheService(NULL)),
        delete_result_(net::OK), delete_completion_count_(0),
        ALLOW_THIS_IN_INITIALIZER_LIST(deletion_callback_(
            base::Bind(&AppCacheServiceTest::OnDeleteAppCachesComplete,
                       base::Unretained(this)))) {
    // Setup to use mock storage.
    service_->storage_.reset(new MockAppCacheStorage(service_.get()));
  }

  void OnDeleteAppCachesComplete(int result) {
    delete_result_ = result;
    ++delete_completion_count_;
  }

  MockAppCacheStorage* mock_storage() {
    return static_cast<MockAppCacheStorage*>(service_->storage());
  }

  void ResetStorage() {
    service_->storage_.reset(new MockAppCacheStorage(service_.get()));
  }

  bool IsGroupStored(const GURL& manifest_url) {
    return mock_storage()->IsGroupForManifestStored(manifest_url);
  }

  int CountPendingHelpers() {
    return service_->pending_helpers_.size();
  }

  void SetupMockGroup() {
    scoped_ptr<net::HttpResponseInfo> info(MakeMockResponseInfo());
    const int kMockInfoSize = GetResponseInfoSize(info.get());

    // Create a mock group, cache, and entry and stuff them into mock storage.
    scoped_refptr<AppCacheGroup> group(
        new AppCacheGroup(service_.get(), kManifestUrl, kMockGroupId));
    scoped_refptr<AppCache> cache(new AppCache(service_.get(), kMockCacheId));
    cache->AddEntry(
        kManifestUrl,
        AppCacheEntry(AppCacheEntry::MANIFEST, kMockResponseId,
                      kMockInfoSize + kMockBodySize));
    cache->set_complete(true);
    group->AddCache(cache);
    mock_storage()->AddStoredGroup(group);
    mock_storage()->AddStoredCache(cache);
  }

  void SetupMockReader(
      bool valid_info, bool valid_data, bool valid_size) {
    net::HttpResponseInfo* info = valid_info ? MakeMockResponseInfo() : NULL;
    int info_size = info ? GetResponseInfoSize(info) : 0;
    const char* data = valid_data ? kMockBody : NULL;
    int data_size = valid_size ? kMockBodySize : 3;
    mock_storage()->SimulateResponseReader(
        new MockResponseReader(kMockResponseId, info, info_size,
                               data, data_size));
  }

  net::HttpResponseInfo* MakeMockResponseInfo() {
    net::HttpResponseInfo* info = new net::HttpResponseInfo;
    info->request_time = base::Time::Now();
    info->response_time = base::Time::Now();
    info->was_cached = false;
    info->headers = new net::HttpResponseHeaders(
        std::string(kMockHeaders, arraysize(kMockHeaders)));
    return info;
  }

  int GetResponseInfoSize(const net::HttpResponseInfo* info) {
    Pickle pickle;
    return PickleResponseInfo(&pickle, info);
  }

  int PickleResponseInfo(Pickle* pickle, const net::HttpResponseInfo* info) {
    const bool kSkipTransientHeaders = true;
    const bool kTruncated = false;
    info->Persist(pickle, kSkipTransientHeaders, kTruncated);
    return pickle->size();
  }

  const GURL kOrigin;
  const GURL kManifestUrl;

  scoped_ptr<AppCacheService> service_;
  int delete_result_;
  int delete_completion_count_;
  net::CompletionCallback deletion_callback_;
};

TEST_F(AppCacheServiceTest, DeleteAppCachesForOrigin) {
  // Without giving mock storage simiulated info, should fail.
  service_->DeleteAppCachesForOrigin(kOrigin, deletion_callback_);
  EXPECT_EQ(0, delete_completion_count_);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(1, delete_completion_count_);
  EXPECT_EQ(net::ERR_FAILED, delete_result_);
  delete_completion_count_ = 0;

  // Should succeed given an empty info collection.
  mock_storage()->SimulateGetAllInfo(new AppCacheInfoCollection);
  service_->DeleteAppCachesForOrigin(kOrigin, deletion_callback_);
  EXPECT_EQ(0, delete_completion_count_);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(1, delete_completion_count_);
  EXPECT_EQ(net::OK, delete_result_);
  delete_completion_count_ = 0;

  scoped_refptr<AppCacheInfoCollection> info(new AppCacheInfoCollection);

  // Should succeed given a non-empty info collection.
  AppCacheInfo mock_manifest_1;
  AppCacheInfo mock_manifest_2;
  AppCacheInfo mock_manifest_3;
  mock_manifest_1.manifest_url = kOrigin.Resolve("manifest1");
  mock_manifest_2.manifest_url = kOrigin.Resolve("manifest2");
  mock_manifest_3.manifest_url = kOrigin.Resolve("manifest3");
  AppCacheInfoVector info_vector;
  info_vector.push_back(mock_manifest_1);
  info_vector.push_back(mock_manifest_2);
  info_vector.push_back(mock_manifest_3);
  info->infos_by_origin[kOrigin] = info_vector;
  mock_storage()->SimulateGetAllInfo(info);
  service_->DeleteAppCachesForOrigin(kOrigin, deletion_callback_);
  EXPECT_EQ(0, delete_completion_count_);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(1, delete_completion_count_);
  EXPECT_EQ(net::OK, delete_result_);
  delete_completion_count_ = 0;

  // Should fail if storage fails to delete.
  info->infos_by_origin[kOrigin] = info_vector;
  mock_storage()->SimulateGetAllInfo(info);
  mock_storage()->SimulateMakeGroupObsoleteFailure();
  service_->DeleteAppCachesForOrigin(kOrigin, deletion_callback_);
  EXPECT_EQ(0, delete_completion_count_);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(1, delete_completion_count_);
  EXPECT_EQ(net::ERR_FAILED, delete_result_);
  delete_completion_count_ = 0;

  // Should complete with abort error if the service is deleted
  // prior to a delete completion.
  service_->DeleteAppCachesForOrigin(kOrigin, deletion_callback_);
  EXPECT_EQ(0, delete_completion_count_);
  service_.reset();  // kill it
  EXPECT_EQ(1, delete_completion_count_);
  EXPECT_EQ(net::ERR_ABORTED, delete_result_);
  delete_completion_count_ = 0;

  // Let any tasks lingering from the sudden deletion run and verify
  // no other completion calls occur.
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(0, delete_completion_count_);
}

TEST_F(AppCacheServiceTest, CheckAppCacheResponse) {
  // Check a non-existing manifest.
  EXPECT_FALSE(IsGroupStored(kManifestUrl));
  service_->CheckAppCacheResponse(kManifestUrl, 1, 1);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(0, CountPendingHelpers());
  EXPECT_FALSE(IsGroupStored(kManifestUrl));
  ResetStorage();

  // Check a response that looks good.
  // Nothing should be deleted.
  SetupMockGroup();
  EXPECT_TRUE(IsGroupStored(kManifestUrl));
  SetupMockReader(true, true, true);
  service_->CheckAppCacheResponse(kManifestUrl, kMockCacheId, kMockResponseId);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(0, CountPendingHelpers());
  EXPECT_TRUE(IsGroupStored(kManifestUrl));
  ResetStorage();

  // Check a response for which there is no cache entry.
  // The group should get deleted.
  SetupMockGroup();
  service_->CheckAppCacheResponse(kManifestUrl, kMockCacheId,
                                  kMissingResponseId);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(0, CountPendingHelpers());
  EXPECT_FALSE(IsGroupStored(kManifestUrl));
  ResetStorage();

  // Check a response for which there is no manifest entry in a newer version
  // of the cache. Nothing should get deleted in this case.
  SetupMockGroup();
  service_->CheckAppCacheResponse(kManifestUrl, kMissingCacheId,
                                  kMissingResponseId);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(0, CountPendingHelpers());
  EXPECT_TRUE(IsGroupStored(kManifestUrl));
  ResetStorage();

  // Check a response with bad headers.
  SetupMockGroup();
  service_->CheckAppCacheResponse(kManifestUrl, kMockCacheId, kMockResponseId);
  SetupMockReader(false, true, true);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(0, CountPendingHelpers());
  EXPECT_FALSE(IsGroupStored(kManifestUrl));
  ResetStorage();

  // Check a response with bad data.
  SetupMockGroup();
  service_->CheckAppCacheResponse(kManifestUrl, kMockCacheId, kMockResponseId);
  SetupMockReader(true, false, true);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(0, CountPendingHelpers());
  EXPECT_FALSE(IsGroupStored(kManifestUrl));
  ResetStorage();

  // Check a response with truncated data.
  SetupMockGroup();
  service_->CheckAppCacheResponse(kManifestUrl, kMockCacheId, kMockResponseId);
  SetupMockReader(true, true, false);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(0, CountPendingHelpers());
  EXPECT_FALSE(IsGroupStored(kManifestUrl));
  ResetStorage();

  service_.reset();  // Clean up.
  MessageLoop::current()->RunAllPending();
}

}  // namespace appcache
