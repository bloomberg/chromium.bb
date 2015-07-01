// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/fake_server/bookmark_entity_builder.h"

#include <string>

#include "base/guid.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/base/unique_position.h"
#include "sync/protocol/sync.pb.h"
#include "sync/syncable/syncable_util.h"
#include "sync/test/fake_server/bookmark_entity.h"
#include "sync/test/fake_server/fake_server_entity.h"
#include "sync/util/time.h"
#include "url/gurl.h"

using std::string;

using syncer::syncable::GenerateSyncableBookmarkHash;

// A version must be passed when creating a FakeServerEntity, but this value
// is overrideen immediately when saving the entity in FakeServer.
const int64 kUnusedVersion = 0L;

// Default time (creation and last modified) used when creating entities.
const int64 kDefaultTime = 1234L;

namespace fake_server {

BookmarkEntityBuilder::BookmarkEntityBuilder(
    const string& title,
    const string& originator_cache_guid,
    const string& originator_client_item_id)
    : title_(title),
      originator_cache_guid_(originator_cache_guid),
      originator_client_item_id_(originator_client_item_id) {
}

BookmarkEntityBuilder::~BookmarkEntityBuilder() {
}

void BookmarkEntityBuilder::SetParentId(const std::string& parent_id) {
  parent_id_ = parent_id;
}

scoped_ptr<FakeServerEntity> BookmarkEntityBuilder::BuildBookmark(
    const GURL& url) {
  if (!url.is_valid()) {
    return make_scoped_ptr<FakeServerEntity>(NULL);
  }

  sync_pb::EntitySpecifics entity_specifics = CreateBaseEntitySpecifics();
  entity_specifics.mutable_bookmark()->set_url(url.spec());
  const bool kIsNotFolder = false;
  return Build(entity_specifics, kIsNotFolder);
}

scoped_ptr<FakeServerEntity> BookmarkEntityBuilder::BuildFolder() {
  const bool kIsFolder = true;
  return Build(CreateBaseEntitySpecifics(), kIsFolder);
}

sync_pb::EntitySpecifics BookmarkEntityBuilder::CreateBaseEntitySpecifics()
    const {
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::BookmarkSpecifics* bookmark_specifics =
      entity_specifics.mutable_bookmark();
  bookmark_specifics->set_title(title_);

  return entity_specifics;
}

scoped_ptr<FakeServerEntity> BookmarkEntityBuilder::Build(
    const sync_pb::EntitySpecifics& entity_specifics,
    bool is_folder) {
  sync_pb::UniquePosition unique_position;
  // TODO(pvalenzuela): Allow caller customization of the position integer.
  const string suffix = GenerateSyncableBookmarkHash(
      originator_cache_guid_,
      originator_client_item_id_);
  syncer::UniquePosition::FromInt64(0, suffix).ToProto(&unique_position);

  if (parent_id_.empty()) {
    parent_id_ = FakeServerEntity::CreateId(syncer::BOOKMARKS, "bookmark_bar");
  }

  const string id = FakeServerEntity::CreateId(syncer::BOOKMARKS,
                                               base::GenerateGUID());

  return make_scoped_ptr<FakeServerEntity>(
      new BookmarkEntity(id,
                         kUnusedVersion,
                         title_,
                         originator_cache_guid_,
                         originator_client_item_id_,
                         unique_position,
                         entity_specifics,
                         is_folder,
                         parent_id_,
                         kDefaultTime,
                         kDefaultTime));
}

}  // namespace fake_server
