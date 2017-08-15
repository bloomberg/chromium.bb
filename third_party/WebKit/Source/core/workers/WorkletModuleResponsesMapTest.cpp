// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/modulescript/ModuleScriptCreationParams.h"
#include "core/workers/WorkletModuleResponsesMap.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/Optional.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class ClientImpl final : public GarbageCollectedFinalized<ClientImpl>,
                         public WorkletModuleResponsesMap::Client {
  USING_GARBAGE_COLLECTED_MIXIN(ClientImpl);

 public:
  enum class Result { kInitial, kOK, kNeedsFetching, kFailed };

  void OnRead(const ModuleScriptCreationParams& params) override {
    ASSERT_EQ(Result::kInitial, result_);
    result_ = Result::kOK;
    params_.emplace(params);
  }

  void OnFetchNeeded() override {
    ASSERT_EQ(Result::kInitial, result_);
    result_ = Result::kNeedsFetching;
  }

  void OnFailed() override {
    ASSERT_EQ(Result::kInitial, result_);
    result_ = Result::kFailed;
  }

  Result GetResult() const { return result_; }
  WTF::Optional<ModuleScriptCreationParams> GetParams() const {
    return params_;
  }

 private:
  Result result_ = Result::kInitial;
  WTF::Optional<ModuleScriptCreationParams> params_;
};

}  // namespace

TEST(WorkletModuleResponsesMapTest, Basic) {
  WorkletModuleResponsesMap* map = new WorkletModuleResponsesMap;
  const KURL kUrl(kParsedURLString, "https://example.com/foo.js");

  // An initial read call creates a placeholder entry and asks the client to
  // fetch a module script.
  ClientImpl* client1 = new ClientImpl;
  map->ReadOrCreateEntry(kUrl, client1);
  EXPECT_EQ(ClientImpl::Result::kNeedsFetching, client1->GetResult());
  EXPECT_FALSE(client1->GetParams().has_value());

  // The entry is now being fetched. Following read calls should wait for the
  // completion.
  ClientImpl* client2 = new ClientImpl;
  map->ReadOrCreateEntry(kUrl, client2);
  EXPECT_EQ(ClientImpl::Result::kInitial, client2->GetResult());

  ClientImpl* client3 = new ClientImpl;
  map->ReadOrCreateEntry(kUrl, client3);
  EXPECT_EQ(ClientImpl::Result::kInitial, client3->GetResult());

  // An update call should notify the waiting clients.
  ModuleScriptCreationParams params(kUrl, "// dummy script",
                                    WebURLRequest::kFetchCredentialsModeOmit,
                                    kNotSharableCrossOrigin);
  map->UpdateEntry(kUrl, params);
  EXPECT_EQ(ClientImpl::Result::kOK, client2->GetResult());
  EXPECT_TRUE(client2->GetParams().has_value());
  EXPECT_EQ(ClientImpl::Result::kOK, client3->GetResult());
  EXPECT_TRUE(client3->GetParams().has_value());
}

TEST(WorkletModuleResponsesMapTest, Failure) {
  WorkletModuleResponsesMap* map = new WorkletModuleResponsesMap;
  const KURL kUrl(kParsedURLString, "https://example.com/foo.js");

  // An initial read call creates a placeholder entry and asks the client to
  // fetch a module script.
  ClientImpl* client1 = new ClientImpl;
  map->ReadOrCreateEntry(kUrl, client1);
  EXPECT_EQ(ClientImpl::Result::kNeedsFetching, client1->GetResult());
  EXPECT_FALSE(client1->GetParams().has_value());

  // The entry is now being fetched. Following read calls should wait for the
  // completion.
  ClientImpl* client2 = new ClientImpl;
  map->ReadOrCreateEntry(kUrl, client2);
  EXPECT_EQ(ClientImpl::Result::kInitial, client2->GetResult());

  ClientImpl* client3 = new ClientImpl;
  map->ReadOrCreateEntry(kUrl, client3);
  EXPECT_EQ(ClientImpl::Result::kInitial, client3->GetResult());

  // An invalidation call should notify the waiting clients.
  map->InvalidateEntry(kUrl);
  EXPECT_EQ(ClientImpl::Result::kFailed, client2->GetResult());
  EXPECT_FALSE(client2->GetParams().has_value());
  EXPECT_EQ(ClientImpl::Result::kFailed, client3->GetResult());
  EXPECT_FALSE(client3->GetParams().has_value());
}

