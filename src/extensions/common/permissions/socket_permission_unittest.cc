// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/pickle.h"
#include "base/values.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/permissions/socket_permission.h"
#include "extensions/common/permissions/socket_permission_data.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

using content::SocketPermissionRequest;

void ParseTest(const std::string& permission,
               const std::string& expected_result) {
  SocketPermissionData data;
  ASSERT_TRUE(data.ParseForTest(permission)) << "Parse permission \""
                                             << permission << "\" failed.";
  EXPECT_EQ(expected_result, data.GetAsStringForTest());
}

TEST(SocketPermissionTest, General) {
  SocketPermissionData data1, data2;

  CHECK(data1.ParseForTest("tcp-connect"));
  CHECK(data2.ParseForTest("tcp-connect"));

  EXPECT_TRUE(data1 == data2);
  EXPECT_FALSE(data1 < data2);

  CHECK(data1.ParseForTest("tcp-connect"));
  CHECK(data2.ParseForTest("tcp-connect:www.example.com"));

  EXPECT_FALSE(data1 == data2);
  EXPECT_TRUE(data1 < data2);
}

TEST(SocketPermissionTest, Parse) {
  SocketPermissionData data;

  EXPECT_FALSE(data.ParseForTest(std::string()));
  EXPECT_FALSE(data.ParseForTest("*"));
  EXPECT_FALSE(data.ParseForTest("\00\00*"));
  EXPECT_FALSE(data.ParseForTest("\01*"));
  EXPECT_FALSE(data.ParseForTest("tcp-connect:www.example.com:-1"));
  EXPECT_FALSE(data.ParseForTest("tcp-connect:www.example.com:65536"));
  EXPECT_FALSE(data.ParseForTest("tcp-connect:::"));
  EXPECT_FALSE(data.ParseForTest("tcp-connect::0"));
  EXPECT_FALSE(data.ParseForTest("tcp-connect:  www.exmaple.com:  99  "));
  EXPECT_FALSE(data.ParseForTest("tcp-connect:*.exmaple.com :99"));
  EXPECT_FALSE(data.ParseForTest("tcp-connect:*.exmaple.com: 99"));
  EXPECT_FALSE(data.ParseForTest("tcp-connect:*.exmaple.com:99 "));
  EXPECT_FALSE(data.ParseForTest("tcp-connect:\t*.exmaple.com:99"));
  EXPECT_FALSE(data.ParseForTest("tcp-connect:\n*.exmaple.com:99"));
  EXPECT_FALSE(data.ParseForTest("resolve-host:exmaple.com:99"));
  EXPECT_FALSE(data.ParseForTest("resolve-host:127.0.0.1"));
  EXPECT_FALSE(data.ParseForTest("resolve-host:"));
  EXPECT_FALSE(data.ParseForTest("resolve-proxy:exmaple.com:99"));
  EXPECT_FALSE(data.ParseForTest("resolve-proxy:exmaple.com"));

  ParseTest("tcp-connect", "tcp-connect:*:*");
  ParseTest("tcp-listen", "tcp-listen:*:*");
  ParseTest("udp-bind", "udp-bind:*:*");
  ParseTest("udp-send-to", "udp-send-to:*:*");
  ParseTest("resolve-host", "resolve-host");
  ParseTest("resolve-proxy", "resolve-proxy");

  ParseTest("tcp-connect:", "tcp-connect:*:*");
  ParseTest("tcp-listen:", "tcp-listen:*:*");
  ParseTest("udp-bind:", "udp-bind:*:*");
  ParseTest("udp-send-to:", "udp-send-to:*:*");

  ParseTest("tcp-connect::", "tcp-connect:*:*");
  ParseTest("tcp-listen::", "tcp-listen:*:*");
  ParseTest("udp-bind::", "udp-bind:*:*");
  ParseTest("udp-send-to::", "udp-send-to:*:*");

  ParseTest("tcp-connect:*", "tcp-connect:*:*");
  ParseTest("tcp-listen:*", "tcp-listen:*:*");
  ParseTest("udp-bind:*", "udp-bind:*:*");
  ParseTest("udp-send-to:*", "udp-send-to:*:*");

  ParseTest("tcp-connect:*:", "tcp-connect:*:*");
  ParseTest("tcp-listen:*:", "tcp-listen:*:*");
  ParseTest("udp-bind:*:", "udp-bind:*:*");
  ParseTest("udp-send-to:*:", "udp-send-to:*:*");

  ParseTest("tcp-connect::*", "tcp-connect:*:*");
  ParseTest("tcp-listen::*", "tcp-listen:*:*");
  ParseTest("udp-bind::*", "udp-bind:*:*");
  ParseTest("udp-send-to::*", "udp-send-to:*:*");

  ParseTest("tcp-connect:www.example.com", "tcp-connect:www.example.com:*");
  ParseTest("tcp-listen:www.example.com", "tcp-listen:www.example.com:*");
  ParseTest("udp-bind:www.example.com", "udp-bind:www.example.com:*");
  ParseTest("udp-send-to:www.example.com", "udp-send-to:www.example.com:*");
  ParseTest("udp-send-to:wWW.ExAmPlE.cOm", "udp-send-to:www.example.com:*");

  ParseTest("tcp-connect:.example.com", "tcp-connect:*.example.com:*");
  ParseTest("tcp-listen:.example.com", "tcp-listen:*.example.com:*");
  ParseTest("udp-bind:.example.com", "udp-bind:*.example.com:*");
  ParseTest("udp-send-to:.example.com", "udp-send-to:*.example.com:*");

  ParseTest("tcp-connect:*.example.com", "tcp-connect:*.example.com:*");
  ParseTest("tcp-listen:*.example.com", "tcp-listen:*.example.com:*");
  ParseTest("udp-bind:*.example.com", "udp-bind:*.example.com:*");
  ParseTest("udp-send-to:*.example.com", "udp-send-to:*.example.com:*");

  ParseTest("tcp-connect::99", "tcp-connect:*:99");
  ParseTest("tcp-listen::99", "tcp-listen:*:99");
  ParseTest("udp-bind::99", "udp-bind:*:99");
  ParseTest("udp-send-to::99", "udp-send-to:*:99");

  ParseTest("tcp-connect:www.example.com", "tcp-connect:www.example.com:*");

  ParseTest("tcp-connect:*.example.com:99", "tcp-connect:*.example.com:99");
}

