// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "controller/OomInterventionImpl.h"

#include "core/exported/WebViewImpl.h"
#include "core/frame/FrameTestHelpers.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/page/Page.h"
#include "core/testing/DummyPageHolder.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/testing/UnitTestHelpers.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class MockOomInterventionHost : public mojom::blink::OomInterventionHost {
 public:
  MockOomInterventionHost(mojom::blink::OomInterventionHostRequest request)
      : binding_(this, std::move(request)) {}
  ~MockOomInterventionHost() = default;

  void OnHighMemoryUsage(bool intervention_triggered) override {}

 private:
  mojo::Binding<mojom::blink::OomInterventionHost> binding_;
};

}  // namespace

class OomInterventionImplTest : public ::testing::Test {
 public:
  size_t MockMemoryWorkloadCalculator() { return memory_workload_; }

 protected:
  size_t memory_workload_ = 0;
  FrameTestHelpers::WebViewHelper web_view_helper_;
};

TEST_F(OomInterventionImplTest, DetectedAndDeclined) {
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad("about::blank");
  Page* page = web_view->MainFrameImpl()->GetFrame()->GetPage();
  EXPECT_FALSE(page->Paused());

  auto intervention = std::make_unique<OomInterventionImpl>(
      WTF::BindRepeating(&OomInterventionImplTest::MockMemoryWorkloadCalculator,
                         WTF::Unretained(this)));
  EXPECT_FALSE(page->Paused());

  memory_workload_ = OomInterventionImpl::kMemoryWorkloadThreshold + 1;
  mojom::blink::OomInterventionHostPtr host_ptr;
  MockOomInterventionHost mock_host(mojo::MakeRequest(&host_ptr));
  intervention->StartDetection(std::move(host_ptr),
                               true /*trigger_intervention*/);
  testing::RunDelayedTasks(TimeDelta::FromSeconds(1));
  EXPECT_TRUE(page->Paused());

  intervention.reset();
  EXPECT_FALSE(page->Paused());
}

}  // namespace blink
