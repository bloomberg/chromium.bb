// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_specifics_conversions.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/sync/protocol/sync.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::Eq;
using testing::NotNull;

namespace sync_bookmarks {

namespace {

TEST(BookmarkSpecificsConversionsTest, ShouldCreateSpecificsFromBookmarkNode) {
  const GURL kUrl("http://www.url.com");
  const std::string kTitle = "Title";
  const base::Time kTime = base::Time::Now();
  const std::string kKey1 = "key1";
  const std::string kValue1 = "value1";
  const std::string kKey2 = "key2";
  const std::string kValue2 = "value2";

  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();

  const bookmarks::BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node = model->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL(kUrl));
  ASSERT_THAT(node, NotNull());
  model->SetDateAdded(node, kTime);
  model->SetNodeMetaInfo(node, kKey1, kValue1);
  model->SetNodeMetaInfo(node, kKey2, kValue2);

  sync_pb::EntitySpecifics specifics =
      CreateSpecificsFromBookmarkNode(node, model.get());
  const sync_pb::BookmarkSpecifics& bm_specifics = specifics.bookmark();
  EXPECT_THAT(bm_specifics.title(), Eq(kTitle));
  EXPECT_THAT(GURL(bm_specifics.url()), Eq(kUrl));
  EXPECT_THAT(
      base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromMicroseconds(bm_specifics.creation_time_us())),
      Eq(kTime));
  for (const sync_pb::MetaInfo& meta_info : bm_specifics.meta_info()) {
    std::string value;
    node->GetMetaInfo(meta_info.key(), &value);
    EXPECT_THAT(meta_info.value(), Eq(value));
  }
}

TEST(BookmarkSpecificsConversionsTest, ShouldCreateBookmarkNodeFromSpecifics) {
  const GURL kUrl("http://www.url.com");
  const std::string kTitle = "Title";
  const base::Time kTime = base::Time::Now();
  const GURL kIconUrl("http://www.icon-url.com");
  const std::string kKey1 = "key1";
  const std::string kValue1 = "value1";
  const std::string kKey2 = "key2";
  const std::string kValue2 = "value2";

  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();
  bm_specifics->set_url(kUrl.spec());
  bm_specifics->set_icon_url(kIconUrl.spec());
  bm_specifics->set_favicon("PNG");
  bm_specifics->set_title(kTitle);
  bm_specifics->set_creation_time_us(
      kTime.ToDeltaSinceWindowsEpoch().InMicroseconds());
  sync_pb::MetaInfo* meta_info1 = bm_specifics->add_meta_info();
  meta_info1->set_key(kKey1);
  meta_info1->set_value(kValue1);

  sync_pb::MetaInfo* meta_info2 = bm_specifics->add_meta_info();
  meta_info2->set_key(kKey2);
  meta_info2->set_value(kValue2);

  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();
  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  EXPECT_CALL(favicon_service,
              AddPageNoVisitForBookmark(kUrl, base::UTF8ToUTF16(kTitle)));
  EXPECT_CALL(favicon_service, MergeFavicon(kUrl, kIconUrl, _, _, _));
  const bookmarks::BookmarkNode* node = CreateBookmarkNodeFromSpecifics(
      *bm_specifics,
      /*parent=*/model->bookmark_bar_node(), /*index=*/0,
      /*is_folder=*/false, model.get(), &favicon_service);
  ASSERT_THAT(node, NotNull());
  EXPECT_THAT(node->GetTitle(), Eq(base::UTF8ToUTF16(kTitle)));
  EXPECT_THAT(node->url(), Eq(kUrl));
  EXPECT_THAT(node->date_added(), Eq(kTime));
  std::string value1;
  node->GetMetaInfo(kKey1, &value1);
  EXPECT_THAT(value1, Eq(kValue1));
  std::string value2;
  node->GetMetaInfo(kKey2, &value2);
  EXPECT_THAT(value2, Eq(kValue2));
}

