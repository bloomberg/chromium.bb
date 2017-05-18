// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "services/resource_coordinator/memory/coordinator/process_map.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/interfaces/service_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_instrumentation {

using RunningServiceInfoPtr = service_manager::mojom::RunningServiceInfoPtr;
using ServiceManagerListenerRequest =
    service_manager::mojom::ServiceManagerListenerRequest;

class ProcessMapTest : public testing::Test {
 public:
  ProcessMapTest() : process_map_(nullptr) {}

 protected:
  ProcessMap process_map_;
};

TEST_F(ProcessMapTest, TypicalCase) {
  service_manager::Identity id1("id1");
  EXPECT_EQ(base::kNullProcessId, process_map_.GetProcessId(id1));
  process_map_.OnInit(std::vector<RunningServiceInfoPtr>());
  EXPECT_EQ(base::kNullProcessId, process_map_.GetProcessId(id1));

  RunningServiceInfoPtr info(service_manager::mojom::RunningServiceInfo::New());
  info->identity = id1;
  info->pid = 1;
  process_map_.OnServiceCreated(info.Clone());
  EXPECT_EQ(static_cast<base::ProcessId>(1), process_map_.GetProcessId(id1));

  // Adding a separate service with a different identity should have no effect.
  info->identity = service_manager::Identity("id2");
  info->pid = 2;
  process_map_.OnServiceCreated(info.Clone());
  EXPECT_EQ(static_cast<base::ProcessId>(1), process_map_.GetProcessId(id1));

  // Once the service is stopped, searching for its id should return a null pid.
  process_map_.OnServiceStopped(id1);
  EXPECT_EQ(base::kNullProcessId, process_map_.GetProcessId(id1));
}

TEST_F(ProcessMapTest, PresentInInit) {
  service_manager::Identity id1("id1");
  RunningServiceInfoPtr info(service_manager::mojom::RunningServiceInfo::New());
  info->identity = id1;
  info->pid = 1;

  std::vector<RunningServiceInfoPtr> v;
  v.push_back(std::move(info));
  process_map_.OnInit(std::move(v));
  EXPECT_EQ(static_cast<base::ProcessId>(1), process_map_.GetProcessId(id1));

  process_map_.OnServiceStopped(id1);
  EXPECT_EQ(base::kNullProcessId, process_map_.GetProcessId(id1));
}

}  // namespace memory_instrumentation
