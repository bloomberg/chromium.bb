// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/surfaces/display_compositor.h"

#include <inttypes.h>

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "cc/ipc/display_compositor.mojom.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_observer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/ui/common/task_runner_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace ui {
namespace test {
namespace {

std::string SurfaceIdString(const cc::SurfaceId& surface_id) {
  return base::StringPrintf("%u:%u:%u", surface_id.frame_sink_id().client_id(),
                            surface_id.frame_sink_id().sink_id(),
                            surface_id.local_frame_id().local_id());
}

cc::SurfaceId MakeSurfaceId(uint32_t client_id,
                            uint32_t sink_id,
                            uint32_t local_id) {
  return cc::SurfaceId(
      cc::FrameSinkId(client_id, sink_id),
      cc::LocalFrameId(local_id, base::UnguessableToken::Deserialize(0, 1u)));
}

// Test mojom::DisplayCompositorClient that records OnSurfaceCreated() events.
class TestDisplayCompositorClient : public cc::mojom::DisplayCompositorClient {
 public:
  TestDisplayCompositorClient() : binding_(this) {}
  ~TestDisplayCompositorClient() override {}

  cc::mojom::DisplayCompositorClientPtr GetPtr() {
    return binding_.CreateInterfacePtrAndBind();
  }

  // Returns events that have occurred and clear.
  std::string events() {
    std::string value = std::move(events_);
    events_.clear();
    return value;
  }

 private:
  void AddEvent(const std::string& text) {
    if (!events_.empty())
      events_ += ";";
    events_ += text;
  }

  // cc::mojom::DisplayCompositorClient:
  void OnSurfaceCreated(const cc::SurfaceId& surface_id,
                        const gfx::Size& frame_size,
                        float device_scale_factor) override {
    AddEvent(base::StringPrintf("OnSurfaceCreated(%s)",
                                SurfaceIdString(surface_id).c_str()));
  }

  mojo::Binding<cc::mojom::DisplayCompositorClient> binding_;
  std::string events_;

  DISALLOW_COPY_AND_ASSIGN(TestDisplayCompositorClient);
};

// Test SurfaceReferenceManager that records AddSurfaceReference() and
// RemoveSurfaceReference() events.
class TestSurfaceReferenceManager : public cc::SurfaceReferenceManager {
 public:
  ~TestSurfaceReferenceManager() override {}

  const cc::SurfaceId& GetRootSurfaceId() const override { return root_id_; }

  void AddSurfaceReference(const cc::SurfaceId& parent_id,
                           const cc::SurfaceId& child_id) override {
    AddEvent(base::StringPrintf("Add(%s-%s)",
                                SurfaceIdString(parent_id).c_str(),
                                SurfaceIdString(child_id).c_str()));
  }

  void RemoveSurfaceReference(const cc::SurfaceId& parent_id,
                              const cc::SurfaceId& child_id) override {
    AddEvent(base::StringPrintf("Remove(%s-%s)",
                                SurfaceIdString(parent_id).c_str(),
                                SurfaceIdString(child_id).c_str()));
  }

  size_t GetSurfaceReferenceCount(
      const cc::SurfaceId& surface_id) const override {
    NOTREACHED();
    return 0;
  }

  size_t GetReferencedSurfaceCount(
      const cc::SurfaceId& surface_id) const override {
    NOTREACHED();
    return 0;
  }

  // Returns events that have occurred and clear.
  std::string events() {
    std::string value = std::move(events_);
    events_.clear();
    return value;
  }

 private:
  void AddEvent(const std::string& text) {
    if (!events_.empty())
      events_ += ";";
    events_ += text;
  }

  const cc::SurfaceId root_id_ = MakeSurfaceId(0, 0, 0);
  std::string events_;
};

}  // namespace

class DisplayCompositorTest : public TaskRunnerTestBase {
 public:
  DisplayCompositorTest() {}
  ~DisplayCompositorTest() override {}

  cc::SurfaceObserver* surface_observer() { return display_compositor_.get(); }

  // Returns the total number of temporary references held by DisplayCompositor.
  size_t CountTempReferences() {
    size_t size = 0;
    for (auto& map_entry : display_compositor_->temp_references_) {
      size += map_entry.second.size();
    }
    return size;
  }

  // TaskRunnerTestBase:
  void SetUp() override {
    TaskRunnerTestBase::SetUp();
    display_compositor_ = base::MakeUnique<DisplayCompositor>(
        nullptr, nullptr, nullptr, nullptr, client_.GetPtr());
    display_compositor_->reference_manager_ = &reference_manager_;
  }

  void TearDown() override {
    // Clear any events before the next test.
    client_.events();
    reference_manager_.events();
  }

 protected:
  TestDisplayCompositorClient client_;
  TestSurfaceReferenceManager reference_manager_;
  std::unique_ptr<DisplayCompositor> display_compositor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayCompositorTest);
};

