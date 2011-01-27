// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_test_sink.h"
#include "ppapi/proxy/plugin_var_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppapi_proxy_test.h"

namespace pp {
namespace proxy {

namespace {

PP_Var MakeObject(PluginVarTracker::VarID object_id) {
  PP_Var ret;
  ret.type = PP_VARTYPE_OBJECT;
  ret.value.as_id = object_id;
  return ret;
}

// Creates a PP_Var from the given string ID.
PP_Var MakeString(PluginVarTracker::VarID string_id) {
  PP_Var ret;
  ret.type = PP_VARTYPE_STRING;
  ret.value.as_id = string_id;
  return ret;
}

}  // namespace

class PluginVarTrackerTest : public PluginProxyTest {
 public:
  PluginVarTrackerTest() {}

 protected:
  // Asserts that there is a unique "release object" IPC message in the test
  // sink. This will return the var ID from the message or -1 if none found.
  PluginVarTracker::VarID GetObjectIDForUniqueReleaseObject() {
    const IPC::Message* release_msg = sink().GetUniqueMessageMatching(
        PpapiHostMsg_PPBVar_ReleaseObject::ID);
    if (!release_msg)
      return -1;

    Tuple1<int64> id;
    PpapiHostMsg_PPBVar_ReleaseObject::Read(release_msg, &id);
    return id.a;
  }
};

TEST_F(PluginVarTrackerTest, Strings) {
  std::string str("Hello");
  PluginVarTracker::VarID str_id1 = var_tracker().MakeString(str);
  EXPECT_NE(0, str_id1);

  PluginVarTracker::VarID str_id2 = var_tracker().MakeString(
      str.c_str(), static_cast<uint32_t>(str.size()));
  EXPECT_NE(0, str_id2);

  // Make sure the strings come out the other end.
  std::string result = var_tracker().GetString(MakeString(str_id1));
  EXPECT_EQ(str, result);
  result = var_tracker().GetString(MakeString(str_id2));
  EXPECT_EQ(str, result);
}

TEST_F(PluginVarTrackerTest, GetHostObject) {
  PP_Var host_object = MakeObject(12345);

  // Round-trip through the tracker to make sure the host object comes out the
  // other end.
  PP_Var plugin_object = var_tracker().ReceiveObjectPassRef(host_object,
                                                            &sink());
  PP_Var host_object2 = var_tracker().GetHostObject(plugin_object);
  EXPECT_EQ(PP_VARTYPE_OBJECT, host_object2.type);
  EXPECT_EQ(host_object.value.as_id, host_object2.value.as_id);

  var_tracker().Release(plugin_object);
}

TEST_F(PluginVarTrackerTest, ReceiveObjectPassRef) {
  PP_Var host_object = MakeObject(12345);

  // Receive the object, we should have one ref and no messages.
  PP_Var plugin_object = var_tracker().ReceiveObjectPassRef(host_object,
                                                            &sink());
  EXPECT_EQ(0u, sink().message_count());
  EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_object));
  EXPECT_EQ(0,
      var_tracker().GetTrackedWithNoReferenceCountForObject(plugin_object));

  // Receive the same object again, we should get the same plugin ID out.
  PP_Var plugin_object2 = var_tracker().ReceiveObjectPassRef(host_object,
                                                             &sink());
  EXPECT_EQ(plugin_object.value.as_id, plugin_object2.value.as_id);
  EXPECT_EQ(2, var_tracker().GetRefCountForObject(plugin_object));
  EXPECT_EQ(0,
      var_tracker().GetTrackedWithNoReferenceCountForObject(plugin_object));

  // It should have sent one message to decerment the refcount in the host.
  // This is because it only maintains one host refcount for all references
  // in the plugin, but the host just sent the second one.
  EXPECT_EQ(host_object.value.as_id, GetObjectIDForUniqueReleaseObject());
  sink().ClearMessages();

