// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/fake_server/entity_builder.h"

#include <string>

#include "base/guid.h"
#include "sync/internal_api/public/base/model_type.h"

using std::string;

using syncer::ModelType;

namespace fake_server {

EntityBuilder::EntityBuilder(ModelType model_type, const string& name)
    : id_(FakeServerEntity::CreateId(model_type, base::GenerateGUID())),
      model_type_(model_type),
      name_(name) {
}

EntityBuilder::~EntityBuilder() {
}

}  // namespace fake_server
