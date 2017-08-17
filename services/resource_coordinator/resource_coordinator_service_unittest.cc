// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit_provider.mojom.h"
#include "services/resource_coordinator/public/interfaces/service_constants.mojom.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_test.h"

namespace resource_coordinator {

class ResourceCoordinatorTest : public service_manager::test::ServiceTest {
 public:
  ResourceCoordinatorTest()
      : service_manager::test::ServiceTest("resource_coordinator_unittests") {}
  ~ResourceCoordinatorTest() override {}

  void GetIDCallback(const CoordinationUnitID& cu_id) {
    loop_->Quit();
  }

 protected:
  void SetUp() override {
    service_manager::test::ServiceTest::SetUp();
    connector()->StartService(mojom::kServiceName);
  }

  void SetRunLoopToQuit(base::RunLoop* loop) { loop_ = loop; }

 private:
  base::RunLoop* loop_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ResourceCoordinatorTest);
};

TEST_F(ResourceCoordinatorTest, ResourceCoordinatorInstantiate) {
  mojom::CoordinationUnitProviderPtr provider;
  connector()->BindInterface(mojom::kServiceName, mojo::MakeRequest(&provider));

  CoordinationUnitID new_id(CoordinationUnitType::kWebContents, "test_id");
  mojom::CoordinationUnitPtr coordination_unit;
  provider->CreateCoordinationUnit(mojo::MakeRequest(&coordination_unit),
                                   new_id);

  coordination_unit->GetID(base::Bind(&ResourceCoordinatorTest::GetIDCallback,
                                      base::Unretained(this)));

  base::RunLoop loop;
  SetRunLoopToQuit(&loop);
  loop.Run();
}

}  // namespace resource_coordinator
