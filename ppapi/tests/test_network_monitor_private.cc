// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_network_monitor_private.h"

#include <string.h>

#include "ppapi/cpp/private/network_list_private.h"
#include "ppapi/cpp/private/network_monitor_private.h"
#include "ppapi/tests/testing_instance.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/cpp/module.h"

REGISTER_TEST_CASE(NetworkMonitorPrivate);

namespace {

struct CallbackData {
  explicit CallbackData(PP_Instance instance)
      : call_counter(0),
        completion_callback(instance),
        list_resource(0) {
  }
  ~CallbackData() {
    if (list_resource)
      pp::Module::Get()->core()->ReleaseResource(list_resource);
  }
  int call_counter;
  TestCompletionCallback completion_callback;
  PP_Resource list_resource;
};

void TestCallback(void* user_data, PP_Resource network_list) {
  CallbackData* data = static_cast<CallbackData*>(user_data);
  data->call_counter++;

  if (data->list_resource)
    pp::Module::Get()->core()->ReleaseResource(data->list_resource);
  data->list_resource = network_list;

  // Invoke completion callback only for the first change notification.
  if (data->call_counter == 1)
    static_cast<pp::CompletionCallback>(data->completion_callback).Run(PP_OK);
}

}  // namespace

TestNetworkMonitorPrivate::TestNetworkMonitorPrivate(TestingInstance* instance)
    : TestCase(instance) {
}

bool TestNetworkMonitorPrivate::Init() {
  if (!pp::NetworkMonitorPrivate::IsAvailable())
    return false;

  return true;
}

void TestNetworkMonitorPrivate::RunTests(const std::string& filter) {
  RUN_TEST_FORCEASYNC_AND_NOT(Basic, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(2Monitors, filter);
}

std::string TestNetworkMonitorPrivate::VerifyNetworkList(
    PP_Resource network_resource) {
  pp::NetworkListPrivate network_list(network_resource);

  // Verify that there is at least one network interface.
  size_t count = network_list.GetCount();
  ASSERT_TRUE(count >= 1U);

  // Iterate over all interfaces and verify their properties.
  for (size_t iface = 0; iface < count; ++iface) {
    // Verify that the first interface has at least one address.
    std::vector<PP_NetAddress_Private> addresses;
    network_list.GetIpAddresses(iface, &addresses);
    ASSERT_TRUE(addresses.size() >= 1U);
    // Verify that the addresses are valid.
    for (size_t i = 0; i < addresses.size(); ++i) {
      ASSERT_TRUE(addresses[i].size == 4 || addresses[i].size == 16);

      // Verify that the address is not zero.
      size_t j;
      for (j = 0; j < addresses[i].size; ++j) {
        if (addresses[i].data[j] != 0)
          break;
      }
      ASSERT_TRUE(j != addresses[i].size);
    }

    // Verify that each interface has a unique name and a display name.
    ASSERT_FALSE(network_list.GetName(iface).empty());
    ASSERT_FALSE(network_list.GetDisplayName(iface).empty());

    PP_NetworkListType_Private type = network_list.GetType(iface);
    ASSERT_TRUE(type >= PP_NETWORKLIST_UNKNOWN);
    ASSERT_TRUE(type <= PP_NETWORKLIST_CELLULAR);

    PP_NetworkListState_Private state = network_list.GetState(iface);
    ASSERT_TRUE(state >= PP_NETWORKLIST_DOWN);
    ASSERT_TRUE(state <= PP_NETWORKLIST_UP);
  }

  // Try to call GetIpAddresses() without C++ wrapper and verify that
  // it always returns correct value.
  const PPB_NetworkList_Private* interface =
      static_cast<const PPB_NetworkList_Private*>(
          pp::Module::Get()->GetBrowserInterface(
              PPB_NETWORKLIST_PRIVATE_INTERFACE));
  ASSERT_TRUE(interface);
  std::vector<PP_NetAddress_Private> addresses;
  network_list.GetIpAddresses(0, &addresses);
  size_t address_count = addresses.size();
  addresses.resize(addresses.size() + 3);
  for (size_t i = 0; i < addresses.size(); ++i) {
    const char kFillValue = 123;
    memset(&addresses.front(), kFillValue,
           addresses.size() * sizeof(PP_NetAddress_Private));
    int result = interface->GetIpAddresses(network_resource, 0,
                                           &addresses.front(), i);
    ASSERT_EQ(result, static_cast<int>(address_count));

    // Verify that nothing outside the buffer was touched.
    for (char* pos = reinterpret_cast<char*>(&addresses[result]);
         pos != reinterpret_cast<char*>(&addresses[0] + addresses.size());
         ++pos) {
      ASSERT_TRUE(*pos == kFillValue);
    }
  }

  PASS();
}

std::string TestNetworkMonitorPrivate::TestBasic() {
  CallbackData callback_data(instance_->pp_instance());

  pp::NetworkMonitorPrivate network_monitor(
      instance_, &TestCallback, reinterpret_cast<void*>(&callback_data));
  ASSERT_EQ(callback_data.completion_callback.WaitForResult(), PP_OK);
  ASSERT_EQ(callback_data.call_counter, 1);

  ASSERT_SUBTEST_SUCCESS(VerifyNetworkList(callback_data.list_resource));

  PASS();
}

std::string TestNetworkMonitorPrivate::Test2Monitors() {
  CallbackData callback_data(instance_->pp_instance());

  pp::NetworkMonitorPrivate network_monitor(
      instance_, &TestCallback, reinterpret_cast<void*>(&callback_data));
  ASSERT_EQ(callback_data.completion_callback.WaitForResult(), PP_OK);
  ASSERT_EQ(callback_data.call_counter, 1);

  ASSERT_SUBTEST_SUCCESS(VerifyNetworkList(callback_data.list_resource));

  CallbackData callback_data_2(instance_->pp_instance());

  pp::NetworkMonitorPrivate network_monitor_2(
      instance_, &TestCallback, reinterpret_cast<void*>(&callback_data_2));
  ASSERT_EQ(callback_data_2.completion_callback.WaitForResult(), PP_OK);
  ASSERT_EQ(callback_data_2.call_counter, 1);

  ASSERT_SUBTEST_SUCCESS(VerifyNetworkList(callback_data_2.list_resource));

  PASS();
}
