// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/resource_load_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Pointee;

}  // namespace

class AttributionTriggerRegistrationBrowserTest : public ContentBrowserTest {
 public:
  AttributionTriggerRegistrationBrowserTest() {
    AttributionManagerImpl::RunInMemoryForTesting();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets up the blink runtime feature for ConversionMeasurement.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "content/test/data/attribution_reporting");
    ASSERT_TRUE(embedded_test_server()->Start());

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    net::test_server::RegisterDefaultHandlers(https_server_.get());
    https_server_->ServeFilesFromSourceDirectory(
        "content/test/data/attribution_reporting");
    ASSERT_TRUE(https_server_->Start());

    mock_attribution_host_ = MockAttributionHost::Override(web_contents());
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  MockAttributionHost& mock_attribution_host() {
    return *mock_attribution_host_;
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::raw_ptr<MockAttributionHost> mock_attribution_host_;
};

IN_PROC_BROWSER_TEST_F(AttributionTriggerRegistrationBrowserTest,
                       NonAttributionSrcImg_TriggerRegistered) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      https_server()->GetURL("c.test", "/page_with_conversion_redirect.html")));

  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(mock_attribution_host(), RegisterDataHost)
      .WillOnce(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  GURL register_url = https_server()->GetURL(
      "c.test", "/register_trigger_headers_all_params.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createTrackingPixel($1);", register_url)));

  if (!data_host)
    loop.Run();

  data_host->WaitForTriggerData(/*num_trigger_data=*/1);
  const auto& trigger_data = data_host->trigger_data();

  EXPECT_EQ(trigger_data.size(), 1u);
  EXPECT_EQ(trigger_data.front()->reporting_origin,
            url::Origin::Create(register_url));
  EXPECT_THAT(
      trigger_data.front()->event_triggers,
      ElementsAre(Pointee(Field(&blink::mojom::EventTriggerData::data, 1)),
                  Pointee(Field(&blink::mojom::EventTriggerData::data, 2))));
}

IN_PROC_BROWSER_TEST_F(
    AttributionTriggerRegistrationBrowserTest,
    NonAttributionSrcImgRedirect_MultipleTriggersRegistered) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      https_server()->GetURL("c.test", "/page_with_conversion_redirect.html")));

  std::vector<std::unique_ptr<MockDataHost>> data_hosts;
  base::RunLoop loop;
  EXPECT_CALL(mock_attribution_host(), RegisterDataHost)
      .WillRepeatedly(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host) {
            data_hosts.push_back(GetRegisteredDataHost(std::move(host)));
            if (data_hosts.size() == 2)
              loop.Quit();
          });

  GURL register_url = https_server()->GetURL(
      "c.test", "/register_trigger_headers_and_redirect.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createTrackingPixel($1);", register_url)));

  if (data_hosts.size() != 2)
    loop.Run();

  data_hosts.front()->WaitForTriggerData(/*num_trigger_data=*/1);
  const auto& trigger_data1 = data_hosts.front()->trigger_data();

  EXPECT_EQ(trigger_data1.size(), 1u);
  EXPECT_EQ(trigger_data1.front()->reporting_origin,
            url::Origin::Create(register_url));
  EXPECT_THAT(
      trigger_data1.front()->event_triggers,
      ElementsAre(Pointee(Field(&blink::mojom::EventTriggerData::data, 5))));

  data_hosts.back()->WaitForTriggerData(/*num_trigger_data=*/1);
  const auto& trigger_data2 = data_hosts.back()->trigger_data();

  EXPECT_EQ(trigger_data2.size(), 1u);
  EXPECT_EQ(trigger_data2.front()->reporting_origin,
            url::Origin::Create(register_url));
  EXPECT_THAT(
      trigger_data2.front()->event_triggers,
      ElementsAre(Pointee(Field(&blink::mojom::EventTriggerData::data, 7))));
}

}  // namespace content
