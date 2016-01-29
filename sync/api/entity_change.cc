// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/entity_change.h"

namespace syncer_v2 {

// static
EntityChange EntityChange::CreateAdd(std::string client_tag,
                                     EntityDataPtr data) {
  return EntityChange(client_tag, ACTION_ADD, data);
}

// static
EntityChange EntityChange::CreateUpdate(std::string client_tag,
                                        EntityDataPtr data) {
  return EntityChange(client_tag, ACTION_UPDATE, data);
}

// static
EntityChange EntityChange::CreateDelete(std::string client_tag) {
  return EntityChange(client_tag, ACTION_DELETE, EntityDataPtr());
}

EntityChange::EntityChange(std::string client_tag,
                           ChangeType type,
                           EntityDataPtr data)
    : client_tag_(client_tag), type_(type), data_(data) {}

EntityChange::~EntityChange() {}

}  // namespace syncer_v2
