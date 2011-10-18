// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppapi_unittest.h"

#include "base/memory/scoped_ptr.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppp_instance.h"
#include "third_party/npapi/bindings/npruntime.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "webkit/plugins/ppapi/mock_plugin_delegate.h"
#include "webkit/plugins/ppapi/mock_resource.h"
#include "webkit/plugins/ppapi/npapi_glue.h"
#include "webkit/plugins/ppapi/npobject_var.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_tracker.h"

using ppapi::NPObjectVar;

namespace webkit {
namespace ppapi {

namespace {

// Tracked NPObjects -----------------------------------------------------------

int g_npobjects_alive = 0;

void TrackedClassDeallocate(NPObject* npobject) {
  g_npobjects_alive--;
  delete npobject;
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

// Returns a new tracked NPObject with a refcount of 1. You'll want to put this
// in a NPObjectReleaser to free this ref when the test completes.
NPObject* NewTrackedNPObject() {
  NPObject* object = new NPObject;
  object->_class = &g_tracked_npclass;
  object->referenceCount = 1;

  g_npobjects_alive++;
  return object;
}

class ReleaseNPObject {
 public:
  void operator()(NPObject* o) const {
    WebKit::WebBindings::releaseObject(o);
  }
};

// Handles automatically releasing a reference to the NPObject on destruction.
// It's assumed the input has a ref already taken.
typedef scoped_ptr_malloc<NPObject, ReleaseNPObject> NPObjectReleaser;

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

TEST_F(ResourceTrackerTest, DeleteObjectVarWithInstance) {
  // Make a second instance (the test harness already creates & manages one).
  scoped_refptr<PluginInstance> instance2(
      PluginInstance::Create1_0(delegate(), module(),
                                GetMockInterface(PPP_INSTANCE_INTERFACE_1_0)));
  PP_Instance pp_instance2 = instance2->pp_instance();

  // Make an object var.
  NPObjectReleaser npobject(NewTrackedNPObject());
  NPObjectToPPVar(instance2.get(), npobject.get());

  EXPECT_EQ(1, g_npobjects_alive);
  EXPECT_EQ(1, tracker().GetLiveNPObjectVarsForInstance(pp_instance2));

  // Free the instance, this should release the ObjectVar.
  instance2 = NULL;
  EXPECT_EQ(0, tracker().GetLiveNPObjectVarsForInstance(pp_instance2));
}

// Make sure that using the same NPObject should give the same PP_Var
// each time.
TEST_F(ResourceTrackerTest, ReuseVar) {
  NPObjectReleaser npobject(NewTrackedNPObject());

  PP_Var pp_object1 = NPObjectToPPVar(instance(), npobject.get());
  PP_Var pp_object2 = NPObjectToPPVar(instance(), npobject.get());

  // The two results should be the same.
  EXPECT_EQ(pp_object1.value.as_id, pp_object2.value.as_id);

  // The objects should be able to get us back to the associated NPObject.
  // This ObjectVar must be released before we do NPObjectToPPVar again
  // below so it gets freed and we get a new identifier.
  {
    scoped_refptr<NPObjectVar> check_object(NPObjectVar::FromPPVar(pp_object1));
    ASSERT_TRUE(check_object.get());
    EXPECT_EQ(instance()->pp_instance(), check_object->pp_instance());
    EXPECT_EQ(npobject.get(), check_object->np_object());
  }

  // Remove both of the refs we made above.
  ::ppapi::VarTracker* var_tracker = tracker().GetVarTracker();
  var_tracker->ReleaseVar(static_cast<int32_t>(pp_object2.value.as_id));
  var_tracker->ReleaseVar(static_cast<int32_t>(pp_object1.value.as_id));

  // Releasing the resource should free the internal ref, and so making a new
  // one now should generate a new ID.
  PP_Var pp_object3 = NPObjectToPPVar(instance(), npobject.get());
  EXPECT_NE(pp_object1.value.as_id, pp_object3.value.as_id);
  var_tracker->ReleaseVar(static_cast<int32_t>(pp_object3.value.as_id));
}

}  // namespace ppapi
}  // namespace webkit
