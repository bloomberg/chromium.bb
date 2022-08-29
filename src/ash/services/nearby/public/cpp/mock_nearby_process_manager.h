// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_NEARBY_PUBLIC_CPP_MOCK_NEARBY_PROCESS_MANAGER_H_
#define ASH_SERVICES_NEARBY_PUBLIC_CPP_MOCK_NEARBY_PROCESS_MANAGER_H_

#include "ash/services/nearby/public/cpp/nearby_process_manager.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace nearby {

class MockNearbyProcessManager : public NearbyProcessManager {
 public:
  class MockNearbyProcessReference : public NearbyProcessReference {
   public:
    MockNearbyProcessReference();
    MockNearbyProcessReference(const MockNearbyProcessReference&) = delete;
    MockNearbyProcessReference& operator=(const MockNearbyProcessReference&) =
        delete;
    ~MockNearbyProcessReference() override;

    MOCK_METHOD(const mojo::SharedRemote<
                    location::nearby::connections::mojom::NearbyConnections>&,
                GetNearbyConnections,
                (),
                (const, override));

    MOCK_METHOD(const mojo::SharedRemote<sharing::mojom::NearbySharingDecoder>&,
                GetNearbySharingDecoder,
                (),
                (const, override));
  };

  MockNearbyProcessManager();
  MockNearbyProcessManager(const MockNearbyProcessManager&) = delete;
  MockNearbyProcessManager& operator=(const MockNearbyProcessManager&) = delete;
  ~MockNearbyProcessManager() override;

  MOCK_METHOD(std::unique_ptr<NearbyProcessReference>,
              GetNearbyProcessReference,
              (NearbyProcessStoppedCallback on_process_stopped_callback),
              (override));
};

}  // namespace nearby
}  // namespace ash

#endif  // ASH_SERVICES_NEARBY_PUBLIC_CPP_MOCK_NEARBY_PROCESS_MANAGER_H_
