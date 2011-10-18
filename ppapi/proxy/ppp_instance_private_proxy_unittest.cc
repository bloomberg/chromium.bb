// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/dev/ppp_class_deprecated.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/private/ppp_instance_private.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/ppapi_proxy_test.h"

namespace ppapi {
namespace proxy {

namespace {
const PP_Instance kInstance = 0xdeadbeef;

PP_Var MakeObjectVar(int64_t object_id) {
  PP_Var ret;
  ret.type = PP_VARTYPE_OBJECT;
  ret.value.as_id = object_id;
  return ret;
}

PluginDispatcher* plugin_dispatcher = NULL;
// Return the plugin-side proxy for PPB_Var_Deprecated.
const PPB_Var_Deprecated* plugin_var_deprecated_if() {
  // The test code must set the plugin dispatcher.
  CHECK(plugin_dispatcher);
  // Grab the plugin-side proxy for PPB_Var_Deprecated (for CreateObject).
  return static_cast<const PPB_Var_Deprecated*>(
      plugin_dispatcher->GetBrowserInterface(
          PPB_VAR_DEPRECATED_INTERFACE));
}

// Mock PPP_Instance_Private.
PP_Var instance_obj;
PP_Var GetInstanceObject(PP_Instance /*instance*/) {
  // The 1 ref we got from CreateObject will be passed to the host. We want to
  // have a ref of our own.
  plugin_var_deprecated_if()->AddRef(instance_obj);
  return instance_obj;
}

PPP_Instance_Private ppp_instance_private_mock = {
  &GetInstanceObject
};

// We need to mock PPP_Instance, so that we can create and destroy the pretend
// instance that PPP_Instance_Private uses.
PP_Bool DidCreate(PP_Instance /*instance*/, uint32_t /*argc*/,
                  const char* /*argn*/[], const char* /*argv*/[]) {
  // Create an object var. This should exercise the typical path for creating
  // instance objects.
  instance_obj =
      plugin_var_deprecated_if()->CreateObject(kInstance, NULL, NULL);
  return PP_TRUE;
}

void DidDestroy(PP_Instance /*instance*/) {
  // Decrement the reference count for our instance object. It should be
  // deleted.
  plugin_var_deprecated_if()->Release(instance_obj);
}

PPP_Instance_1_0 ppp_instance_mock = { &DidCreate, &DidDestroy };

}  // namespace

// Mock PPB_Var_Deprecated, so that we can emulate creating an Object Var.
std::map<int64_t, int> id_refcount_map;
void AddRefVar(PP_Var var) {
  CHECK(var.type >= PP_VARTYPE_STRING);  // Must be a ref-counted type.
  CHECK(id_refcount_map.find(var.value.as_id) != id_refcount_map.end());
  ++id_refcount_map[var.value.as_id];
}

void ReleaseVar(PP_Var var) {
  CHECK(var.type >= PP_VARTYPE_STRING);  // Must be a ref-counted type.
  std::map<int64_t, int>::iterator iter = id_refcount_map.find(var.value.as_id);
  CHECK(iter != id_refcount_map.end());
  CHECK(iter->second > 0);
  if (--(iter->second) == 0)
    id_refcount_map.erase(iter);
}

PP_Var CreateObject(PP_Instance instance,
                    const PPP_Class_Deprecated* /*ppp_class*/,
                    void* /*ppp_class_data*/) {
  static int64_t last_id = 0;
  ++last_id;
  // Set the refcount to 0. It should really be 1, but that ref gets passed to
  // the plugin immediately (and we don't emulate all of that behavior here).
  id_refcount_map[last_id] = 0;
  return MakeObjectVar(last_id);
}

const PPB_Var_Deprecated ppb_var_deprecated_mock = {
  &AddRefVar,
  &ReleaseVar,
  NULL, // VarFromUtf8
  NULL, // VarToUtf8
  NULL, // HasProperty
  NULL, // HasMethod
  NULL, // GetProperty
  NULL, // EnumerateProperties
  NULL, // SetProperty
  NULL, // RemoveProperty
  NULL, // Call
  NULL, // Construct
  NULL, // IsInstanceOf
  &CreateObject
};

const PPB_Var ppb_var_mock = { &AddRefVar, &ReleaseVar };

class PPP_Instance_Private_ProxyTest : public TwoWayTest {
 public:
   PPP_Instance_Private_ProxyTest()
       : TwoWayTest(TwoWayTest::TEST_PPP_INTERFACE) {
      plugin().RegisterTestInterface(PPP_INSTANCE_PRIVATE_INTERFACE,
                                     &ppp_instance_private_mock);
      plugin().RegisterTestInterface(PPP_INSTANCE_INTERFACE_1_0,
                                     &ppp_instance_mock);
      host().RegisterTestInterface(PPB_VAR_DEPRECATED_INTERFACE,
                                   &ppb_var_deprecated_mock);
      host().RegisterTestInterface(PPB_VAR_INTERFACE,
                                   &ppb_var_mock);
  }
};

TEST_F(PPP_Instance_Private_ProxyTest, PPPInstancePrivate) {
  // This test controls its own instance; we can't use the one that
  // PluginProxyTestHarness provides.
  ASSERT_NE(kInstance, pp_instance());
  HostDispatcher::SetForInstance(kInstance, host().host_dispatcher());

  // This file-local global is used by the PPP_Instance mock above in order to
  // access PPB_Var_Deprecated.
  plugin_dispatcher = plugin().plugin_dispatcher();

  // Grab the host-side proxy for PPP_Instance and PPP_Instance_Private.
  const PPP_Instance_Private* ppp_instance_private =
      static_cast<const PPP_Instance_Private*>(
          host().host_dispatcher()->GetProxiedInterface(
              PPP_INSTANCE_PRIVATE_INTERFACE));
  const PPP_Instance_1_0* ppp_instance = static_cast<const PPP_Instance_1_0*>(
      host().host_dispatcher()->GetProxiedInterface(
          PPP_INSTANCE_INTERFACE_1_0));

  // Initialize an Instance, so that the plugin-side machinery will work
  // properly.
  EXPECT_EQ(PP_TRUE, ppp_instance->DidCreate(kInstance, 0, NULL, NULL));

  // Now instance_obj is valid and should have a ref-count of 1.
  PluginVarTracker& plugin_var_tracker =
      PluginResourceTracker::GetInstance()->var_tracker();
  // Check the plugin-side reference count.
  EXPECT_EQ(1, plugin_var_tracker.GetRefCountForObject(instance_obj));
  // Check the host-side var and reference count.
  ASSERT_EQ(1u, id_refcount_map.size());
  EXPECT_EQ(plugin_var_tracker.GetHostObject(instance_obj).value.as_id,
            id_refcount_map.begin()->first);
  EXPECT_EQ(0, id_refcount_map.begin()->second);

  // Call from the browser side to get the instance object.
  PP_Var host_obj = ppp_instance_private->GetInstanceObject(kInstance);
  EXPECT_EQ(instance_obj.type, host_obj.type);
  EXPECT_EQ(host_obj.value.as_id,
            plugin_var_tracker.GetHostObject(instance_obj).value.as_id);
  EXPECT_EQ(1, plugin_var_tracker.GetRefCountForObject(instance_obj));
  ASSERT_EQ(1u, id_refcount_map.size());
  // The browser should be passed a reference.
  EXPECT_EQ(1, id_refcount_map.begin()->second);

  // The plugin is going away; generally, so will all references to its instance
  // object.
  ReleaseVar(host_obj);
  // Destroy the instance. DidDestroy above decrements the reference count for
  // instance_obj, so it should also be destroyed.
  ppp_instance->DidDestroy(kInstance);
  EXPECT_EQ(-1, plugin_var_tracker.GetRefCountForObject(instance_obj));
  // Check the host-side reference count.
  EXPECT_EQ(0u, id_refcount_map.size());
}

}  // namespace proxy
}  // namespace ppapi