TEST(WorkletModuleResponsesMapTest, Isolation) {
  WorkletModuleResponsesMap* map = new WorkletModuleResponsesMap;
  const KURL kUrl1(kParsedURLString, "https://example.com/foo.js");
  const KURL kUrl2(kParsedURLString, "https://example.com/bar.js");

  // An initial read call for |kUrl1| creates a placeholder entry and asks the
  // client to fetch a module script.
  ClientImpl* client1 = new ClientImpl;
  map->ReadOrCreateEntry(kUrl1, client1);
  EXPECT_EQ(ClientImpl::Result::kNeedsFetching, client1->GetResult());
  EXPECT_FALSE(client1->GetParams().has_value());

  // The entry is now being fetched. Following read calls for |kUrl1| should
  // wait for the completion.
  ClientImpl* client2 = new ClientImpl;
  map->ReadOrCreateEntry(kUrl1, client2);
  EXPECT_EQ(ClientImpl::Result::kInitial, client2->GetResult());

  // An initial read call for |kUrl2| also creates a placeholder entry and asks
  // the client to fetch a module script.
  ClientImpl* client3 = new ClientImpl;
  map->ReadOrCreateEntry(kUrl2, client3);
  EXPECT_EQ(ClientImpl::Result::kNeedsFetching, client3->GetResult());
  EXPECT_FALSE(client3->GetParams().has_value());

  // The entry is now being fetched. Following read calls for |kUrl2| should
  // wait for the completion.
  ClientImpl* client4 = new ClientImpl;
  map->ReadOrCreateEntry(kUrl2, client4);
  EXPECT_EQ(ClientImpl::Result::kInitial, client4->GetResult());

  // The read call for |kUrl2| should not affect the other entry for |kUrl1|.
  EXPECT_EQ(ClientImpl::Result::kInitial, client2->GetResult());

  // An update call for |kUrl2| should notify the waiting clients for |kUrl2|.
  ModuleScriptCreationParams params(kUrl2, "// dummy script",
                                    WebURLRequest::kFetchCredentialsModeOmit,
                                    kNotSharableCrossOrigin);
  map->UpdateEntry(kUrl2, params);
  EXPECT_EQ(ClientImpl::Result::kOK, client4->GetResult());
  EXPECT_TRUE(client4->GetParams().has_value());

  // ... but should not notify the waiting clients for |kUrl1|.
  EXPECT_EQ(ClientImpl::Result::kInitial, client2->GetResult());
}

TEST(WorkletModuleResponsesMapTest, InvalidURL) {
  WorkletModuleResponsesMap* map = new WorkletModuleResponsesMap;

  const KURL kEmptyURL;
  ASSERT_TRUE(kEmptyURL.IsEmpty());
  ClientImpl* client1 = new ClientImpl;
  map->ReadOrCreateEntry(kEmptyURL, client1);
  EXPECT_EQ(ClientImpl::Result::kFailed, client1->GetResult());
  EXPECT_FALSE(client1->GetParams().has_value());

  const KURL kNullURL = NullURL();
  ASSERT_TRUE(kNullURL.IsNull());
  ClientImpl* client2 = new ClientImpl;
  map->ReadOrCreateEntry(kNullURL, client2);
  EXPECT_EQ(ClientImpl::Result::kFailed, client2->GetResult());
  EXPECT_FALSE(client2->GetParams().has_value());

  const KURL kInvalidURL(kParsedURLString, String());
  ASSERT_FALSE(kInvalidURL.IsValid());
  ClientImpl* client3 = new ClientImpl;
  map->ReadOrCreateEntry(kInvalidURL, client3);
  EXPECT_EQ(ClientImpl::Result::kFailed, client3->GetResult());
  EXPECT_FALSE(client3->GetParams().has_value());
}

TEST(WorkletModuleResponsesMapTest, Dispose) {
  WorkletModuleResponsesMap* map = new WorkletModuleResponsesMap;
  const KURL kUrl1(kParsedURLString, "https://example.com/foo.js");
  const KURL kUrl2(kParsedURLString, "https://example.com/bar.js");

  // An initial read call for |kUrl1| creates a placeholder entry and asks the
  // client to fetch a module script.
  ClientImpl* client1 = new ClientImpl;
  map->ReadOrCreateEntry(kUrl1, client1);
  EXPECT_EQ(ClientImpl::Result::kNeedsFetching, client1->GetResult());
  EXPECT_FALSE(client1->GetParams().has_value());

  // The entry is now being fetched. Following read calls for |kUrl1| should
  // wait for the completion.
  ClientImpl* client2 = new ClientImpl;
  map->ReadOrCreateEntry(kUrl1, client2);
  EXPECT_EQ(ClientImpl::Result::kInitial, client2->GetResult());

  // An initial read call for |kUrl2| also creates a placeholder entry and asks
  // the client to fetch a module script.
  ClientImpl* client3 = new ClientImpl;
  map->ReadOrCreateEntry(kUrl2, client3);
  EXPECT_EQ(ClientImpl::Result::kNeedsFetching, client3->GetResult());
  EXPECT_FALSE(client3->GetParams().has_value());

  // The entry is now being fetched. Following read calls for |kUrl2| should
  // wait for the completion.
  ClientImpl* client4 = new ClientImpl;
  map->ReadOrCreateEntry(kUrl2, client4);
  EXPECT_EQ(ClientImpl::Result::kInitial, client4->GetResult());

  // Dispose() should notify to all waiting clients.
  map->Dispose();
  EXPECT_EQ(ClientImpl::Result::kFailed, client2->GetResult());
  EXPECT_FALSE(client2->GetParams().has_value());
  EXPECT_EQ(ClientImpl::Result::kFailed, client4->GetResult());
  EXPECT_FALSE(client4->GetParams().has_value());
}

}  // namespace blink
