// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_FAKE_SERVER_TOMBSTONE_ENTITY_H_
#define SYNC_TEST_FAKE_SERVER_TOMBSTONE_ENTITY_H_

#include <string>

#include "base/basictypes.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"
#include "sync/test/fake_server/fake_server_entity.h"

namespace fake_server {

// A Sync entity that represents a deleted item.
class TombstoneEntity : public FakeServerEntity {
 public:
  ~TombstoneEntity() override;

  // Factory function for TombstoneEntity.
  static scoped_ptr<FakeServerEntity> Create(const std::string& id);

  // FakeServerEntity implementation.
  bool RequiresParentId() const override;
  std::string GetParentId() const override;
  void SerializeAsProto(sync_pb::SyncEntity* proto) const override;
  bool IsDeleted() const override;

 private:
  TombstoneEntity(const std::string& id, const syncer::ModelType& model_type);
};

}  // namespace fake_server

#endif  // SYNC_TEST_FAKE_SERVER_TOMBSTONE_ENTITY_H_
