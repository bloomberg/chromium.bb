// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_FAKE_SERVER_ENTITY_BUILDER_FACTORY_H_
#define SYNC_TEST_FAKE_SERVER_ENTITY_BUILDER_FACTORY_H_

#include <string>

#include "sync/test/fake_server/bookmark_entity_builder.h"
#include "url/gurl.h"

namespace fake_server {

// Creates various types of EntityBuilders.
//
// add comment of why this class exists
class EntityBuilderFactory {
 public:
  EntityBuilderFactory();
  explicit EntityBuilderFactory(const std::string& cache_guid);
  virtual ~EntityBuilderFactory();

  BookmarkEntityBuilder NewBookmarkEntityBuilder(const std::string& title,
                                                 const GURL& url);
 private:
  // An identifier used when creating entities. This value is used similarly to
  // the value in the Sync directory code.
  std::string cache_guid_;

  // The latest client item id assigned to an entity.
  int64 latest_client_item_id_;
};

}  // namespace fake_server

#endif  // SYNC_TEST_FAKE_SERVER_ENTITY_BUILDER_FACTORY_H_