TEST(SocketPermissionTest, Match) {
  SocketPermissionData data;
  std::unique_ptr<SocketPermission::CheckParam> param;

  CHECK(data.ParseForTest("tcp-connect"));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::TCP_CONNECT, "www.example.com", 80));
  EXPECT_TRUE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::UDP_SEND_TO, "www.example.com", 80));
  EXPECT_FALSE(data.Check(param.get()));

  CHECK(data.ParseForTest("udp-send-to::8800"));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::UDP_SEND_TO, "www.example.com", 8800));
  EXPECT_TRUE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::UDP_SEND_TO, "smtp.example.com", 8800));
  EXPECT_TRUE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::TCP_CONNECT, "www.example.com", 80));
  EXPECT_FALSE(data.Check(param.get()));

  CHECK(data.ParseForTest("udp-send-to:*.example.com:8800"));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::UDP_SEND_TO, "www.example.com", 8800));
  EXPECT_TRUE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::UDP_SEND_TO, "smtp.example.com", 8800));
  EXPECT_TRUE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::UDP_SEND_TO, "SMTP.example.com", 8800));
  EXPECT_TRUE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::TCP_CONNECT, "www.example.com", 80));
  EXPECT_FALSE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::UDP_SEND_TO, "www.google.com", 8800));
  EXPECT_FALSE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::UDP_SEND_TO, "wwwexample.com", 8800));
  EXPECT_FALSE(data.Check(param.get()));

  CHECK(data.ParseForTest("udp-send-to:*.ExAmPlE.cOm:8800"));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::UDP_SEND_TO, "www.example.com", 8800));
  EXPECT_TRUE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::UDP_SEND_TO, "smtp.example.com", 8800));
  EXPECT_TRUE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::UDP_SEND_TO, "SMTP.example.com", 8800));
  EXPECT_TRUE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::TCP_CONNECT, "www.example.com", 80));
  EXPECT_FALSE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::UDP_SEND_TO, "www.google.com", 8800));
  EXPECT_FALSE(data.Check(param.get()));

  ASSERT_TRUE(data.ParseForTest("udp-bind::8800"));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::UDP_BIND, "127.0.0.1", 8800));
  EXPECT_TRUE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::UDP_BIND, "127.0.0.1", 8888));
  EXPECT_FALSE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::TCP_CONNECT, "www.example.com", 80));
  EXPECT_FALSE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::UDP_SEND_TO, "www.google.com", 8800));
  EXPECT_FALSE(data.Check(param.get()));

  // Do not wildcard part of ip address.
  ASSERT_TRUE(data.ParseForTest("tcp-connect:*.168.0.1:8800"));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::TCP_CONNECT, "192.168.0.1", 8800));
  EXPECT_FALSE(data.Check(param.get()));

  ASSERT_FALSE(data.ParseForTest("udp-multicast-membership:*"));
  ASSERT_FALSE(data.ParseForTest("udp-multicast-membership:*:*"));
  ASSERT_TRUE(data.ParseForTest("udp-multicast-membership"));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::UDP_BIND, "127.0.0.1", 8800));
  EXPECT_FALSE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::UDP_BIND, "127.0.0.1", 8888));
  EXPECT_FALSE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::TCP_CONNECT, "www.example.com", 80));
  EXPECT_FALSE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::UDP_SEND_TO, "www.google.com", 8800));
  EXPECT_FALSE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP, "127.0.0.1", 35));
  EXPECT_TRUE(data.Check(param.get()));

  ASSERT_TRUE(data.ParseForTest("resolve-host"));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::RESOLVE_HOST, "www.example.com", 80));
  EXPECT_TRUE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::RESOLVE_HOST, "www.example.com", 8080));
  EXPECT_TRUE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::UDP_BIND, "127.0.0.1", 8800));
  EXPECT_FALSE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::TCP_CONNECT, "127.0.0.1", 8800));
  EXPECT_FALSE(data.Check(param.get()));

  ASSERT_TRUE(data.ParseForTest("resolve-proxy"));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::RESOLVE_PROXY, "www.example.com", 80));
  EXPECT_TRUE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::RESOLVE_PROXY, "www.example.com", 8080));
  EXPECT_TRUE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::UDP_BIND, "127.0.0.1", 8800));
  EXPECT_FALSE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::TCP_CONNECT, "127.0.0.1", 8800));
  EXPECT_FALSE(data.Check(param.get()));

  ASSERT_TRUE(data.ParseForTest("network-state"));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::NETWORK_STATE, std::string(), 0));
  EXPECT_TRUE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::UDP_BIND, "127.0.0.1", 8800));
  EXPECT_FALSE(data.Check(param.get()));
  param.reset(new SocketPermission::CheckParam(
      SocketPermissionRequest::TCP_CONNECT, "127.0.0.1", 8800));
  EXPECT_FALSE(data.Check(param.get()));
}

