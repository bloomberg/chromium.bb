// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_INVALIDATION_ADAPTER_H_
#define COMPONENTS_SYNC_BASE_INVALIDATION_ADAPTER_H_

#include <stdint.h>

#include <string>

#include "components/invalidation/public/invalidation.h"
#include "components/sync/base/invalidation_interface.h"

namespace syncer {

// Wraps a Invalidation in the InvalidationInterface.
class InvalidationAdapter : public InvalidationInterface {
 public:
  explicit InvalidationAdapter(const Invalidation& invalidation);
  ~InvalidationAdapter() override;

  // Implementation of InvalidationInterface.
  bool IsUnknownVersion() const override;
  const std::string& GetPayload() const override;
  int64_t GetVersion() const override;
  void Acknowledge() override;
  void Drop() override;

 private:
  Invalidation invalidation_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_INVALIDATION_ADAPTER_H_
