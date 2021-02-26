// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/util/values/values_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/hats/hats_bubble_view.h"
#include "chrome/browser/ui/views/hats/hats_next_web_dialog.h"
#include "chrome/browser/ui/views/hats/hats_web_dialog.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

class HatsBubbleTest : public DialogBrowserTest {
 public:
  HatsBubbleTest() {}

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ASSERT_TRUE(browser()->is_type_normal());
    BrowserView::GetBrowserViewForBrowser(InProcessBrowserTest::browser())
        ->ShowHatsBubble("test_site_id", base::DoNothing(), base::DoNothing());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HatsBubbleTest);
};

class TestHatsWebDialog : public HatsWebDialog {
 public:
  TestHatsWebDialog(Browser* browser,
                    const base::TimeDelta& timeout,
                    const GURL& url)
      : HatsWebDialog(browser, "fake_id_not_used"),
        loading_timeout_(timeout),
        content_url_(url) {}

  // ui::WebDialogDelegate implementation.
  GURL GetDialogContentURL() const override {
    if (content_url_.is_valid()) {
      // When we have a valid overridden url, use it instead.
      return content_url_;
    }
    return HatsWebDialog::GetDialogContentURL();
  }

  void OnMainFrameResourceLoadComplete(
      const blink::mojom::ResourceLoadInfo& resource_load_info) {
    if (resource_load_info.net_error == net::Error::OK &&
        resource_load_info.original_url == resource_url_) {
      // The resource is loaded successfully.
      resource_loaded_ = true;
    }
  }

  void set_resource_url(const GURL& url) { resource_url_ = url; }
  bool resource_loaded() { return resource_loaded_; }

  MOCK_METHOD0(OnWebContentsFinishedLoad, void());
  MOCK_METHOD0(OnLoadTimedOut, void());

 private:
  const base::TimeDelta ContentLoadingTimeout() const override {
    return loading_timeout_;
  }

  base::TimeDelta loading_timeout_;
  GURL content_url_;
  GURL resource_url_;
};

class HatsWebDialogBrowserTest : public InProcessBrowserTest {
 public:
  HatsWebDialogBrowserTest() {
    feature_list_.InitAndDisableFeature(
        features::kHappinessTrackingSurveysForDesktopMigration);
  }

  TestHatsWebDialog* Create(Browser* browser,
                            const base::TimeDelta& timeout,
                            const GURL& url = GURL()) {
    auto* hats_dialog = new TestHatsWebDialog(browser, timeout, url);
    hats_dialog->CreateWebDialog(browser);
    return hats_dialog;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that calls ShowUi("default").
IN_PROC_BROWSER_TEST_F(HatsBubbleTest, InvokeUi_Default) {
  ShowAndVerifyUi();
}

// Test time out of preloading works.
IN_PROC_BROWSER_TEST_F(HatsWebDialogBrowserTest, Timeout) {
  TestHatsWebDialog* dialog = Create(browser(), base::TimeDelta());
  EXPECT_CALL(*dialog, OnLoadTimedOut).Times(1);
}

// Test preloading content works.
IN_PROC_BROWSER_TEST_F(HatsWebDialogBrowserTest, ContentPreloading) {
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  std::string contents;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::ReadFileToString(test_data_dir.AppendASCII("simple.html"),
                                       &contents));
  }

  TestHatsWebDialog* dialog =
      Create(browser(), base::TimeDelta::FromSeconds(100),
             GURL("data:text/html;charset=utf-8," + contents));
  base::RunLoop run_loop;
  EXPECT_CALL(*dialog, OnWebContentsFinishedLoad)
      .WillOnce(testing::Invoke(&run_loop, &base::RunLoop::Quit));
  run_loop.Run();
}

