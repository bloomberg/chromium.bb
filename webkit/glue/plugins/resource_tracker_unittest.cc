// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/ppapi_unittest.h"

#include "webkit/glue/plugins/pepper_resource_tracker.h"
#include "webkit/glue/plugins/mock_resource.h"

namespace pepper {

namespace {

class TrackedMockResource : public MockResource {
 public:
  static int tracked_objects_alive;

  TrackedMockResource(PluginModule* module) : MockResource(module) {
    tracked_objects_alive++;
  }
  ~TrackedMockResource() {
    tracked_objects_alive--;
  }
};

int TrackedMockResource::tracked_objects_alive = 0;

}  // namespace

class ResourceTrackerTest : public PpapiUnittest {
 public:
  ResourceTrackerTest() {
  }

  virtual void SetUp() {
    PpapiUnittest::SetUp();
    ResourceTracker::SetSingletonOverride(&tracker_);
  }
  virtual void TearDown() {
    ResourceTracker::ClearSingletonOverride();
    PpapiUnittest::TearDown();
  }

  ResourceTracker& tracker() { return tracker_; }

 private:
  ResourceTracker tracker_;
};

TEST_F(ResourceTrackerTest, Ref) {
  ASSERT_EQ(0, TrackedMockResource::tracked_objects_alive);
  EXPECT_EQ(0u, tracker().GetLiveObjectsForModule(module()));
  {
    scoped_refptr<TrackedMockResource> new_resource(
        new TrackedMockResource(module()));
    ASSERT_EQ(1, TrackedMockResource::tracked_objects_alive);

    // Since we haven't gotten a PP_Resource, it's not associated with the
    // module.
    EXPECT_EQ(0u, tracker().GetLiveObjectsForModule(module()));
  }
  ASSERT_EQ(0, TrackedMockResource::tracked_objects_alive);

  // Make a new resource and get it as a PP_Resource.
  PP_Resource resource_id = 0;
  {
    scoped_refptr<TrackedMockResource> new_resource(
        new TrackedMockResource(module()));
    ASSERT_EQ(1, TrackedMockResource::tracked_objects_alive);
    resource_id = new_resource->GetReference();
    EXPECT_EQ(1u, tracker().GetLiveObjectsForModule(module()));

    // Resource IDs should be consistent.
    PP_Resource resource_id_2 = new_resource->GetReference();
    ASSERT_EQ(resource_id, resource_id_2);
  }

  // This time it should not have been deleted since the PP_Resource carries
  // a ref.
  ASSERT_EQ(1, TrackedMockResource::tracked_objects_alive);

  // Now we have two refs, derefing twice should delete the object.
  tracker().UnrefResource(resource_id);
  ASSERT_EQ(1, TrackedMockResource::tracked_objects_alive);
  tracker().UnrefResource(resource_id);
  ASSERT_EQ(0, TrackedMockResource::tracked_objects_alive);
}

TEST_F(ResourceTrackerTest, ForceDelete) {
  // Make two resources.
  scoped_refptr<TrackedMockResource> resource1(
      new TrackedMockResource(module()));
  PP_Resource pp_resource1 = resource1->GetReference();
  scoped_refptr<TrackedMockResource> resource2(
      new TrackedMockResource(module()));
  PP_Resource pp_resource2 = resource2->GetReference();

  // Keep an "internal" ref to only the first (the PP_Resource also holds a
  // ref to each resource on behalf of the plugin).
  resource2 = NULL;

  ASSERT_EQ(2, TrackedMockResource::tracked_objects_alive);
  EXPECT_EQ(2u, tracker().GetLiveObjectsForModule(module()));

  // Force delete both refs.
  tracker().ForceDeletePluginResourceRefs(pp_resource1);
  tracker().ForceDeletePluginResourceRefs(pp_resource2);
  EXPECT_EQ(0u, tracker().GetLiveObjectsForModule(module()));

  // The resource we have a scoped_refptr to should still be alive.
  ASSERT_EQ(1, TrackedMockResource::tracked_objects_alive);
  resource1 = NULL;
  ASSERT_EQ(0, TrackedMockResource::tracked_objects_alive);
}

}  // namespace pepper