TEST(BookmarkSpecificsConversionsTest, ShouldUpdateBookmarkNodeFromSpecifics) {
  const GURL kUrl("http://www.url.com");
  const std::string kTitle = "Title";
  const base::Time kTime = base::Time::Now();
  const std::string kKey1 = "key1";
  const std::string kValue1 = "value1";
  const std::string kKey2 = "key2";
  const std::string kValue2 = "value2";

  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();

  const bookmarks::BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node = model->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL(kUrl));
  ASSERT_THAT(node, NotNull());
  model->SetNodeMetaInfo(node, kKey1, kValue1);
  model->SetNodeMetaInfo(node, kKey2, kValue2);

  const GURL kNewUrl("http://www.new-url.com");
  const std::string kNewTitle = "NewTitle";
  const GURL kNewIconUrl("http://www.new-icon-url.com");
  const std::string kNewValue1 = "new-value1";
  const std::string kNewValue2 = "new-value2";

  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();
  bm_specifics->set_url(kNewUrl.spec());
  bm_specifics->set_icon_url(kNewIconUrl.spec());
  bm_specifics->set_favicon("PNG");
  bm_specifics->set_title(kNewTitle);
  bm_specifics->set_creation_time_us(
      kTime.ToDeltaSinceWindowsEpoch().InMicroseconds());
  sync_pb::MetaInfo* meta_info1 = bm_specifics->add_meta_info();
  meta_info1->set_key(kKey1);
  meta_info1->set_value(kNewValue1);

  sync_pb::MetaInfo* meta_info2 = bm_specifics->add_meta_info();
  meta_info2->set_key(kKey2);
  meta_info2->set_value(kNewValue2);

  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  EXPECT_CALL(favicon_service,
              AddPageNoVisitForBookmark(kNewUrl, base::UTF8ToUTF16(kNewTitle)));
  EXPECT_CALL(favicon_service, MergeFavicon(kNewUrl, kNewIconUrl, _, _, _));
  UpdateBookmarkNodeFromSpecifics(*bm_specifics, node, model.get(),
                                  &favicon_service);
  EXPECT_THAT(node->GetTitle(), Eq(base::UTF8ToUTF16(kNewTitle)));
  EXPECT_THAT(node->url(), Eq(kNewUrl));
  std::string value1;
  node->GetMetaInfo(kKey1, &value1);
  EXPECT_THAT(value1, Eq(kNewValue1));
  std::string value2;
  node->GetMetaInfo(kKey2, &value2);
  EXPECT_THAT(value2, Eq(kNewValue2));
}

TEST(BookmarkSpecificsConversionsTest, ShouldBeValidBookmarkSpecifics) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();

  // URL is irrelevant for a folder.
  bm_specifics->set_url("INVALID_URL");
  EXPECT_TRUE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/true));

  bm_specifics->set_url("http://www.valid-url.com");
  EXPECT_TRUE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/false));
}

TEST(BookmarkSpecificsConversionsTest, ShouldBeInvalidBookmarkSpecifics) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();
  // Empty specifics.
  EXPECT_FALSE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/false));
  EXPECT_FALSE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/true));

  // Add invalid url.
  bm_specifics->set_url("INVALID_URL");
  EXPECT_FALSE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/false));

  // Add a valid url.
  bm_specifics->set_url("http://www.valid-url.com");
  // Add redudant keys in meta_info.
  sync_pb::MetaInfo* meta_info1 = bm_specifics->add_meta_info();
  meta_info1->set_key("key");
  meta_info1->set_value("value1");

  sync_pb::MetaInfo* meta_info2 = bm_specifics->add_meta_info();
  meta_info2->set_key("key");
  meta_info2->set_value("value2");
  EXPECT_FALSE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/false));
  EXPECT_FALSE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/true));
}

}  // namespace

}  // namespace sync_bookmarks