// Test the correct state will be set when the resource fails to load.
// Load with_inline_js.html which has an inline javascript that points to a
// nonexistent file.
IN_PROC_BROWSER_TEST_F(HatsWebDialogBrowserTest, LoadFailureInPreloading) {
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  std::string contents;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::ReadFileToString(
        test_data_dir.AppendASCII("hats").AppendASCII("with_inline_js.html"),
        &contents));
  }

  ASSERT_TRUE(embedded_test_server()->Start());

  constexpr char kJSPath[] = "/hats/nonexistent.js";
  constexpr char kSrcPlaceholder[] = "$JS_SRC";
  GURL url = embedded_test_server()->GetURL(kJSPath);
  size_t pos = contents.find(kSrcPlaceholder);
  EXPECT_NE(pos, std::string::npos);
  contents.replace(pos, strlen(kSrcPlaceholder), url.spec());

  TestHatsWebDialog* dialog =
      Create(browser(), base::TimeDelta::FromSeconds(100),
             GURL("data:text/html;charset=utf-8," + contents));
  dialog->set_resource_url(url);
  base::RunLoop run_loop;
  EXPECT_CALL(*dialog, OnWebContentsFinishedLoad)
      .WillOnce(testing::Invoke([dialog, &run_loop]() {
        EXPECT_FALSE(dialog->resource_loaded());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test cookies aren't blocked.
IN_PROC_BROWSER_TEST_F(HatsWebDialogBrowserTest, Cookies) {
  auto* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  settings_map->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                         CONTENT_SETTING_BLOCK);

  TestHatsWebDialog* dialog =
      Create(browser(), base::TimeDelta::FromSeconds(100));

  settings_map = HostContentSettingsMapFactory::GetForProfile(
      dialog->otr_profile_for_testing());
  GURL url1("https://survey.google.com/");
  GURL url2("https://survey.g.doubleclick.net/");
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            settings_map->GetContentSetting(url1, url1,
                                            ContentSettingsType::COOKIES));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            settings_map->GetContentSetting(url2, url2,
                                            ContentSettingsType::COOKIES));
}

class MockHatsNextWebDialog : public HatsNextWebDialog {
 public:
  MockHatsNextWebDialog(Browser* browser,
                        const std::string& trigger_id,
                        const GURL& hats_survey_url,
                        const base::TimeDelta& timeout,
                        base::OnceClosure success_callback,
                        base::OnceClosure failure_callback)
      : HatsNextWebDialog(browser,
                          trigger_id,
                          hats_survey_url,
                          timeout,
                          std::move(success_callback),
                          std::move(failure_callback)) {}

  MOCK_METHOD0(ShowWidget, void());
  MOCK_METHOD0(CloseWidget, void());
  MOCK_METHOD0(UpdateWidgetSize, void());

  void WaitForClose() {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, CloseWidget).WillOnce([&]() {
      widget_->Close();
      run_loop.Quit();
    });
    run_loop.Run();
  }

  void WaitForUpdateWidgetSize() {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, UpdateWidgetSize).WillOnce(testing::Invoke([&run_loop] {
      run_loop.Quit();
    }));
    run_loop.Run();
  }
};

class HatsNextWebDialogBrowserTest : public InProcessBrowserTest {
 public:
  HatsNextWebDialogBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kHappinessTrackingSurveysForDesktopMigration);
  }

  void SetUpOnMainThread() override {
    hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            browser()->profile(), base::BindRepeating(&BuildMockHatsService)));
  }

  // Open a blank tab in the main browser, inspect it, and return the devtools
  // Browser for the undocked devtools window.
  Browser* OpenUndockedDevToolsWindow() {
    ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));

    const bool is_docked = false;
    DevToolsWindow* devtools_window =
        DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), is_docked);
    return devtools_window->browser_;
  }

  MockHatsService* hats_service() { return hats_service_; }

  base::OnceClosure GetSuccessClosure() {
    return base::BindLambdaForTesting([&]() { ++success_count; });
  }

  base::OnceClosure GetFailureClosure() {
    return base::BindLambdaForTesting([&]() { ++failure_count; });
  }

  int success_count = 0;
  int failure_count = 0;

 private:
  base::test::ScopedFeatureList feature_list_;
  MockHatsService* hats_service_;
};

