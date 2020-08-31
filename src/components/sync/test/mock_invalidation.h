// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_MOCK_INVALIDATION_H_
#define COMPONENTS_SYNC_TEST_MOCK_INVALIDATION_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "components/sync/base/invalidation_interface.h"

namespace syncer {

// An InvalidationInterface used by sync for testing.
// It does not support any form of acknowledgements.
class MockInvalidation : public InvalidationInterface {
 public:
  // Helpers to build new MockInvalidations.
  static std::unique_ptr<MockInvalidation> BuildUnknownVersion();
  static std::unique_ptr<MockInvalidation> Build(int64_t version,
                                                 const std::string& payload);

  ~MockInvalidation() override;

  // Implementation of InvalidationInterface.
  bool IsUnknownVersion() const override;
  const std::string& GetPayload() const override;
  int64_t GetVersion() const override;
  void Acknowledge() override;
  void Drop() override;

 protected:
  MockInvalidation(bool is_unknown_version,
                   int64_t version,
                   const std::string& payload);

  // Whether or not this is an 'unknown version' invalidation.
  const bool is_unknown_version_;

  // The version of this invalidation.  Valid only if !is_unknown_version_.
  const int64_t version_;

  // The payload of this invalidation.  Valid only if !is_unknown_version_.
  const std::string payload_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_MOCK_INVALIDATION_H_
