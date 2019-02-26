// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/test/bind_test_util.h"
#include "components/services/unzip/public/interfaces/constants.mojom.h"
#include "components/services/unzip/public/interfaces/unzipper.mojom.h"
#include "ios/chrome/browser/web/chrome_web_client.h"
#include "ios/web/public/service_manager_connection.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#include "ios/web/public/test/test_service_manager_context.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class ServicesTest : public PlatformTest {
 public:
  ServicesTest() : web_client_(std::make_unique<ChromeWebClient>()) {}
  ~ServicesTest() override = default;

 protected:
  service_manager::Connector* connector() {
    return web::ServiceManagerConnection::Get()->GetConnector();
  }

 private:
  web::ScopedTestingWebClient web_client_;
  web::TestWebThreadBundle thread_bundle_;
  web::TestServiceManagerContext service_manager_context_;

  DISALLOW_COPY_AND_ASSIGN(ServicesTest);
};

// Tests that services provided by Chrome reachable from browser code.
TEST_F(ServicesTest, ConnectToUnzip) {
  unzip::mojom::UnzipperPtr unzipper;
  connector()->BindInterface(unzip::mojom::kServiceName,
                             mojo::MakeRequest(&unzipper));

  // If the service is present, the unzipper interface will be connected and
  // FlushForTesting will complete without an error on the interface. Conversely
  // if there is a problem connecting to the service, we will always hit the
  // error handler before FlushForTesting returns.
  bool encountered_error = false;
  unzipper.set_connection_error_handler(
      base::BindLambdaForTesting([&] { encountered_error = true; }));
  unzipper.FlushForTesting();
  EXPECT_FALSE(encountered_error);
}
