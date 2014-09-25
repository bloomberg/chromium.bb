// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_FAKE_SERVER_UNIQUE_CLIENT_ENTITY_H_
#define SYNC_TEST_FAKE_SERVER_UNIQUE_CLIENT_ENTITY_H_

#include <string>

#include "base/basictypes.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"
#include "sync/test/fake_server/fake_server_entity.h"

namespace fake_server {

// An entity that is unique per client account.
class UniqueClientEntity : public FakeServerEntity {
 public:
  virtual ~UniqueClientEntity();

  // Factory function for creating a UniqueClientEntity.
  static FakeServerEntity* Create(const sync_pb::SyncEntity& client_entity);

  // Derives an ID from a unique client tagged entity.
  static std::string EffectiveIdForClientTaggedEntity(
      const sync_pb::SyncEntity& entity);

  // FakeServerEntity implementation.
  virtual std::string GetParentId() const OVERRIDE;
  virtual void SerializeAsProto(sync_pb::SyncEntity* proto) OVERRIDE;
  virtual bool IsDeleted() const OVERRIDE;
  virtual bool IsFolder() const OVERRIDE;

 private:
  UniqueClientEntity(const std::string& id,
                     const syncer::ModelType& model_type,
                     int64 version,
                     const std::string& name,
                     const std::string& client_defined_unique_tag,
                     const sync_pb::EntitySpecifics& specifics,
                     int64 creation_time,
                     int64 last_modified_time);

  // These member values have equivalent fields in SyncEntity.
  std::string client_defined_unique_tag_;
  sync_pb::EntitySpecifics specifics_;
  int64 creation_time_;
  int64 last_modified_time_;
};

}  // namespace fake_server

#endif  // SYNC_TEST_FAKE_SERVER_UNIQUE_CLIENT_ENTITY_H_
