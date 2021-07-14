// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_access_manager_impl.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/font_access/font_access_test_utils.h"
#include "content/browser/font_access/font_enumeration_cache.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/font_access/font_access.mojom.h"

namespace content {

// This class exists so that tests can be written without enabling
// the kFontAccess feature flag. This will be redundant once the flag
// goes away.
class FontAccessManagerImplBrowserBase : public ContentBrowserTest {
 public:
  FontAccessManagerImplBrowserBase() {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  }

  void SetUpOnMainThread() override {
    enumeration_cache_ = FontEnumerationCache::GetInstance();
    enumeration_cache_->ResetStateForTesting();
  }

  void TearDownOnMainThread() override {
    font_access_manager()->SkipPrivacyChecksForTesting(false);
  }

  RenderFrameHost* main_rfh() {
    return shell()->web_contents()->GetMainFrame();
  }

  FontAccessManagerImpl* font_access_manager() {
    auto* storage_partition =
        static_cast<StoragePartitionImpl*>(main_rfh()->GetStoragePartition());
    return storage_partition->GetFontAccessManager();
  }

  void OverrideFontAccessLocale(const std::string& locale) {
    enumeration_cache_->OverrideLocaleForTesting(locale);
    enumeration_cache_->ResetStateForTesting();
  }

 protected:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  FontEnumerationCache* enumeration_cache_;
};

class FontAccessManagerImplBrowserTest
    : public FontAccessManagerImplBrowserBase {
 public:
  FontAccessManagerImplBrowserTest() {
    std::vector<base::Feature> enabled_features({
        blink::features::kFontAccess,
        blink::features::kFontAccessPersistent,
    });
    scoped_feature_list_->InitWithFeatures(std::move(enabled_features),
                                           /*disabled_features=*/{});
  }
};

#if defined(PLATFORM_HAS_LOCAL_FONT_ENUMERATION_IMPL)

// Disabled test: https://crbug.com/1224238
IN_PROC_BROWSER_TEST_F(FontAccessManagerImplBrowserBase,
                       DISABLED_RendererInterfaceIsBound) {
  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html")));
  // This tests that the renderer interface is bound even if the kFontAccess
  // feature flag is disabled.

  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker>& bib =
      rfh->browser_interface_broker_receiver_for_testing();
  blink::mojom::BrowserInterfaceBroker* broker = bib.internal_state()->impl();

  mojo::Remote<blink::mojom::FontAccessManager> manager_remote;
  broker->GetInterface(manager_remote.BindNewPipeAndPassReceiver());

  EXPECT_TRUE(manager_remote.is_bound() && manager_remote.is_connected())
      << "FontAccessManager remote is expected to be bound and connected.";
}

IN_PROC_BROWSER_TEST_F(FontAccessManagerImplBrowserTest, EnumerationTest) {
  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html")));
  font_access_manager()->SkipPrivacyChecksForTesting(true);

  int result = EvalJs(shell(),
                      "(async () => {"
                      "  let count = 0;"
                      "  const fonts = await "
                      "navigator.fonts.query({persistentAccess: true});"
                      "  for (const item of fonts) {"
                      "    count++;"
                      "  }"
                      "  return count;"
                      "})()")
                   .ExtractInt();
  EXPECT_GT(result, 0) << "Expected at least one font. Got: " << result;
}

#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(FontAccessManagerImplBrowserTest, LocaleTest) {
  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html")));
  font_access_manager()->SkipPrivacyChecksForTesting(true);

  OverrideFontAccessLocale("zh-cn");

  std::string result =
      EvalJs(shell(),
             "(async () => {"
             "  let fullName = '';"
             "  const fonts = await navigator.fonts.query({persistentAccess: "
             "true});"
             "  for (const item of fonts) {"
             "    if (item.postscriptName == 'MicrosoftYaHei') {"
             "      fullName = item.fullName;"
             "      break;"
             "    }"
             "  }"
             "  return fullName;"
             "})()")
          .ExtractString();
  std::string ms_yahei_utf8 = "微软雅黑";
  EXPECT_EQ(result, ms_yahei_utf8);
}

IN_PROC_BROWSER_TEST_F(FontAccessManagerImplBrowserTest,
                       UnlocalizedFamilyTest) {
  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html")));
  font_access_manager()->SkipPrivacyChecksForTesting(true);

  OverrideFontAccessLocale("zh-cn");

  std::string result =
      EvalJs(shell(),
             "(async () => {"
             "  let family = '';"
             "  const fonts = await navigator.fonts.query({persistentAccess: "
             "true});"
             "  for (const item of fonts) {"
             "    if (item.postscriptName == 'MicrosoftYaHei') {"
             "      family = item.family;"
             "      break;"
             "    }"
             "  }"
             "  return family;"
             "})()")
          .ExtractString();
  std::string unlocalized_family = "Microsoft YaHei";
  EXPECT_EQ(result, unlocalized_family);
}
#endif

#endif

}  // namespace content
