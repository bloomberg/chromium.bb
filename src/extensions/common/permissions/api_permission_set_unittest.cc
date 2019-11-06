// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/pickle.h"
#include "base/values.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permissions_info.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(APIPermissionSetTest, General) {
  APIPermissionSet apis;
  apis.insert(APIPermission::kAudioCapture);
  apis.insert(APIPermission::kDns);
  apis.insert(APIPermission::kHid);
  apis.insert(APIPermission::kPower);
  apis.insert(APIPermission::kSerial);

  EXPECT_EQ(apis.find(APIPermission::kPower)->id(), APIPermission::kPower);
  EXPECT_TRUE(apis.find(APIPermission::kSocket) == apis.end());

  EXPECT_EQ(apis.size(), 5u);

  EXPECT_EQ(apis.erase(APIPermission::kAudioCapture), 1u);
  EXPECT_EQ(apis.size(), 4u);

  EXPECT_EQ(apis.erase(APIPermission::kAudioCapture), 0u);
  EXPECT_EQ(apis.size(), 4u);
}

TEST(APIPermissionSetTest, CreateUnion) {
  APIPermissionSet apis1;
  APIPermissionSet apis2;
  APIPermissionSet expected_apis;
  APIPermissionSet result;

  const APIPermissionInfo* permission_info =
    PermissionsInfo::GetInstance()->GetByID(APIPermission::kSocket);
  std::unique_ptr<APIPermission> permission =
      permission_info->CreateAPIPermission();
  {
    std::unique_ptr<base::ListValue> value(new base::ListValue());
    value->AppendString("tcp-connect:*.example.com:80");
    value->AppendString("udp-bind::8080");
    value->AppendString("udp-send-to::8888");
    ASSERT_TRUE(permission->FromValue(value.get(), NULL, NULL));
  }

  // Union with an empty set.
  apis1.insert(APIPermission::kAudioCapture);
  apis1.insert(APIPermission::kDns);
  apis1.insert(permission->Clone());
  expected_apis.insert(APIPermission::kAudioCapture);
  expected_apis.insert(APIPermission::kDns);
  expected_apis.insert(std::move(permission));

  ASSERT_TRUE(apis2.empty());
  APIPermissionSet::Union(apis1, apis2, &result);

  EXPECT_TRUE(apis1.Contains(apis2));
  EXPECT_TRUE(apis1.Contains(result));
  EXPECT_FALSE(apis2.Contains(apis1));
  EXPECT_FALSE(apis2.Contains(result));
  EXPECT_TRUE(result.Contains(apis1));
  EXPECT_TRUE(result.Contains(apis2));

  EXPECT_EQ(expected_apis, result);

  // Now use a real second set.
  apis2.insert(APIPermission::kAudioCapture);
  apis2.insert(APIPermission::kHid);
  apis2.insert(APIPermission::kPower);
  apis2.insert(APIPermission::kSerial);

  permission = permission_info->CreateAPIPermission();
  {
    std::unique_ptr<base::ListValue> value(new base::ListValue());
    value->AppendString("tcp-connect:*.example.com:80");
    value->AppendString("udp-send-to::8899");
    ASSERT_TRUE(permission->FromValue(value.get(), NULL, NULL));
  }
  apis2.insert(std::move(permission));

  expected_apis.insert(APIPermission::kAudioCapture);
  expected_apis.insert(APIPermission::kHid);
  expected_apis.insert(APIPermission::kPower);
  expected_apis.insert(APIPermission::kSerial);

  permission = permission_info->CreateAPIPermission();
  {
    std::unique_ptr<base::ListValue> value(new base::ListValue());
    value->AppendString("tcp-connect:*.example.com:80");
    value->AppendString("udp-bind::8080");
    value->AppendString("udp-send-to::8888");
    value->AppendString("udp-send-to::8899");
    ASSERT_TRUE(permission->FromValue(value.get(), NULL, NULL));
  }
  // Insert a new socket permission which will replace the old one.
  expected_apis.insert(std::move(permission));

  APIPermissionSet::Union(apis1, apis2, &result);

  EXPECT_FALSE(apis1.Contains(apis2));
  EXPECT_FALSE(apis1.Contains(result));
  EXPECT_FALSE(apis2.Contains(apis1));
  EXPECT_FALSE(apis2.Contains(result));
  EXPECT_TRUE(result.Contains(apis1));
  EXPECT_TRUE(result.Contains(apis2));

  EXPECT_EQ(expected_apis, result);
}

