// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/device/public/cpp/test/fake_serial_port_manager.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "url/gurl.h"

namespace {

class SerialTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());

    mojo::PendingRemote<device::mojom::SerialPortManager> port_manager;
    port_manager_.AddReceiver(port_manager.InitWithNewPipeAndPassReceiver());
    context_ = SerialChooserContextFactory::GetForProfile(browser()->profile());
    context_->SetPortManagerForTesting(std::move(port_manager));

    GURL url = embedded_test_server()->GetURL("localhost", "/simple_page.html");
    ui_test_utils::NavigateToURL(browser(), url);
  }

  device::FakeSerialPortManager& port_manager() { return port_manager_; }
  SerialChooserContext* context() { return context_; }

 private:
  device::FakeSerialPortManager port_manager_;
  SerialChooserContext* context_;
};

IN_PROC_BROWSER_TEST_F(SerialTest, NavigateWithChooserCrossOrigin) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationObserver observer(
      web_contents, 1 /* number_of_navigations */,
      content::MessageLoopRunner::QuitMode::DEFERRED);

  EXPECT_TRUE(content::ExecJs(web_contents,
                              R"(navigator.serial.requestPort({});
         document.location.href = "https://google.com";)"));

  observer.Wait();
  EXPECT_FALSE(chrome::IsDeviceChooserShowingForTesting(browser()));
}

IN_PROC_BROWSER_TEST_F(SerialTest, RemovePort) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Create port and grant permission to it.
  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  url::Origin origin = web_contents->GetMainFrame()->GetLastCommittedOrigin();
  context()->GrantPortPermission(origin, origin, *port);
  port_manager().AddPort(port.Clone());

  // In order to ensure that the renderer is ready to receive events we must
  // wait for the Promise returned by getPorts() to resolve before continuing.
  EXPECT_EQ(true, content::EvalJs(web_contents, R"(
      var removedPromise;
      (async () => {
        let ports = await navigator.serial.getPorts();
        removedPromise = new Promise(resolve => {
          navigator.serial.addEventListener(
              'disconnect', e => {
                resolve(e.port === ports[0]);
              }, { once: true });
        });
        return true;
      })())"));

  port_manager().RemovePort(port->token);

  EXPECT_EQ(true, content::EvalJs(web_contents, "removedPromise"));
}

}  // namespace
