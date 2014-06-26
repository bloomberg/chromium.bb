// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_FAKE_SERVER_FAKE_SERVER_ENTITY_H_
#define SYNC_TEST_FAKE_SERVER_FAKE_SERVER_ENTITY_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"

namespace fake_server {

// The representation of a Sync entity for the fake server.
class FakeServerEntity {
 public:
  // Creates an ID of the form <type><separator><inner-id> where
  // <type> is the EntitySpecifics field number for |model_type|, <separator>
  // is kIdSeparator, and <inner-id> is |inner_id|.
  //
  // If |inner_id| is globally unique, then the returned ID will also be
  // globally unique.
  static std::string CreateId(const syncer::ModelType& model_type,
                              const std::string& inner_id);

  // Returns the ID string of the top level node for the specified type.
  static std::string GetTopLevelId(const syncer::ModelType& model_type);

  virtual ~FakeServerEntity();
  const std::string& GetId() const;
  syncer::ModelType GetModelType() const;
  int64 GetVersion() const;
  void SetVersion(int64 version);
  const std::string& GetName() const;

  // Common data items needed by server
  virtual std::string GetParentId() const = 0;
  virtual sync_pb::SyncEntity* SerializeAsProto() = 0;
  virtual bool IsDeleted() const = 0;
  virtual bool IsFolder() const = 0;

 protected:
  // Extracts the ModelType from |id|. If |id| is malformed or does not contain
  // a valid ModelType, UNSPECIFIED is returned.
  static syncer::ModelType GetModelTypeFromId(const std::string& id);

  FakeServerEntity(const std::string& id,
                   const syncer::ModelType& model_type,
                   int64 version,
                   const std::string& name);

  void SerializeBaseProtoFields(sync_pb::SyncEntity* sync_entity);

  // The ModelType that categorizes this entity.
  syncer::ModelType model_type_;

 private:
  // The entity's ID.
  std::string id_;

  // The version of this entity.
  int64 version_;

  // The name of the entity.
  std::string name_;
};

}  // namespace fake_server

#endif  // SYNC_TEST_FAKE_SERVER_FAKE_SERVER_ENTITY_H_
