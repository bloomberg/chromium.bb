// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_ENTITY_CHANGE_H_
#define SYNC_API_ENTITY_CHANGE_H_

#include <string>
#include <vector>

#include "sync/api/entity_data.h"
#include "sync/base/sync_export.h"

namespace syncer_v2 {

class SYNC_EXPORT EntityChange {
 public:
  enum ChangeType {
    ACTION_ADD,
    ACTION_UPDATE,
    ACTION_DELETE
  };

  static EntityChange CreateAdd(std::string client_key, EntityDataPtr data);
  static EntityChange CreateUpdate(std::string client_key, EntityDataPtr data);
  static EntityChange CreateDelete(std::string client_key);

  virtual ~EntityChange();

  std::string client_key() const { return client_key_; }
  ChangeType type() const { return type_; }
  const EntityData& data() const { return data_.value(); }

 private:
  EntityChange(std::string client_key, ChangeType type, EntityDataPtr data);

  const std::string client_key_;
  const ChangeType type_;
  const EntityDataPtr data_;
};

typedef std::vector<EntityChange> EntityChangeList;

}  // namespace syncer_v2

#endif  // SYNC_API_ENTITY_CHANGE_H_
