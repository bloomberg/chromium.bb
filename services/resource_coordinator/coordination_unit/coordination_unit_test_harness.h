// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_TEST_HARNESS_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_TEST_HARNESS_H_

#include <stdint.h>

#include <memory>

#include "base/message_loop/message_loop.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_base.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_manager.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_provider_impl.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

struct CoordinationUnitID;

class TestCoordinationUnitWrapper {
 public:
  TestCoordinationUnitWrapper(CoordinationUnitBase* impl) : impl_(impl) {
    DCHECK(impl);
  }
  ~TestCoordinationUnitWrapper() { impl_->Destruct(); }

  CoordinationUnitBase* operator->() const { return impl_; }

  TestCoordinationUnitWrapper(TestCoordinationUnitWrapper&& other)
      : impl_(other.impl_) {}

  CoordinationUnitBase* get() const { return impl_; }

 private:
  CoordinationUnitBase* impl_;

  DISALLOW_COPY_AND_ASSIGN(TestCoordinationUnitWrapper);
};

class CoordinationUnitTestHarness : public testing::Test {
 public:
  CoordinationUnitTestHarness();
  ~CoordinationUnitTestHarness() override;

  // testing::Test:
  void TearDown() override;

  TestCoordinationUnitWrapper CreateCoordinationUnit(CoordinationUnitID cu_id);
  TestCoordinationUnitWrapper CreateCoordinationUnit(CoordinationUnitType type);

 protected:
  service_manager::ServiceContextRefFactory* service_context_ref_factory() {
    return &service_ref_factory_;
  }
  CoordinationUnitManager& coordination_unit_manager() {
    return coordination_unit_manager_;
  }
  CoordinationUnitProviderImpl* provider() { return &provider_; }

 private:
  base::MessageLoop message_loop_;
  service_manager::ServiceContextRefFactory service_ref_factory_;
  CoordinationUnitManager coordination_unit_manager_;
  CoordinationUnitProviderImpl provider_;
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_TEST_HARNESS_H_
