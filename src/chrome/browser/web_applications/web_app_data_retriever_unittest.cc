// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_data_retriever.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/webapps/browser/installable/fake_installable_manager.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
#include "components/webapps/common/web_page_metadata_agent.mojom-test-utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace web_app {

namespace {

const char16_t kFooTitle[] = u"Foo Title";

}  // namespace

class FakeWebPageMetadataAgent
    : public webapps::mojom::WebPageMetadataAgentInterceptorForTesting {
 public:
  FakeWebPageMetadataAgent() = default;
  ~FakeWebPageMetadataAgent() override = default;

  WebPageMetadataAgent* GetForwardingInterface() override {
    NOTREACHED();
    return nullptr;
  }

  void Bind(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(mojo::PendingAssociatedReceiver<WebPageMetadataAgent>(
        std::move(handle)));
  }

  // Set |web_app_info| to respond on |GetWebApplicationInfo|.
  void SetWebApplicationInfo(const WebApplicationInfo& web_app_info) {
    web_app_info_ = web_app_info;
  }

  void GetWebPageMetadata(GetWebPageMetadataCallback callback) override {
    webapps::mojom::WebPageMetadataPtr web_page_metadata(
        webapps::mojom::WebPageMetadata::New());
    web_page_metadata->application_name = web_app_info_.title;
    web_page_metadata->description = web_app_info_.description;
    web_page_metadata->application_url = web_app_info_.start_url;

    // Convert more fields as needed.
    DCHECK(web_app_info_.manifest_icons.empty());
    DCHECK(web_app_info_.mobile_capable ==
           WebApplicationInfo::MOBILE_CAPABLE_UNSPECIFIED);

    std::move(callback).Run(std::move(web_page_metadata));
  }

 private:
  WebApplicationInfo web_app_info_;

  mojo::AssociatedReceiver<webapps::mojom::WebPageMetadataAgent> receiver_{
      this};
};

class WebAppDataRetrieverTest : public ChromeRenderViewHostTestHarness {
 public:
  WebAppDataRetrieverTest() = default;
  WebAppDataRetrieverTest(const WebAppDataRetrieverTest&) = delete;
  WebAppDataRetrieverTest& operator=(const WebAppDataRetrieverTest&) = delete;
  ~WebAppDataRetrieverTest() override = default;

  // Set fake WebPageMetadataAgent to avoid mojo connection errors.
  void SetFakeWebPageMetadataAgent() {
    web_contents()
        ->GetMainFrame()
        ->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            webapps::mojom::WebPageMetadataAgent::Name_,
            base::BindRepeating(&FakeWebPageMetadataAgent::Bind,
                                base::Unretained(&fake_chrome_render_frame_)));

    // When ProactivelySwapBrowsingInstance or RenderDocument is enabled on
    // same-site main-frame navigations, a same-site navigation might result in
    // a change of RenderFrames, which WebAppDataRetriever does not track (it
    // tracks the old RenderFrame where the navigation started in). So we
    // should disable same-site proactive BrowsingInstance for the main frame.
    // Note: this will not disable RenderDocument.
    // TODO(crbug.com/936696): Make WebAppDataRetriever support a change of
    // RenderFrames.
    content::DisableProactiveBrowsingInstanceSwapFor(
        web_contents()->GetMainFrame());
  }

  void SetRendererWebApplicationInfo(const WebApplicationInfo& web_app_info) {
    fake_chrome_render_frame_.SetWebApplicationInfo(web_app_info);
  }

  void GetWebApplicationInfoCallback(
      base::OnceClosure quit_closure,
      std::unique_ptr<WebApplicationInfo> web_app_info) {
    web_app_info_ = std::move(web_app_info);
    std::move(quit_closure).Run();
  }

  void GetIconsCallback(base::OnceClosure quit_closure,
                        std::vector<apps::IconInfo> icons) {
    icons_ = std::move(icons);
    std::move(quit_closure).Run();
  }

 protected:
  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(web_contents());
  }

  const std::unique_ptr<WebApplicationInfo>& web_app_info() {
    return web_app_info_.value();
  }

  const std::vector<apps::IconInfo>& icons() { return icons_; }

 private:
  FakeWebPageMetadataAgent fake_chrome_render_frame_;
  absl::optional<std::unique_ptr<WebApplicationInfo>> web_app_info_;
  std::vector<apps::IconInfo> icons_;
};