  // Release the object, one ref at a time. The second release should free
  // the tracking data and send a release message to the browser.
  var_tracker().Release(plugin_object);
  EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_object));
  var_tracker().Release(plugin_object);
  EXPECT_EQ(-1, var_tracker().GetRefCountForObject(plugin_object));
  EXPECT_EQ(host_object.value.as_id, GetObjectIDForUniqueReleaseObject());
}

// Tests freeing objects that have both refcounts and "tracked with no ref".
TEST_F(PluginVarTrackerTest, FreeTrackedAndReferencedObject) {
  PP_Var host_object = MakeObject(12345);

  // Phase one: First receive via a "pass ref", then a tracked with no ref.
  PP_Var plugin_var = var_tracker().ReceiveObjectPassRef(host_object, &sink());
  PP_Var plugin_var2 =
      var_tracker().TrackObjectWithNoReference(host_object, &sink());
  EXPECT_EQ(plugin_var.value.as_id, plugin_var2.value.as_id);
  EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_var));
  EXPECT_EQ(1,
            var_tracker().GetTrackedWithNoReferenceCountForObject(plugin_var));

  // Free via the refcount, this should release the object to the browser but
  // maintain the tracked object.
  var_tracker().Release(plugin_var);
  EXPECT_EQ(0, var_tracker().GetRefCountForObject(plugin_var));
  EXPECT_EQ(1u, sink().message_count());
  EXPECT_EQ(host_object.value.as_id, GetObjectIDForUniqueReleaseObject());

  // Now free via the tracked object, this should free it.
  var_tracker().StopTrackingObjectWithNoReference(plugin_var);
  EXPECT_EQ(-1, var_tracker().GetRefCountForObject(plugin_var));

  // Phase two: Receive via a tracked, then get an addref.
  sink().ClearMessages();
  plugin_var = var_tracker().TrackObjectWithNoReference(host_object, &sink());
  plugin_var2 = var_tracker().ReceiveObjectPassRef(host_object, &sink());
  EXPECT_EQ(plugin_var.value.as_id, plugin_var2.value.as_id);
  EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_var));
  EXPECT_EQ(1,
            var_tracker().GetTrackedWithNoReferenceCountForObject(plugin_var));

  // Free via the tracked object, this should have no effect.
  var_tracker().StopTrackingObjectWithNoReference(plugin_var);
  EXPECT_EQ(0,
            var_tracker().GetTrackedWithNoReferenceCountForObject(plugin_var));
  EXPECT_EQ(0u, sink().message_count());

  // Now free via the refcount, this should delete it.
  var_tracker().Release(plugin_var);
  EXPECT_EQ(-1, var_tracker().GetRefCountForObject(plugin_var));
  EXPECT_EQ(host_object.value.as_id, GetObjectIDForUniqueReleaseObject());
}

TEST_F(PluginVarTrackerTest, RecursiveTrackWithNoRef) {
  PP_Var host_object = MakeObject(12345);

  // Receive a tracked object twice.
  PP_Var plugin_var = var_tracker().TrackObjectWithNoReference(
      host_object, &sink());
  EXPECT_EQ(1,
            var_tracker().GetTrackedWithNoReferenceCountForObject(plugin_var));
  PP_Var plugin_var2 = var_tracker().TrackObjectWithNoReference(host_object,
                                                                &sink());
  EXPECT_EQ(plugin_var.value.as_id, plugin_var2.value.as_id);
  EXPECT_EQ(0, var_tracker().GetRefCountForObject(plugin_var));
  EXPECT_EQ(2,
            var_tracker().GetTrackedWithNoReferenceCountForObject(plugin_var));

  // Now release those tracked items, the reference should be freed.
  var_tracker().StopTrackingObjectWithNoReference(plugin_var);
  EXPECT_EQ(1,
            var_tracker().GetTrackedWithNoReferenceCountForObject(plugin_var));
  var_tracker().StopTrackingObjectWithNoReference(plugin_var);
  EXPECT_EQ(-1,
            var_tracker().GetTrackedWithNoReferenceCountForObject(plugin_var));
}

}  // namespace proxy
}  // namespace pp
