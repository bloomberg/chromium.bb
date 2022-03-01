// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/network_conditions_override_manager.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/network_conditions.h"
#include "chrome/test/chromedriver/chrome/recorder_devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::Optional;

void AssertNetworkConditionsCommand(
    const Command& command,
    const NetworkConditions& network_conditions) {
  ASSERT_EQ("Network.emulateNetworkConditions", command.method);
  ASSERT_THAT(command.params.FindBoolKey("offline"),
              Optional(network_conditions.offline));

  ASSERT_EQ(network_conditions.latency,
            command.params.FindDoubleKey("latency").value());
  ASSERT_EQ(network_conditions.download_throughput,
            command.params.FindDoubleKey("downloadThroughput").value());
  ASSERT_EQ(network_conditions.upload_throughput,
            command.params.FindDoubleKey("uploadThroughput").value());
}

}  // namespace

TEST(NetworkConditionsOverrideManager, OverrideSendsCommand) {
  RecorderDevToolsClient client;
  NetworkConditionsOverrideManager manager(&client);
  NetworkConditions network_conditions = {false, 100, 750*1024, 750*1024};
  manager.OverrideNetworkConditions(network_conditions);
  ASSERT_EQ(3u, client.commands_.size());
  ASSERT_NO_FATAL_FAILURE(
     AssertNetworkConditionsCommand(client.commands_[2], network_conditions));

  network_conditions.latency = 200;
  manager.OverrideNetworkConditions(network_conditions);
  ASSERT_EQ(6u, client.commands_.size());
  ASSERT_NO_FATAL_FAILURE(
      AssertNetworkConditionsCommand(client.commands_[5], network_conditions));
}

TEST(NetworkConditionsOverrideManager, SendsCommandOnConnect) {
  RecorderDevToolsClient client;
  NetworkConditionsOverrideManager manager(&client);
  ASSERT_EQ(0u, client.commands_.size());
  ASSERT_EQ(kOk, manager.OnConnected(&client).code());

  NetworkConditions network_conditions = {false, 100, 750*1024, 750*1024};
  manager.OverrideNetworkConditions(network_conditions);
  ASSERT_EQ(3u, client.commands_.size());
  ASSERT_EQ(kOk, manager.OnConnected(&client).code());
  ASSERT_EQ(6u, client.commands_.size());
  ASSERT_NO_FATAL_FAILURE(
      AssertNetworkConditionsCommand(client.commands_[5], network_conditions));
}

TEST(NetworkConditionsOverrideManager, SendsCommandOnNavigation) {
  RecorderDevToolsClient client;
  NetworkConditionsOverrideManager manager(&client);
  base::DictionaryValue main_frame_params;
  ASSERT_EQ(kOk,
            manager.OnEvent(&client, "Page.frameNavigated", main_frame_params)
                .code());
  ASSERT_EQ(0u, client.commands_.size());

  NetworkConditions network_conditions = {false, 100, 750*1024, 750*1024};
  manager.OverrideNetworkConditions(network_conditions);
  ASSERT_EQ(3u, client.commands_.size());
  ASSERT_EQ(kOk,
            manager.OnEvent(&client, "Page.frameNavigated", main_frame_params)
                .code());
  ASSERT_EQ(6u, client.commands_.size());
  ASSERT_NO_FATAL_FAILURE(
      AssertNetworkConditionsCommand(client.commands_[2], network_conditions));

  base::DictionaryValue sub_frame_params;
  sub_frame_params.SetString("frame.parentId", "id");
  ASSERT_EQ(
      kOk,
      manager.OnEvent(&client, "Page.frameNavigated", sub_frame_params).code());
  ASSERT_EQ(6u, client.commands_.size());
  ASSERT_NO_FATAL_FAILURE(
      AssertNetworkConditionsCommand(client.commands_[5], network_conditions));
}
