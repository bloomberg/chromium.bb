// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/http_server/http_server_util.h"

#include <memory>

#include "base/path_service.h"
#import "ios/web/public/test/http_server/html_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace test {

void SetUpSimpleHttpServer(const std::map<GURL, std::string>& responses) {
  SetUpHttpServer(std::make_unique<HtmlResponseProvider>(responses));
}

void SetUpSimpleHttpServerWithSetCookies(
    const std::map<GURL, std::pair<std::string, std::string>>& responses) {
  SetUpHttpServer(std::make_unique<HtmlResponseProvider>(responses));
}

// TODO(crbug.com/694859): Cleanup tests and remove the function. Not
// necessary after switching to EmbeddedTestServer.
void SetUpFileBasedHttpServer() {}

void SetUpHttpServer(std::unique_ptr<web::ResponseProvider> provider) {
  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  DCHECK(server.IsRunning());

  server.RemoveAllResponseProviders();
  server.AddResponseProvider(std::move(provider));
}

void AddResponseProvider(std::unique_ptr<web::ResponseProvider> provider) {
  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  DCHECK(server.IsRunning());
  server.AddResponseProvider(std::move(provider));
}

}  // namespace test
}  // namespace web
