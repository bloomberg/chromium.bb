// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_url_loader_factory.h"
#include "base/logging.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

class TestURLLoaderFactoryTest : public testing::Test {
 public:
  TestURLLoaderFactoryTest() {}
  ~TestURLLoaderFactoryTest() override {}

  void SetUp() override {}

  void StartRequest(const std::string& url) { StartRequest(url, &client_); }

  void StartRequest(const std::string& url, TestURLLoaderClient* client) {
    ResourceRequest request;
    request.url = GURL(url);
    factory_.CreateLoaderAndStart(
        mojo::MakeRequest(&loader_), 0, 0, 0, request,
        client->CreateInterfacePtr(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  std::string GetData(TestURLLoaderClient* client) {
    std::string response;
    EXPECT_TRUE(client->response_body().is_valid());
    EXPECT_TRUE(
        mojo::BlockingCopyToString(client->response_body_release(), &response));
    return response;
  }

  TestURLLoaderFactory* factory() { return &factory_; }
  TestURLLoaderClient* client() { return &client_; }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  TestURLLoaderFactory factory_;
  mojom::URLLoaderPtr loader_;
  TestURLLoaderClient client_;
};

TEST_F(TestURLLoaderFactoryTest, Simple) {
  std::string url = "http://foo";
  std::string data = "bar";

  factory()->AddResponse(url, data);
  StartRequest(url);
  client()->RunUntilComplete();
  EXPECT_EQ(GetData(client()), data);
}

TEST_F(TestURLLoaderFactoryTest, MultipleSameURL) {
  std::string url = "http://foo";
  std::string data = "bar";

  factory()->AddResponse(url, data);
  factory()->AddResponse(url, data);

  StartRequest(url);
  client()->RunUntilComplete();
  EXPECT_EQ(GetData(client()), data);

  mojom::URLLoaderPtr loader2;
  TestURLLoaderClient client2;
  StartRequest(url, &client2);
  client2.RunUntilComplete();
  EXPECT_EQ(GetData(&client2), data);
}

TEST_F(TestURLLoaderFactoryTest, MultipleDifferentURL) {
  std::string url1 = "http://foo";
  std::string data1 = "bar";
  factory()->AddResponse(url1, data1);

  StartRequest(url1);
  client()->RunUntilComplete();
  EXPECT_EQ(GetData(client()), data1);

  std::string url2 = "http://foo2";
  std::string data2 = "bar2";
  factory()->AddResponse(url2, data2);

  mojom::URLLoaderPtr loader2;
  TestURLLoaderClient client2;
  StartRequest(url2, &client2);
  client2.RunUntilComplete();
  EXPECT_EQ(GetData(&client2), data2);
}

}  // namespace network