TEST(APIPermissionSetTest, CreateIntersection) {
  APIPermissionSet apis1;
  APIPermissionSet apis2;
  APIPermissionSet expected_apis;
  APIPermissionSet result;

  const APIPermissionInfo* permission_info =
    PermissionsInfo::GetInstance()->GetByID(APIPermission::kSocket);

  // Intersection with an empty set.
  apis1.insert(APIPermission::kAudioCapture);
  apis1.insert(APIPermission::kDns);
  std::unique_ptr<APIPermission> permission =
      permission_info->CreateAPIPermission();
  {
    std::unique_ptr<base::ListValue> value(new base::ListValue());
    value->AppendString("tcp-connect:*.example.com:80");
    value->AppendString("udp-bind::8080");
    value->AppendString("udp-send-to::8888");
    ASSERT_TRUE(permission->FromValue(value.get(), NULL, NULL));
  }
  apis1.insert(std::move(permission));

  ASSERT_TRUE(apis2.empty());
  APIPermissionSet::Intersection(apis1, apis2, &result);

  EXPECT_TRUE(apis1.Contains(result));
  EXPECT_TRUE(apis2.Contains(result));
  EXPECT_TRUE(apis1.Contains(apis2));
  EXPECT_FALSE(apis2.Contains(apis1));
  EXPECT_FALSE(result.Contains(apis1));
  EXPECT_TRUE(result.Contains(apis2));

  EXPECT_TRUE(result.empty());
  EXPECT_EQ(expected_apis, result);

  // Now use a real second set.
  apis2.insert(APIPermission::kAudioCapture);
  apis2.insert(APIPermission::kHid);
  apis2.insert(APIPermission::kPower);
  apis2.insert(APIPermission::kSerial);
  permission = permission_info->CreateAPIPermission();
  {
    std::unique_ptr<base::ListValue> value(new base::ListValue());
    value->AppendString("udp-bind::8080");
    value->AppendString("udp-send-to::8888");
    value->AppendString("udp-send-to::8899");
    ASSERT_TRUE(permission->FromValue(value.get(), NULL, NULL));
  }
  apis2.insert(std::move(permission));

  expected_apis.insert(APIPermission::kAudioCapture);
  permission = permission_info->CreateAPIPermission();
  {
    std::unique_ptr<base::ListValue> value(new base::ListValue());
    value->AppendString("udp-bind::8080");
    value->AppendString("udp-send-to::8888");
    ASSERT_TRUE(permission->FromValue(value.get(), NULL, NULL));
  }
  expected_apis.insert(std::move(permission));

  APIPermissionSet::Intersection(apis1, apis2, &result);

  EXPECT_TRUE(apis1.Contains(result));
  EXPECT_TRUE(apis2.Contains(result));
  EXPECT_FALSE(apis1.Contains(apis2));
  EXPECT_FALSE(apis2.Contains(apis1));
  EXPECT_FALSE(result.Contains(apis1));
  EXPECT_FALSE(result.Contains(apis2));

  EXPECT_EQ(expected_apis, result);
}

TEST(APIPermissionSetTest, CreateDifference) {
  APIPermissionSet apis1;
  APIPermissionSet apis2;
  APIPermissionSet expected_apis;
  APIPermissionSet result;

  const APIPermissionInfo* permission_info =
    PermissionsInfo::GetInstance()->GetByID(APIPermission::kSocket);

  // Difference with an empty set.
  apis1.insert(APIPermission::kAudioCapture);
  apis1.insert(APIPermission::kDns);
  std::unique_ptr<APIPermission> permission =
      permission_info->CreateAPIPermission();
  {
    std::unique_ptr<base::ListValue> value(new base::ListValue());
    value->AppendString("tcp-connect:*.example.com:80");
    value->AppendString("udp-bind::8080");
    value->AppendString("udp-send-to::8888");
    ASSERT_TRUE(permission->FromValue(value.get(), NULL, NULL));
  }
  apis1.insert(std::move(permission));

  ASSERT_TRUE(apis2.empty());
  APIPermissionSet::Difference(apis1, apis2, &result);

  EXPECT_EQ(apis1, result);

  // Now use a real second set.
  apis2.insert(APIPermission::kAudioCapture);
  apis2.insert(APIPermission::kHid);
  apis2.insert(APIPermission::kPower);
  apis2.insert(APIPermission::kSerial);
  permission = permission_info->CreateAPIPermission();
  {
    std::unique_ptr<base::ListValue> value(new base::ListValue());
    value->AppendString("tcp-connect:*.example.com:80");
    value->AppendString("udp-send-to::8899");
    ASSERT_TRUE(permission->FromValue(value.get(), NULL, NULL));
  }
  apis2.insert(std::move(permission));

  expected_apis.insert(APIPermission::kDns);
  permission = permission_info->CreateAPIPermission();
  {
    std::unique_ptr<base::ListValue> value(new base::ListValue());
    value->AppendString("udp-bind::8080");
    value->AppendString("udp-send-to::8888");
    ASSERT_TRUE(permission->FromValue(value.get(), NULL, NULL));
  }
  expected_apis.insert(std::move(permission));

  APIPermissionSet::Difference(apis1, apis2, &result);

  EXPECT_TRUE(apis1.Contains(result));
  EXPECT_FALSE(apis2.Contains(result));

  EXPECT_EQ(expected_apis, result);

  // |result| = |apis1| - |apis2| --> |result| intersect |apis2| == empty_set
  APIPermissionSet result2;
  APIPermissionSet::Intersection(result, apis2, &result2);
  EXPECT_TRUE(result2.empty());
}

TEST(APIPermissionSetTest, IPC) {
  APIPermissionSet apis;
  APIPermissionSet expected_apis;

  const APIPermissionInfo* permission_info =
    PermissionsInfo::GetInstance()->GetByID(APIPermission::kSocket);

  apis.insert(APIPermission::kAudioCapture);
  apis.insert(APIPermission::kDns);
  std::unique_ptr<APIPermission> permission =
      permission_info->CreateAPIPermission();
  {
    std::unique_ptr<base::ListValue> value(new base::ListValue());
    value->AppendString("tcp-connect:*.example.com:80");
    value->AppendString("udp-bind::8080");
    value->AppendString("udp-send-to::8888");
    ASSERT_TRUE(permission->FromValue(value.get(), NULL, NULL));
  }
  apis.insert(std::move(permission));

  EXPECT_NE(apis, expected_apis);

  IPC::Message m;
  WriteParam(&m, apis);
  base::PickleIterator iter(m);
  CHECK(ReadParam(&m, &iter, &expected_apis));
  EXPECT_EQ(apis, expected_apis);
}

}  // namespace extensions
