// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_UNITTEST_TEST_SERVER_H__
#define WEBKIT_GLUE_UNITTEST_TEST_SERVER_H__

#include "webkit/appcache/appcache_interfaces.h"
#include "webkit/glue/resource_loader_bridge.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_unittest.h"

using webkit_glue::ResourceLoaderBridge;

// We need to use ResourceLoaderBridge to communicate with the testserver
// instead of using URLRequest directly because URLRequests need to be run on
// the test_shell's IO thread.
class UnittestTestServer : public HTTPTestServer {
 protected:
  UnittestTestServer() {
  }

 public:
  static UnittestTestServer* CreateServer() {
    UnittestTestServer* test_server = new UnittestTestServer();
    FilePath no_cert;
    FilePath docroot(FILE_PATH_LITERAL("webkit/data"));
    if (!test_server->Start(net::TestServerLauncher::ProtoHTTP,
        "localhost", 1337, docroot, no_cert, std::wstring())) {
      delete test_server;
      return NULL;
    }
    return test_server;
  }

  virtual bool MakeGETRequest(const std::string& page_name) {
    GURL url(TestServerPage(page_name));
    webkit_glue::ResourceLoaderBridge::RequestInfo request_info;
    request_info.method = "GET";
    request_info.url = url;
    request_info.first_party_for_cookies = url;
    request_info.referrer = GURL();  // No referrer.
    request_info.frame_origin = "null";
    request_info.main_frame_origin = "null";
    request_info.headers = std::string();  // No extra headers.
    request_info.load_flags = net::LOAD_NORMAL;
    request_info.requestor_pid = 0;
    request_info.request_type = ResourceType::SUB_RESOURCE;
    request_info.request_context = 0;
    request_info.appcache_host_id = appcache::kNoHostId;
    request_info.routing_id = 0;
    scoped_ptr<ResourceLoaderBridge> loader(
        ResourceLoaderBridge::Create(request_info));
    EXPECT_TRUE(loader.get());

    ResourceLoaderBridge::SyncLoadResponse resp;
    loader->SyncLoad(&resp);
    return resp.status.is_success();
  }

 private:
  virtual ~UnittestTestServer() {}
};

#endif  // WEBKIT_GLUE_UNITTEST_TEST_SERVER_H__