TEST(SocketPermissionTest, IPC) {
  const APIPermissionInfo* permission_info =
      PermissionsInfo::GetInstance()->GetByID(APIPermission::kSocket);

  {
    IPC::Message m;

    std::unique_ptr<APIPermission> permission1(
        permission_info->CreateAPIPermission());
    std::unique_ptr<APIPermission> permission2(
        permission_info->CreateAPIPermission());

    permission1->Write(&m);
    base::PickleIterator iter(m);
    permission2->Read(&m, &iter);

    EXPECT_TRUE(permission1->Equal(permission2.get()));
  }

  {
    IPC::Message m;

    std::unique_ptr<APIPermission> permission1(
        permission_info->CreateAPIPermission());
    std::unique_ptr<APIPermission> permission2(
        permission_info->CreateAPIPermission());

    std::unique_ptr<base::ListValue> value(new base::ListValue());
    value->AppendString("tcp-connect:*.example.com:80");
    value->AppendString("udp-bind::8080");
    value->AppendString("udp-send-to::8888");
    ASSERT_TRUE(permission1->FromValue(value.get(), NULL, NULL));

    EXPECT_FALSE(permission1->Equal(permission2.get()));

    permission1->Write(&m);
    base::PickleIterator iter(m);
    permission2->Read(&m, &iter);
    EXPECT_TRUE(permission1->Equal(permission2.get()));
  }
}

TEST(SocketPermissionTest, Value) {
  const APIPermissionInfo* permission_info =
      PermissionsInfo::GetInstance()->GetByID(APIPermission::kSocket);

  std::unique_ptr<APIPermission> permission1(
      permission_info->CreateAPIPermission());
  std::unique_ptr<APIPermission> permission2(
      permission_info->CreateAPIPermission());

  std::unique_ptr<base::ListValue> value(new base::ListValue());
  value->AppendString("tcp-connect:*.example.com:80");
  value->AppendString("udp-bind::8080");
  value->AppendString("udp-send-to::8888");
  ASSERT_TRUE(permission1->FromValue(value.get(), NULL, NULL));

  EXPECT_FALSE(permission1->Equal(permission2.get()));

  std::unique_ptr<base::Value> vtmp(permission1->ToValue());
  ASSERT_TRUE(vtmp);
  ASSERT_TRUE(permission2->FromValue(vtmp.get(), NULL, NULL));
  EXPECT_TRUE(permission1->Equal(permission2.get()));
}

}  // namespace

}  // namespace extensions
