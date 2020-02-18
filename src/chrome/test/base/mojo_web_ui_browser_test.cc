// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/mojo_web_ui_browser_test.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/web_ui_test_handler.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/data/grit/webui_test_resources.h"
#include "chrome/test/data/webui/web_ui_test.mojom.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/service_manager/public/cpp/binder_map.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Handles Mojo-style test communication.
class WebUITestPageHandler : public web_ui_test::mojom::TestRunner,
                             public WebUITestHandler {
 public:
  explicit WebUITestPageHandler(content::WebUI* web_ui) : web_ui_(web_ui) {}
  ~WebUITestPageHandler() override {}

  // Binds the Mojo test interface to this handler.
  void BindToTestRunnerReceiver(
      mojo::PendingReceiver<web_ui_test::mojom::TestRunner> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  // web_ui_test::mojom::TestRunner:
  void TestComplete(const base::Optional<std::string>& message) override {
    WebUITestHandler::TestComplete(message);
  }

  content::WebUI* GetWebUI() override { return web_ui_; }

 private:
  content::WebUI* web_ui_;
  mojo::Receiver<web_ui_test::mojom::TestRunner> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(WebUITestPageHandler);
};

}  // namespace

class MojoWebUIBrowserTest::WebUITestContentBrowserClient
    : public ChromeContentBrowserClient {
 public:
  WebUITestContentBrowserClient() {}
  WebUITestContentBrowserClient(const WebUITestContentBrowserClient&) = delete;
  WebUITestContentBrowserClient& operator=(
      const WebUITestContentBrowserClient&) = delete;

  ~WebUITestContentBrowserClient() override {}

  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      service_manager::BinderMapWithContext<content::RenderFrameHost*>* map)
      override {
    ChromeContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
        render_frame_host, map);
    map->Add<web_ui_test::mojom::TestRunner>(
        base::BindRepeating(&WebUITestContentBrowserClient::BindWebUITestRunner,
                            base::Unretained(this)));
  }

  void set_test_page_handler(WebUITestPageHandler* test_page_handler) {
    test_page_handler_ = test_page_handler;
  }

  void BindWebUITestRunner(
      content::RenderFrameHost* const render_frame_host,
      mojo::PendingReceiver<web_ui_test::mojom::TestRunner> receiver) {
    // Right now, this is expected to be called only for main frames.
    ASSERT_FALSE(render_frame_host->GetParent());
    test_page_handler_->BindToTestRunnerReceiver(std::move(receiver));
  }

 private:
  WebUITestPageHandler* test_page_handler_;
};

MojoWebUIBrowserTest::MojoWebUIBrowserTest()
    : test_content_browser_client_(
          std::make_unique<WebUITestContentBrowserClient>()) {}

MojoWebUIBrowserTest::~MojoWebUIBrowserTest() = default;

void MojoWebUIBrowserTest::SetUpOnMainThread() {
  BaseWebUIBrowserTest::SetUpOnMainThread();

  base::FilePath pak_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_MODULE, &pak_path));
  pak_path = pak_path.AppendASCII("browser_tests.pak");
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_path, ui::SCALE_FACTOR_NONE);

  content::SetBrowserClientForTesting(test_content_browser_client_.get());
}

void MojoWebUIBrowserTest::SetupHandlers() {
  content::WebUI* web_ui_instance =
      override_selected_web_ui()
          ? override_selected_web_ui()
          : browser()->tab_strip_model()->GetActiveWebContents()->GetWebUI();
  ASSERT_TRUE(web_ui_instance != nullptr);

  auto test_handler = std::make_unique<WebUITestPageHandler>(web_ui_instance);
  test_content_browser_client_->set_test_page_handler(test_handler.get());
  set_test_handler(std::move(test_handler));
}

void MojoWebUIBrowserTest::BrowsePreload(const GURL& browse_to) {
  BaseWebUIBrowserTest::BrowsePreload(browse_to);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  if (use_mojo_lite_bindings_) {
    web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
        l10n_util::GetStringUTF16(IDR_WEB_UI_TEST_MOJO_LITE_JS),
        base::NullCallback());
  } else {
    web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
        l10n_util::GetStringUTF16(IDR_WEB_UI_TEST_MOJO_JS),
        base::NullCallback());
  }
}
