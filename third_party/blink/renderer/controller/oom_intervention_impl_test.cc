// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/oom_intervention_impl.h"

#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

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

class OomInterventionImplTest : public testing::Test {
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
  test::RunDelayedTasks(TimeDelta::FromSeconds(1));
  EXPECT_TRUE(page->Paused());

  intervention.reset();
  EXPECT_FALSE(page->Paused());
}

}  // namespace blink
