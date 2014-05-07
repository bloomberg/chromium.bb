// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/fake_server/entity_builder_factory.h"

#include <string>

#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "sync/test/fake_server/bookmark_entity_builder.h"
#include "url/gurl.h"

using std::string;

namespace fake_server {

EntityBuilderFactory::EntityBuilderFactory()
    : cache_guid_(base::GenerateGUID()), latest_client_item_id_(0L)  {
}

EntityBuilderFactory::EntityBuilderFactory(const string& cache_guid)
    : cache_guid_(cache_guid), latest_client_item_id_(0L) {
}

EntityBuilderFactory::~EntityBuilderFactory() {
}

BookmarkEntityBuilder EntityBuilderFactory::NewBookmarkEntityBuilder(
    const string& title, const GURL& url) {
  --latest_client_item_id_;
  BookmarkEntityBuilder builder(title,
                                url,
                                cache_guid_,
                                base::Int64ToString(latest_client_item_id_));
  return builder;
}

}  // namespace fake_server