TEST_F(DisplayCompositorTest, AddSurfaceThenReference) {
  const cc::SurfaceId parent_id = MakeSurfaceId(1, 1, 1);
  const cc::SurfaceId surface_id = MakeSurfaceId(2, 1, 1);
  surface_observer()->OnSurfaceCreated(surface_id, gfx::Size(1, 1), 1.0f);
  RunUntilIdle();

  // Client should get OnSurfaceCreated call and temporary reference added.
  EXPECT_EQ("OnSurfaceCreated(2:1:1)", client_.events());
  EXPECT_EQ("Add(0:0:0-2:1:1)", reference_manager_.events());
  EXPECT_EQ(1u, CountTempReferences());

  display_compositor_->AddSurfaceReference(parent_id, surface_id);
  RunUntilIdle();

  // Real reference is added then temporary reference removed.
  EXPECT_EQ("Add(1:1:1-2:1:1);Remove(0:0:0-2:1:1)",
            reference_manager_.events());
  EXPECT_EQ(0u, CountTempReferences());
}

TEST_F(DisplayCompositorTest, AddSurfaceThenRootReference) {
  const cc::SurfaceId surface_id = MakeSurfaceId(1, 1, 1);
  surface_observer()->OnSurfaceCreated(surface_id, gfx::Size(1, 1), 1.0f);
  RunUntilIdle();

  // Temporary reference should be added.
  EXPECT_EQ("Add(0:0:0-1:1:1)", reference_manager_.events());
  EXPECT_EQ(1u, CountTempReferences());

  display_compositor_->AddRootSurfaceReference(surface_id);
  RunUntilIdle();

  // Adding real reference doesn't need to change anything in
  // SurfaceReferenceManager does remove the temporary reference marker.
  EXPECT_EQ("", reference_manager_.events());
  EXPECT_EQ(0u, CountTempReferences());
}

TEST_F(DisplayCompositorTest, AddTwoSurfacesThenOneReference) {
  const cc::SurfaceId parent_id = MakeSurfaceId(1, 1, 1);
  const cc::SurfaceId surface_id1 = MakeSurfaceId(2, 1, 1);
  const cc::SurfaceId surface_id2 = MakeSurfaceId(3, 1, 1);

  // Add two surfaces with different FrameSinkIds.
  surface_observer()->OnSurfaceCreated(surface_id1, gfx::Size(1, 1), 1.0f);
  surface_observer()->OnSurfaceCreated(surface_id2, gfx::Size(1, 1), 1.0f);
  RunUntilIdle();

  // Temporary reference should be added for both surfaces.
  EXPECT_EQ("Add(0:0:0-2:1:1);Add(0:0:0-3:1:1)", reference_manager_.events());
  EXPECT_EQ(2u, CountTempReferences());

  display_compositor_->AddSurfaceReference(parent_id, surface_id1);
  RunUntilIdle();

  // Real reference is added then temporary reference removed for 2:1:1. There
  // should still be a temporary reference left to 3:1:1
  EXPECT_EQ("Add(1:1:1-2:1:1);Remove(0:0:0-2:1:1)",
            reference_manager_.events());
  EXPECT_EQ(1u, CountTempReferences());
}

TEST_F(DisplayCompositorTest, AddSurfacesSkipReference) {
  const cc::SurfaceId parent_id = MakeSurfaceId(1, 1, 1);
  const cc::SurfaceId surface_id1 = MakeSurfaceId(2, 1, 1);
  const cc::SurfaceId surface_id2 = MakeSurfaceId(2, 1, 2);

  // Add two surfaces that have the same FrameSinkId. This would happen when a
  // client submits two CFs before parent submits a new CF.
  surface_observer()->OnSurfaceCreated(surface_id1, gfx::Size(1, 1), 1.0f);
  surface_observer()->OnSurfaceCreated(surface_id2, gfx::Size(1, 1), 1.0f);
  RunUntilIdle();

  // Client should get OnSurfaceCreated call and temporary reference added for
  // both surfaces.
  EXPECT_EQ("OnSurfaceCreated(2:1:1);OnSurfaceCreated(2:1:2)",
            client_.events());
  EXPECT_EQ("Add(0:0:0-2:1:1);Add(0:0:0-2:1:2)", reference_manager_.events());
  EXPECT_EQ(2u, CountTempReferences());

  // Add a reference to the surface with the later LocalFrameId.
  display_compositor_->AddSurfaceReference(parent_id, surface_id2);
  RunUntilIdle();

  // The real reference should be added for 2:1:2 and both temporary references
  // should be removed.
  EXPECT_EQ("Add(1:1:1-2:1:2);Remove(0:0:0-2:1:2);Remove(0:0:0-2:1:1)",
            reference_manager_.events());
  EXPECT_EQ(0u, CountTempReferences());
}

}  // namespace test
}  // namespace ui
