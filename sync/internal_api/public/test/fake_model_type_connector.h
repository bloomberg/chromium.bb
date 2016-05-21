// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_MODEL_TYPE_CONNECTOR_H_
#define SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_MODEL_TYPE_CONNECTOR_H_

#include "sync/internal_api/public/model_type_connector.h"

namespace syncer_v2 {

// A no-op implementation of ModelTypeConnector for testing.
class FakeModelTypeConnector : public ModelTypeConnector {
 public:
  FakeModelTypeConnector();
  ~FakeModelTypeConnector() override;

  void ConnectType(
      syncer::ModelType type,
      std::unique_ptr<ActivationContext> activation_context) override;
  void DisconnectType(syncer::ModelType type) override;
};

}  // namespace syncer_v2

#endif  // SYNC_INTERNAL_API_PUBLIC_TEST_FAKE_MODEL_TYPE_CONNECTOR_H_