TEST_F(WebAppDataRetrieverTest, GetWebApplicationInfo_NoEntry) {
  SetFakeWebPageMetadataAgent();

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebApplicationInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(nullptr, web_app_info());
}

TEST_F(WebAppDataRetrieverTest, GetWebApplicationInfo_AppUrlAbsent) {
  SetFakeWebPageMetadataAgent();

  const GURL kFooUrl("https://foo.example");
  web_contents_tester()->NavigateAndCommit(kFooUrl);

  WebApplicationInfo original_web_app_info;
  original_web_app_info.start_url = GURL();

  SetRendererWebApplicationInfo(original_web_app_info);

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebApplicationInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  // If the WebApplicationInfo has no URL, we fallback to the last committed
  // URL.
  EXPECT_EQ(kFooUrl, web_app_info()->start_url);
}

TEST_F(WebAppDataRetrieverTest, GetWebApplicationInfo_AppUrlPresent) {
  SetFakeWebPageMetadataAgent();

  web_contents_tester()->NavigateAndCommit(GURL("https://foo.example"));

  WebApplicationInfo original_web_app_info;
  original_web_app_info.start_url = GURL("https://bar.example");

  SetRendererWebApplicationInfo(original_web_app_info);

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebApplicationInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(original_web_app_info.start_url, web_app_info()->start_url);
}

TEST_F(WebAppDataRetrieverTest, GetWebApplicationInfo_TitleAbsentFromRenderer) {
  SetFakeWebPageMetadataAgent();

  web_contents_tester()->NavigateAndCommit(GURL("https://foo.example"));

  web_contents_tester()->SetTitle(kFooTitle);

  WebApplicationInfo original_web_app_info;
  original_web_app_info.title = u"";

  SetRendererWebApplicationInfo(original_web_app_info);

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebApplicationInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  // If the WebApplicationInfo has no title, we fallback to the WebContents
  // title.
  EXPECT_EQ(kFooTitle, web_app_info()->title);
}

TEST_F(WebAppDataRetrieverTest,
       GetWebApplicationInfo_TitleAbsentFromWebContents) {
  SetFakeWebPageMetadataAgent();

  web_contents_tester()->NavigateAndCommit(GURL("https://foo.example"));

  web_contents_tester()->SetTitle(u"");

  WebApplicationInfo original_web_app_info;
  original_web_app_info.title = u"";

  SetRendererWebApplicationInfo(original_web_app_info);

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebApplicationInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  // If the WebApplicationInfo has no title and the WebContents has no title, we
  // fallback to start_url.
  EXPECT_EQ(base::UTF8ToUTF16(web_app_info()->start_url.spec()),
            web_app_info()->title);
}

TEST_F(WebAppDataRetrieverTest, GetWebApplicationInfo_ConnectionError) {
  // Do not set fake WebPageMetadataAgent to simulate connection error.

  web_contents_tester()->NavigateAndCommit(GURL("https://foo.example"));

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebApplicationInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(nullptr, web_app_info());
}

TEST_F(WebAppDataRetrieverTest, GetWebApplicationInfo_WebContentsDestroyed) {
  SetFakeWebPageMetadataAgent();

  web_contents_tester()->NavigateAndCommit(GURL("https://foo.example"));

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebApplicationInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  DeleteContents();
  run_loop.Run();

  EXPECT_EQ(nullptr, web_app_info());
}

TEST_F(WebAppDataRetrieverTest,
       CheckInstallabilityAndRetrieveManifest_WebContentsDestroyed) {
  SetFakeWebPageMetadataAgent();

  web_contents_tester()->NavigateAndCommit(GURL("https://foo.example"));

  {
    webapps::FakeInstallableManager::CreateForWebContentsWithManifest(
        web_contents(), webapps::NO_MANIFEST, GURL(),
        blink::mojom::Manifest::New());
  }

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.CheckInstallabilityAndRetrieveManifest(
      web_contents(), /*bypass_service_worker_check=*/false,
      base::BindLambdaForTesting(
          [&](blink::mojom::ManifestPtr opt_manifest, const GURL& manifest_url,
              bool valid_manifest_for_web_app, bool is_installable) {
            EXPECT_FALSE(opt_manifest);
            EXPECT_EQ(manifest_url, GURL());
            EXPECT_FALSE(valid_manifest_for_web_app);
            EXPECT_FALSE(is_installable);
            run_loop.Quit();
          }));
  DeleteContents();
  run_loop.Run();
}

