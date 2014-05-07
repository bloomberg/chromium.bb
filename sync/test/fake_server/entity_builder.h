// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_FAKE_SERVER_ENTITY_BUILDER_H_
#define SYNC_TEST_FAKE_SERVER_ENTITY_BUILDER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/test/fake_server/fake_server_entity.h"
#include "url/gurl.h"

namespace fake_server {

// Parent class for FakeServerEntity builders.
class EntityBuilder {
 public:
  virtual ~EntityBuilder();

  // Builds a FakeServerEntity and returns a scoped_ptr tht owns it. If building
  // was not successful, the returned scoped_ptr will not own a
  // FakeServerEntity*.
  virtual scoped_ptr<FakeServerEntity> Build() = 0;

 protected:
  EntityBuilder(syncer::ModelType model_type, const std::string& name);

  std::string id_;
  syncer::ModelType model_type_;
  std::string name_;
};

}  // namespace fake_server

#endif  // SYNC_TEST_FAKE_SERVER_ENTITY_BUILDER_H_
