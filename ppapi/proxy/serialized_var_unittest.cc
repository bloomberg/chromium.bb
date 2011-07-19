// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppapi_proxy_test.h"

#include "ppapi/proxy/serialized_var.h"

namespace pp {
namespace proxy {

namespace {

PP_Var MakeStringVar(int64_t string_id) {
  PP_Var ret;
  ret.type = PP_VARTYPE_STRING;
  ret.value.as_id = string_id;
  return ret;
}

PP_Var MakeObjectVar(int64_t object_id) {
  PP_Var ret;
  ret.type = PP_VARTYPE_OBJECT;
  ret.value.as_id = object_id;
  return ret;
}

class SerializedVarTest : public PluginProxyTest {
 public:
  SerializedVarTest() {}
};

}  // namespace

// Tests output arguments.
TEST_F(SerializedVarTest, PluginSerializedVarOutParam) {
  PP_Var host_object = MakeObjectVar(0x31337);

  // Start tracking this object in the plugin.
  PP_Var plugin_object = var_tracker().ReceiveObjectPassRef(
      host_object, plugin_dispatcher());
  EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_object));

  {
    SerializedVar sv;
    {
      // The "OutParam" does its work in its destructor, it will write the
      // information to the SerializedVar we passed in the constructor.
      SerializedVarOutParam out_param(&sv);
      *out_param.OutParam(plugin_dispatcher()) = plugin_object;
    }

    // The object should have transformed the plugin object back to the host
    // object ID. Nothing in the var tracker should have changed yet, and no
    // messages should have been sent.
    SerializedVarTestReader reader(sv);
    EXPECT_EQ(host_object.value.as_id, reader.GetIncompleteVar().value.as_id);
    EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_object));
    EXPECT_EQ(0u, sink().message_count());
  }

  // The out param should have done an "end send pass ref" on the plugin
  // var serialization rules, which should have in turn released the reference
  // in the var tracker. Since we only had one reference, this should have sent
  // a release to the browser.
  EXPECT_EQ(-1, var_tracker().GetRefCountForObject(plugin_object));
  EXPECT_EQ(1u, sink().message_count());

  // We don't bother validating that message since it's nontrivial and the
  // PluginVarTracker test has cases that cover that this message is correct.
}

}  // namespace proxy
}  // namespace pp