TEST_F(WebAppDataRetrieverTest, GetIcons_WebContentsDestroyed) {
  SetFakeWebPageMetadataAgent();

  web_contents_tester()->NavigateAndCommit(GURL("https://foo.example"));

  const std::vector<GURL> icon_urls;
  bool skip_page_favicons = true;

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetIcons(web_contents(), icon_urls, skip_page_favicons,
                     base::BindLambdaForTesting(
                         [&](IconsDownloadedResult result, IconsMap icons_map,
                             DownloadedIconsHttpResults icons_http_results) {
                           EXPECT_TRUE(icons_map.empty());
                           run_loop.Quit();
                         }));
  DeleteContents();
  run_loop.Run();
}

TEST_F(WebAppDataRetrieverTest, GetWebApplicationInfo_FrameNavigated) {
  SetFakeWebPageMetadataAgent();

  web_contents_tester()->SetTitle(kFooTitle);

  const GURL kFooUrl("https://foo.example/bar");
  web_contents_tester()->NavigateAndCommit(kFooUrl.DeprecatedGetOriginAsURL());

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebApplicationInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  web_contents_tester()->NavigateAndCommit(kFooUrl);
  run_loop.Run();

  EXPECT_EQ(kFooUrl.DeprecatedGetOriginAsURL(), web_app_info()->start_url);
  EXPECT_EQ(kFooTitle, web_app_info()->title);
}

TEST_F(WebAppDataRetrieverTest, CheckInstallabilityAndRetrieveManifest) {
  SetFakeWebPageMetadataAgent();

  const GURL manifest_start_url = GURL("https://example.com/start");
  const std::u16string manifest_short_name = u"Short Name from Manifest";
  const std::u16string manifest_name = u"Name from Manifest";
  const GURL manifest_scope = GURL("https://example.com/scope");
  const SkColor manifest_theme_color = 0xAABBCCDD;

  {
    auto manifest = blink::mojom::Manifest::New();
    manifest->short_name = manifest_short_name;
    manifest->name = manifest_name;
    manifest->start_url = manifest_start_url;
    manifest->scope = manifest_scope;
    manifest->has_theme_color = true;
    manifest->theme_color = manifest_theme_color;

    webapps::FakeInstallableManager::CreateForWebContentsWithManifest(
        web_contents(), webapps::NO_ERROR_DETECTED,
        GURL("https://example.com/manifest"), std::move(manifest));
  }

  base::RunLoop run_loop;
  bool callback_called = false;

  WebAppDataRetriever retriever;

  retriever.CheckInstallabilityAndRetrieveManifest(
      web_contents(), /*bypass_service_worker_check=*/false,
      base::BindLambdaForTesting(
          [&](blink::mojom::ManifestPtr opt_manifest, const GURL& manifest_url,
              bool valid_manifest_for_web_app, bool is_installable) {
            EXPECT_TRUE(is_installable);

            EXPECT_EQ(manifest_short_name, opt_manifest->short_name);
            EXPECT_EQ(manifest_name, opt_manifest->name);
            EXPECT_EQ(manifest_start_url, opt_manifest->start_url);
            EXPECT_EQ(manifest_scope, opt_manifest->scope);
            EXPECT_EQ(manifest_theme_color, opt_manifest->theme_color);

            EXPECT_EQ(manifest_url, GURL("https://example.com/manifest"));

            callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

TEST_F(WebAppDataRetrieverTest, CheckInstallabilityFails) {
  SetFakeWebPageMetadataAgent();

  {
    webapps::FakeInstallableManager::CreateForWebContentsWithManifest(
        web_contents(), webapps::NO_MANIFEST, GURL(),
        blink::mojom::Manifest::New());
  }

  base::RunLoop run_loop;
  bool callback_called = false;

  WebAppDataRetriever retriever;

  retriever.CheckInstallabilityAndRetrieveManifest(
      web_contents(), /*bypass_service_worker_check=*/false,
      base::BindLambdaForTesting(
          [&](blink::mojom::ManifestPtr opt_manifest, const GURL& manifest_url,
              bool valid_manifest_for_web_app, bool is_installable) {
            EXPECT_FALSE(is_installable);
            EXPECT_FALSE(valid_manifest_for_web_app);
            EXPECT_EQ(manifest_url, GURL());
            callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

}  // namespace web_app
