// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "services/tracing/public/mojom/tracing.mojom.h"

namespace tracing {

class TracingServiceTest : public service_manager::test::ServiceTest {
 public:
  TracingServiceTest()
      : service_manager::test::ServiceTest("tracing_unittests") {}
  ~TracingServiceTest() override {}

 protected:
  void SetUp() override {
    service_manager::test::ServiceTest::SetUp();
    connector()->StartService(mojom::kServiceName);
  }

  void SetRunLoopToQuit(base::RunLoop* loop) { loop_ = loop; }

 private:
  base::RunLoop* loop_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TracingServiceTest);
};

TEST_F(TracingServiceTest, TracingServiceInstantiate) {
  mojom::AgentRegistryPtr agent_registry;
  connector()->BindInterface(mojom::kServiceName,
                             mojo::MakeRequest(&agent_registry));

  tracing::mojom::AgentPtr agent;
  agent_registry->RegisterAgent(std::move(agent), "FOO",
                                mojom::TraceDataType::STRING,
                                false /*supports_explicit_clock_sync*/);

  base::RunLoop loop;
  SetRunLoopToQuit(&loop);
  loop.Run();
}

}  // namespace tracing
