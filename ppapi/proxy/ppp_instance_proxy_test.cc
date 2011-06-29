// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/waitable_event.h"
#include "ipc/ipc_message_utils.h"
#include "ppapi/c/dev/ppb_fullscreen_dev.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppapi_proxy_test.h"

namespace pp {
namespace proxy {

namespace {
// This is a poor man's mock of PPP_Instance using global variables.  Eventually
// we should generalize making PPAPI interface mocks by using IDL or macro/
// template magic.
PP_Instance received_instance;
uint32_t received_argc;
std::vector<std::string> received_argn;
std::vector<std::string> received_argv;
PP_Bool bool_to_return;
PP_Bool DidCreate(PP_Instance instance, uint32_t argc, const char* argn[],
                  const char* argv[]) {
  received_instance = instance;
  received_argc = argc;
  received_argn.clear();
  received_argn.insert(received_argn.begin(), argn, argn + argc);
  received_argv.clear();
  received_argv.insert(received_argv.begin(), argv, argv + argc);
  return bool_to_return;
}

void DidDestroy(PP_Instance instance) {
  received_instance = instance;
}

PP_Rect received_position;
PP_Rect received_clip;
// DidChangeView is asynchronous. We wait until the call has completed before
// proceeding on to the next test.
base::WaitableEvent did_change_view_called(false, false);
void DidChangeView(PP_Instance instance, const PP_Rect* position,
                   const PP_Rect* clip) {
  received_instance = instance;
  received_position = *position;
  received_clip = *clip;
  did_change_view_called.Signal();
}

PP_Bool received_has_focus;
base::WaitableEvent did_change_focus_called(false, false);
void DidChangeFocus(PP_Instance instance, PP_Bool has_focus) {
  received_instance = instance;
  received_has_focus = has_focus;
  did_change_focus_called.Signal();
}

PP_InputEvent received_event;
PP_Bool HandleInputEvent(PP_Instance instance, const PP_InputEvent* event) {
  received_instance = instance;
  memcpy(&received_event, event, sizeof(*event));;
  return bool_to_return;
}

PP_Bool HandleDocumentLoad(PP_Instance instance, PP_Resource url_loader) {
  // This one requires use of the PPB_URLLoader proxy and PPB_Core, plus a
  // resource tracker for the url_loader resource.
  // TODO(dmichael): Mock those out and test this function.
  NOTREACHED();
  return PP_FALSE;
}

PP_Var var_to_return;
PP_Var GetInstanceObject(PP_Instance instance) {
  received_instance = instance;
  return var_to_return;
}

// Clear all the 'received' values for our mock.  Call this before you expect
// one of the functions to be invoked.  TODO(dmichael): It would be better to
// have a flag also for each function, so we know the right one got called.
void ResetReceived() {
  received_instance = 0;
  received_argc = 0;
  received_argn.clear();
  received_argv.clear();
  memset(&received_position, 0, sizeof(received_position));
  memset(&received_clip, 0, sizeof(received_clip));
  received_has_focus = PP_FALSE;
  memset(&received_event, 0, sizeof(received_event));
}

PPP_Instance_0_4 ppp_instance_0_4 = {
  &DidCreate,
  &DidDestroy,
  &DidChangeView,
  &DidChangeFocus,
  &HandleInputEvent,
  &HandleDocumentLoad,
  &GetInstanceObject
};

PPP_Instance_0_5 ppp_instance_0_5 = {
  &DidCreate,
  &DidDestroy,
  &DidChangeView,
  &DidChangeFocus,
  &HandleInputEvent,
  &HandleDocumentLoad
};

// PPP_Instance_Proxy::DidChangeView relies on PPB_FullscreenDev being
// available with a valid implementation of IsFullScreen, so we mock it.
PP_Bool IsFullscreen(PP_Instance instance) {
  return PP_FALSE;
}
PPB_Fullscreen_Dev ppb_fullscreen_dev = { &IsFullscreen };

}  // namespace

class PPP_Instance_ProxyTest : public TwoWayTest {
 public:
   PPP_Instance_ProxyTest()
       : TwoWayTest(TwoWayTest::TEST_PPP_INTERFACE) {
   }
};

TEST_F(PPP_Instance_ProxyTest, PPPInstance0_4) {
  plugin().RegisterTestInterface(PPP_INSTANCE_INTERFACE_0_4, &ppp_instance_0_4);
  host().RegisterTestInterface(PPB_FULLSCREEN_DEV_INTERFACE,
                               &ppb_fullscreen_dev);

  // Try requesting the 0.5 version, like the browser does. This should come
  // back NULL, since we're not registering 0.5.  But this ensures that the
  // behavior through the proxy code reflects more closely what happens for a
  // real plug-in.
  const void* interface =
      host().host_dispatcher()->GetProxiedInterface(PPP_INSTANCE_INTERFACE_0_5);
  EXPECT_EQ(NULL, interface);

  // Grab the host-side proxy for the 0.4 interface.
  const PPP_Instance_0_4* ppp_instance = static_cast<const PPP_Instance_0_4*>(
      host().host_dispatcher()->GetProxiedInterface(
          PPP_INSTANCE_INTERFACE_0_4));

  // Call each function in turn, make sure we get the expected values and
  // returns.
  //
  // We don't test DidDestroy, because it has the side-effect of removing the
  // PP_Instance from the PluginDispatcher, which will cause a failure later
  // when the test is torn down.
  PP_Instance expected_instance = pp_instance();
  std::vector<std::string> expected_argn, expected_argv;
  expected_argn.push_back("Hello");
  expected_argn.push_back("world.");
  expected_argv.push_back("elloHay");
  expected_argv.push_back("orldway.");
  std::vector<const char*> argn_to_pass, argv_to_pass;
  CHECK(expected_argn.size() == expected_argv.size());
  for (size_t i = 0; i < expected_argn.size(); ++i) {
    argn_to_pass.push_back(expected_argn[i].c_str());
    argv_to_pass.push_back(expected_argv[i].c_str());
  }
  uint32_t expected_argc = expected_argn.size();
  bool_to_return = PP_TRUE;
  ResetReceived();
  EXPECT_EQ(bool_to_return, ppp_instance->DidCreate(expected_instance,
                                                    expected_argc,
                                                    &argn_to_pass[0],
                                                    &argv_to_pass[0]));
  EXPECT_EQ(received_instance, expected_instance);
  EXPECT_EQ(received_argc, expected_argc);
  EXPECT_EQ(received_argn, expected_argn);
  EXPECT_EQ(received_argv, expected_argv);

  PP_Rect expected_position = { {1, 2}, {3, 4} };
  PP_Rect expected_clip = { {5, 6}, {7, 8} };
  ResetReceived();
  ppp_instance->DidChangeView(expected_instance, &expected_position,
                              &expected_clip);
  did_change_view_called.Wait();
  EXPECT_EQ(received_instance, expected_instance);
  // If I define operator== for PP_Rect, it has to come before gtest's template
  // definitions in the translation unit, or else it's not found.  So instead of
  // defining operator== before the #include that brings in gtest, I compare the
  // individual parts.
  EXPECT_EQ(received_position.point.x, expected_position.point.x);
  EXPECT_EQ(received_position.point.y, expected_position.point.y);
  EXPECT_EQ(received_position.size.width, expected_position.size.width);
  EXPECT_EQ(received_position.size.height, expected_position.size.height);
  EXPECT_EQ(received_clip.point.x, expected_clip.point.x);
  EXPECT_EQ(received_clip.point.y, expected_clip.point.y);
  EXPECT_EQ(received_clip.size.width, expected_clip.size.width);
  EXPECT_EQ(received_clip.size.height, expected_clip.size.height);

  PP_Bool expected_has_focus = PP_TRUE;
  ResetReceived();
  ppp_instance->DidChangeFocus(expected_instance, expected_has_focus);
  did_change_focus_called.Wait();
  EXPECT_EQ(received_instance, expected_instance);
  EXPECT_EQ(received_has_focus, expected_has_focus);

  PP_InputEvent expected_event = { PP_INPUTEVENT_TYPE_KEYDOWN,  // type
                                   0,  // padding
                                   1.0,  // time_stamp
                                   { { 2, 3 } } };  // u (as PP_InputEvent_Key)
  ResetReceived();
  EXPECT_EQ(bool_to_return,
            ppp_instance->HandleInputEvent(expected_instance, &expected_event));
  EXPECT_EQ(received_instance, expected_instance);
  ASSERT_EQ(received_event.type, expected_event.type);
  // Ignore padding; it's okay if it's not serialized.
  EXPECT_EQ(received_event.time_stamp, expected_event.time_stamp);
  EXPECT_EQ(received_event.u.key.modifier, expected_event.u.key.modifier);
  EXPECT_EQ(received_event.u.key.key_code, expected_event.u.key.key_code);

  //  TODO(dmichael): Need to mock out a resource Tracker to be able to test
  //                  HandleResourceLoad. It also requires
  //                  PPB_Core.AddRefResource and for PPB_URLLoader to be
  //                  registered.

  var_to_return = PP_MakeInt32(100);
  ResetReceived();
  PP_Var result(ppp_instance->GetInstanceObject(expected_instance));
  ASSERT_EQ(var_to_return.type, result.type);
  EXPECT_EQ(var_to_return.value.as_int, result.value.as_int);
  EXPECT_EQ(received_instance, expected_instance);
}

TEST_F(PPP_Instance_ProxyTest, PPPInstance0_5) {
  plugin().RegisterTestInterface(PPP_INSTANCE_INTERFACE_0_5, &ppp_instance_0_5);
  host().RegisterTestInterface(PPB_FULLSCREEN_DEV_INTERFACE,
                               &ppb_fullscreen_dev);

  // Grab the host-side proxy for the 0.5 interface.
  const PPP_Instance_0_5* ppp_instance = static_cast<const PPP_Instance_0_5*>(
      host().host_dispatcher()->GetProxiedInterface(
          PPP_INSTANCE_INTERFACE_0_5));

  // Call each function in turn, make sure we get the expected values and
  // returns.
  //
  // We don't test DidDestroy, because it has the side-effect of removing the
  // PP_Instance from the PluginDispatcher, which will cause a failure later
  // when the test is torn down.
  PP_Instance expected_instance = pp_instance();
  std::vector<std::string> expected_argn, expected_argv;
  expected_argn.push_back("Hello");
  expected_argn.push_back("world.");
  expected_argv.push_back("elloHay");
  expected_argv.push_back("orldway.");
  std::vector<const char*> argn_to_pass, argv_to_pass;
  CHECK(expected_argn.size() == expected_argv.size());
  for (size_t i = 0; i < expected_argn.size(); ++i) {
    argn_to_pass.push_back(expected_argn[i].c_str());
    argv_to_pass.push_back(expected_argv[i].c_str());
  }
  uint32_t expected_argc = expected_argn.size();
  bool_to_return = PP_TRUE;
  ResetReceived();
  EXPECT_EQ(bool_to_return, ppp_instance->DidCreate(expected_instance,
                                                    expected_argc,
                                                    &argn_to_pass[0],
                                                    &argv_to_pass[0]));
  EXPECT_EQ(received_instance, expected_instance);
  EXPECT_EQ(received_argc, expected_argc);
  EXPECT_EQ(received_argn, expected_argn);
  EXPECT_EQ(received_argv, expected_argv);

  PP_Rect expected_position = { {1, 2}, {3, 4} };
  PP_Rect expected_clip = { {5, 6}, {7, 8} };
  ResetReceived();
  ppp_instance->DidChangeView(expected_instance, &expected_position,
                              &expected_clip);
  did_change_view_called.Wait();
  EXPECT_EQ(received_instance, expected_instance);
  EXPECT_EQ(received_position.point.x, expected_position.point.x);
  EXPECT_EQ(received_position.point.y, expected_position.point.y);
  EXPECT_EQ(received_position.size.width, expected_position.size.width);
  EXPECT_EQ(received_position.size.height, expected_position.size.height);
  EXPECT_EQ(received_clip.point.x, expected_clip.point.x);
  EXPECT_EQ(received_clip.point.y, expected_clip.point.y);
  EXPECT_EQ(received_clip.size.width, expected_clip.size.width);
  EXPECT_EQ(received_clip.size.height, expected_clip.size.height);

  PP_Bool expected_has_focus = PP_TRUE;
  ResetReceived();
  ppp_instance->DidChangeFocus(expected_instance, expected_has_focus);
  did_change_focus_called.Wait();
  EXPECT_EQ(received_instance, expected_instance);
  EXPECT_EQ(received_has_focus, expected_has_focus);

  PP_InputEvent expected_event = { PP_INPUTEVENT_TYPE_KEYDOWN,  // type
                                   0,  // padding
                                   1.0,  // time_stamp
                                   { { 2, 3 } } };  // u (as PP_InputEvent_Key)
  ResetReceived();
  EXPECT_EQ(bool_to_return,
            ppp_instance->HandleInputEvent(expected_instance, &expected_event));
  EXPECT_EQ(received_instance, expected_instance);
  ASSERT_EQ(received_event.type, expected_event.type);
  // Ignore padding; it's okay if it's not serialized.
  EXPECT_EQ(received_event.time_stamp, expected_event.time_stamp);
  EXPECT_EQ(received_event.u.key.modifier, expected_event.u.key.modifier);
  EXPECT_EQ(received_event.u.key.key_code, expected_event.u.key.key_code);

  //  TODO(dmichael): Need to mock out a resource Tracker to be able to test
  //                  HandleResourceLoad. It also requires
  //                  PPB_Core.AddRefResource and for PPB_URLLoader to be
  //                  registered.
}

}  // namespace proxy
}  // namespace pp