// Test that the web dialog correctly receives change to history state that
// indicates a survey is ready to be shown.
IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest, SurveyLoaded) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Use the preference path constants defined in hats_service.cc.
  const std::string kLastSurveyStartedTime =
      std::string(kHatsSurveyTriggerTesting) + ".last_survey_started_time";
  const std::string kLastMajorVersion =
      std::string(kHatsSurveyTriggerTesting) + ".last_major_version";

  auto* dialog = new MockHatsNextWebDialog(
      browser(), kHatsNextSurveyTriggerIDTesting,
      embedded_test_server()->GetURL("/hats/hats_next_mock.html"),
      base::TimeDelta::FromSeconds(100), GetSuccessClosure(),
      GetFailureClosure());

  // Check that no record of a survey being shown is present.
  const base::DictionaryValue* pref_data =
      browser()->profile()->GetPrefs()->GetDictionary(
          prefs::kHatsSurveyMetadata);
  base::Optional<base::Time> last_survey_started_time =
      util::ValueToTime(pref_data->FindPath(kLastSurveyStartedTime));
  base::Optional<int> last_major_version =
      pref_data->FindIntPath(kLastMajorVersion);
  ASSERT_FALSE(last_survey_started_time.has_value());
  ASSERT_FALSE(last_major_version.has_value());

  // The hats_next_mock.html will provide a state update to the dialog to
  // indicate that the survey has been loaded.
  base::RunLoop run_loop;
  EXPECT_CALL(*dialog, ShowWidget)
      .WillOnce(testing::Invoke([dialog, &run_loop]() {
        EXPECT_FALSE(dialog->IsWaitingForSurveyForTesting());
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_EQ(1, success_count);
  EXPECT_EQ(0, failure_count);

  // Check that a record of the survey being shown has been recorded.
  pref_data = browser()->profile()->GetPrefs()->GetDictionary(
      prefs::kHatsSurveyMetadata);
  last_survey_started_time =
      util::ValueToTime(pref_data->FindPath(kLastSurveyStartedTime));
  last_major_version = pref_data->FindIntPath(kLastMajorVersion);
  ASSERT_TRUE(last_survey_started_time.has_value());
  ASSERT_TRUE(last_major_version.has_value());
  ASSERT_EQ(static_cast<uint32_t>(*last_major_version),
            version_info::GetVersion().components()[0]);
}

// Test that the web dialog correctly receives change to history state that
// indicates the survey window should be closed.
IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest, SurveyClosed) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*hats_service(), HatsNextDialogClosed);
  auto* dialog = new MockHatsNextWebDialog(
      browser(), "close_for_testing",
      embedded_test_server()->GetURL("/hats/hats_next_mock.html"),
      base::TimeDelta::FromSeconds(100), GetSuccessClosure(),
      GetFailureClosure());

  // The hats_next_mock.html will provide a state update to the dialog to
  // indicate that the survey window should be closed.
  dialog->WaitForClose();

  EXPECT_EQ(0, success_count);
  EXPECT_EQ(1, failure_count);

  // Because no loaded state was provided, only a rejection should be recorded.
  histogram_tester.ExpectUniqueSample(
      kHatsShouldShowSurveyReasonHistogram,
      HatsService::ShouldShowSurveyReasons::kNoRejectedByHatsService, 1);
}

// Test that a survey which first reports as loaded, then reports closure, only
// logs that the survey was shown.
IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest, SurveyLoadedThenClosed) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*hats_service(), HatsNextDialogClosed);
  auto* dialog = new MockHatsNextWebDialog(
      browser(), kHatsNextSurveyTriggerIDTesting,
      embedded_test_server()->GetURL("/hats/hats_next_mock.html"),
      base::TimeDelta::FromSeconds(100), GetSuccessClosure(),
      GetFailureClosure());
  dialog->WaitForClose();

  EXPECT_EQ(1, success_count);
  EXPECT_EQ(0, failure_count);

  // The only recorded sample should indicate that the survey was shown.
  histogram_tester.ExpectUniqueSample(
      kHatsShouldShowSurveyReasonHistogram,
      HatsService::ShouldShowSurveyReasons::kYes, 1);
}

// Test that if the survey does not indicate it is ready for display before the
// timeout the widget is closed.
IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest, SurveyTimeout) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*hats_service(), HatsNextDialogClosed);
  auto* dialog = new MockHatsNextWebDialog(
      browser(), "invalid_test",
      embedded_test_server()->GetURL("/hats/non_existent.html"),
      base::TimeDelta::FromMilliseconds(1), GetSuccessClosure(),
      GetFailureClosure());

  dialog->WaitForClose();

  EXPECT_EQ(0, success_count);
  EXPECT_EQ(1, failure_count);
  histogram_tester.ExpectUniqueSample(
      kHatsShouldShowSurveyReasonHistogram,
      HatsService::ShouldShowSurveyReasons::kNoSurveyUnreachable, 1);
}

IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest, UnknownURLFragment) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Check that providing an unknown URL fragment results in the dialog being
  // closed.
  EXPECT_CALL(*hats_service(), HatsNextDialogClosed);
  auto* dialog = new MockHatsNextWebDialog(
      browser(), "invalid_url_fragment_for_testing",
      embedded_test_server()->GetURL("/hats/hats_next_mock.html"),
      base::TimeDelta::FromSeconds(100), GetSuccessClosure(),
      GetFailureClosure());

  dialog->WaitForClose();
  EXPECT_EQ(0, success_count);
  EXPECT_EQ(1, failure_count);
}

IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest, NewWebContents) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto* dialog = new MockHatsNextWebDialog(
      browser(), "open_new_web_contents_for_testing",
      embedded_test_server()->GetURL("/hats/hats_next_mock.html"),
      base::TimeDelta::FromSeconds(100), base::DoNothing(), base::DoNothing());

  // The mock hats dialog will push a close state after it has attempted to
  // open another web contents.
  EXPECT_CALL(*hats_service(), HatsNextDialogClosed);
  dialog->WaitForClose();

  // Check that a tab with http://foo.com (defined in hats_next_mock.html) has
  // been opened in the regular browser and is active.
  EXPECT_EQ(
      GURL("http://foo.com"),
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

// The devtools browser for undocked devtools has no tab strip and can't open
// new tabs. Instead it should open new WebContents in the main browser.
IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest,
                       NewWebContentsForDevtoolsBrowser) {
  ASSERT_TRUE(embedded_test_server()->Start());

  Browser* devtools_browser = OpenUndockedDevToolsWindow();

  auto* dialog = new MockHatsNextWebDialog(
      devtools_browser, "open_new_web_contents_for_testing",
      embedded_test_server()->GetURL("/hats/hats_next_mock.html"),
      base::TimeDelta::FromSeconds(100), base::DoNothing(), base::DoNothing());

  // The mock hats dialog will push a close state after it has attempted to
  // open another web contents.
  EXPECT_CALL(*hats_service(), HatsNextDialogClosed);
  dialog->WaitForClose();

  // Check that a tab with http://foo.com (defined in hats_next_mock.html) has
  // been opened in the regular browser and is active.
  EXPECT_EQ(
      GURL("http://foo.com"),
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest, DialogResize) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto* dialog = new MockHatsNextWebDialog(
      browser(), "resize_for_testing",
      embedded_test_server()->GetURL("/hats/hats_next_mock.html"),
      base::TimeDelta::FromSeconds(100), base::DoNothing(), base::DoNothing());

  // Check that the dialog reports a preferred size the same as the size defined
  // in hats_next_mock.html.
  constexpr auto kTargetSize = gfx::Size(70, 300);

  // Depending on renderer warm-up, an initial empty size may additionally be
  // reported before hats_next_mock.html has had a chance to resize.
  dialog->WaitForUpdateWidgetSize();
  auto size = dialog->CalculatePreferredSize();
  EXPECT_TRUE(size == kTargetSize || size == dialog->kMinSize);
  if (size != kTargetSize) {
    dialog->WaitForUpdateWidgetSize();
    EXPECT_EQ(kTargetSize, dialog->CalculatePreferredSize());
  }
}

IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest, MaximumSize) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_CALL(*hats_service(), HatsNextDialogClosed);
  auto* dialog = new MockHatsNextWebDialog(
      browser(), "resize_to_large_for_testing",
      embedded_test_server()->GetURL("/hats/hats_next_mock.html"),
      base::TimeDelta::FromSeconds(100), base::DoNothing(), base::DoNothing());

  // Check that the maximum size of the dialog is bounded appropriately by the
  // dialogs maximum size. Depending on renderer warm-up, an initial empty size
  // may additionally be reported before hats_next_mock.html has had a chance
  // to resize.
  dialog->WaitForUpdateWidgetSize();
  auto size = dialog->CalculatePreferredSize();
  EXPECT_TRUE(size == HatsNextWebDialog::kMaxSize || size == dialog->kMinSize);
  if (size != HatsNextWebDialog::kMaxSize) {
    dialog->WaitForUpdateWidgetSize();
    EXPECT_EQ(HatsNextWebDialog::kMaxSize, dialog->CalculatePreferredSize());
  }
}
