// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/test/fake_model_type_connector.h"

#include "sync/internal_api/public/activation_context.h"

namespace syncer_v2 {

FakeModelTypeConnector::FakeModelTypeConnector() {}

FakeModelTypeConnector::~FakeModelTypeConnector() {}

void FakeModelTypeConnector::ConnectType(
    syncer::ModelType type,
    std::unique_ptr<ActivationContext> activation_context) {
  NOTREACHED() << "FakeModelTypeConnector is not meant to be used";
}

void FakeModelTypeConnector::DisconnectType(syncer::ModelType type) {
  NOTREACHED() << "FakeModelTypeConnector is not meant to be used";
}

}  // namespace syncer_v2
