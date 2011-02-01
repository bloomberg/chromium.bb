// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppapi_unittest.h"

#include "base/scoped_ptr.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppp_instance.h"
#include "third_party/npapi/bindings/npruntime.h"
#include "webkit/plugins/ppapi/mock_plugin_delegate.h"
#include "webkit/plugins/ppapi/mock_resource.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_tracker.h"
#include "webkit/plugins/ppapi/var.h"

namespace webkit {
namespace ppapi {

namespace {

// Tracked Resources -----------------------------------------------------------

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

// Tracked NPObjects -----------------------------------------------------------

int g_npobjects_alive = 0;

void TrackedClassDeallocate(NPObject* npobject) {
  g_npobjects_alive--;
}

NPClass g_tracked_npclass = {
  NP_CLASS_STRUCT_VERSION,
  NULL,
  &TrackedClassDeallocate,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
};

// Returns a new tracked NPObject with a refcount of 1.
NPObject* NewTrackedNPObject() {
  NPObject* object = new NPObject;
  object->_class = &g_tracked_npclass;
  object->referenceCount = 1;

  g_npobjects_alive++;
  return object;
}

}  // namespace

// ResourceTrackerTest ---------------------------------------------------------

class ResourceTrackerTest : public PpapiUnittest {
 public:
  ResourceTrackerTest() {
  }

  virtual void SetUp() {
    // The singleton override must be installed before the generic setup because
    // that creates an instance, etc. which uses the tracker.
    ResourceTracker::SetSingletonOverride(&tracker_);
    PpapiUnittest::SetUp();
  }
  virtual void TearDown() {
    // Must do normal tear down before clearing the override for the same rason
    // as the SetUp.
    PpapiUnittest::TearDown();
    ResourceTracker::ClearSingletonOverride();
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

TEST_F(ResourceTrackerTest, DeleteResourceWithInstance) {
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

TEST_F(ResourceTrackerTest, DeleteObjectVarWithInstance) {
  // Make a second instance (the test harness already creates & manages one).
  scoped_refptr<PluginInstance> instance2(
      new PluginInstance(delegate(), module(),
                         static_cast<const PPP_Instance*>(
                             GetMockInterface(PPP_INSTANCE_INTERFACE))));
  PP_Instance pp_instance2 = instance2->pp_instance();

  // Make an object var.
  scoped_ptr<NPObject> npobject(NewTrackedNPObject());
  PP_Var pp_object = ObjectVar::NPObjectToPPVar(instance2.get(),
                                                npobject.get());

  EXPECT_EQ(1, g_npobjects_alive);
  EXPECT_EQ(1u, tracker().GetLiveObjectsForInstance(pp_instance2));

  // Free the instance, this should release the ObjectVar.
  instance2 = NULL;
  EXPECT_EQ(0u, tracker().GetLiveObjectsForInstance(pp_instance2));
}

// Make sure that using the same NPObject should give the same PP_Var
// each time.
TEST_F(ResourceTrackerTest, ReuseVar) {
  scoped_ptr<NPObject> npobject(NewTrackedNPObject());

  PP_Var pp_object1 = ObjectVar::NPObjectToPPVar(instance(), npobject.get());
  PP_Var pp_object2 = ObjectVar::NPObjectToPPVar(instance(), npobject.get());

  // The two results should be the same.
  EXPECT_EQ(pp_object1.value.as_id, pp_object2.value.as_id);

  // The objects should be able to get us back to the associated NPObject.
  // This ObjectVar must be released before we do NPObjectToPPVar again
  // below so it gets freed and we get a new identifier.
  {
    scoped_refptr<ObjectVar> check_object(ObjectVar::FromPPVar(pp_object1));
    ASSERT_TRUE(check_object.get());
    EXPECT_EQ(instance(), check_object->instance());
    EXPECT_EQ(npobject.get(), check_object->np_object());
  }

  // Remove both of the refs we made above.
  tracker().UnrefVar(static_cast<int32_t>(pp_object2.value.as_id));
  tracker().UnrefVar(static_cast<int32_t>(pp_object1.value.as_id));

  // Releasing the resource should free the internal ref, and so making a new
  // one now should generate a new ID.
  PP_Var pp_object3 = ObjectVar::NPObjectToPPVar(instance(), npobject.get());
  EXPECT_NE(pp_object1.value.as_id, pp_object3.value.as_id);
  tracker().UnrefVar(static_cast<int32_t>(pp_object3.value.as_id));
}

}  // namespace ppapi
}  // namespace webkit
