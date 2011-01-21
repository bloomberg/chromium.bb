// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppapi_unittest.h"

#include "ppapi/c/ppp_instance.h"
#include "webkit/plugins/ppapi/mock_plugin_delegate.h"
#include "webkit/plugins/ppapi/mock_resource.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_tracker.h"

namespace webkit {
namespace ppapi {

namespace {

class TrackedMockResource : public MockResource {
 public:
  static int tracked_objects_alive;

  TrackedMockResource(PluginInstance* instance) : MockResource(instance) {
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
  EXPECT_EQ(0u,
            tracker().GetLiveObjectsForInstance(instance()->pp_instance()));
  {
    scoped_refptr<TrackedMockResource> new_resource(
        new TrackedMockResource(instance()));
    ASSERT_EQ(1, TrackedMockResource::tracked_objects_alive);

    // Since we haven't gotten a PP_Resource, it's not associated with the
    // module.
    EXPECT_EQ(0u,
              tracker().GetLiveObjectsForInstance(instance()->pp_instance()));
  }
  ASSERT_EQ(0, TrackedMockResource::tracked_objects_alive);

  // Make a new resource and get it as a PP_Resource.
  PP_Resource resource_id = 0;
  {
    scoped_refptr<TrackedMockResource> new_resource(
        new TrackedMockResource(instance()));
    ASSERT_EQ(1, TrackedMockResource::tracked_objects_alive);
    resource_id = new_resource->GetReference();
    EXPECT_EQ(1u,
              tracker().GetLiveObjectsForInstance(instance()->pp_instance()));

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

TEST_F(ResourceTrackerTest, ForceDeleteWithInstance) {
  // Make a second instance (the test harness already creates & manages one).
  scoped_refptr<PluginInstance> instance2(
      new PluginInstance(delegate(), module(),
                         static_cast<const PPP_Instance*>(
                             GetMockInterface(PPP_INSTANCE_INTERFACE))));
  PP_Instance pp_instance2 = instance2->pp_instance();

  // Make two resources and take refs on behalf of the "plugin" for each.
  scoped_refptr<TrackedMockResource> resource1(
      new TrackedMockResource(instance2));
  resource1->GetReference();
  scoped_refptr<TrackedMockResource> resource2(
      new TrackedMockResource(instance2));
  resource2->GetReference();

  // Keep an "internal" ref to only the first (the PP_Resource also holds a
  // ref to each resource on behalf of the plugin).
  resource2 = NULL;

  ASSERT_EQ(2, TrackedMockResource::tracked_objects_alive);
  EXPECT_EQ(2u, tracker().GetLiveObjectsForInstance(pp_instance2));

  // Free the instance, this should release both plugin refs.
  instance2 = NULL;
  EXPECT_EQ(0u, tracker().GetLiveObjectsForInstance(pp_instance2));

  // The resource we have a scoped_refptr to should still be alive, but it
  // should have a NULL instance.
  ASSERT_FALSE(resource1->instance());
  ASSERT_EQ(1, TrackedMockResource::tracked_objects_alive);
  resource1 = NULL;
  ASSERT_EQ(0, TrackedMockResource::tracked_objects_alive);
}

}  // namespace ppapi
}  // namespace webkit
