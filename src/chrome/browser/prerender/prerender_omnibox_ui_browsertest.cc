// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class AutocompleteActionPredictorObserverImpl
    : public predictors::AutocompleteActionPredictor::Observer {
 public:
  explicit AutocompleteActionPredictorObserverImpl(
      predictors::AutocompleteActionPredictor* predictor) {
    observation_.Observe(predictor);
  }

  void WaitForInitialization() {
    base::RunLoop loop;
    waiting_ = loop.QuitClosure();
    loop.Run();
  }

  void OnInitialized() override {
    DCHECK(waiting_);
    std::move(waiting_).Run();
  }

  base::ScopedObservation<predictors::AutocompleteActionPredictor,
                          predictors::AutocompleteActionPredictor::Observer>
      observation_{this};

  base::OnceClosure waiting_;
};

// This is a browser test for Omnibox triggered prerendering. This is
// implemented as an interactive UI test so that it can emulate navigation
// initiated by URL typed on the Omnibox.
class PrerenderOmniboxUIBrowserTest : public InProcessBrowserTest,
                                      public content::WebContentsObserver {
 public:
  PrerenderOmniboxUIBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &PrerenderOmniboxUIBrowserTest::GetActiveWebContents,
            base::Unretained(this))) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kOmniboxTriggerForPrerender2);
  }

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  // Returns last committed page transition type. This value is only
  // meaningful after calling Observe(GetActiveWebContents()) in the test case
  // and after DidFinishNavigation.
  ui::PageTransition GetLastPageTransitionType() {
    return last_finished_page_transition_type_;
  }

  // Returns last committed page is prerendered or not. This value is only
  // meaningful after calling Observe(GetActiveWebContents()) in the test case
  // and after DidFinishNavigation.
  bool IsPrerenderingNavigation() { return is_prerendering_page_; }

 protected:
  void StartOmniboxNavigationAndWaitForActivation(const GURL& url) {
    SetOmniboxText(url.spec());
    PressEnterAndWaitForActivation(url);
  }

  void SelectAutocompleteMatchAndWaitForActivation(
      const AutocompleteMatch& match,
      int host_id) {
    GURL url = match.destination_url;
    content::test::PrerenderHostObserver prerender_observer(
        *GetActiveWebContents(), host_id);
    omnibox()->model()->OpenMatch(match, WindowOpenDisposition::CURRENT_TAB,
                                  url, std::u16string(), 0);
    prerender_observer.WaitForActivation();
  }

  predictors::AutocompleteActionPredictor* GetAutocompleteActionPredictor() {
    return predictors::AutocompleteActionPredictorFactory::GetForProfile(
        browser()->profile());
  }

  void WaitForAutocompleteActionPredictorInitialization() {
    // Initialization is already completed so it's not needed to wait for
    // initialization.
    if (GetAutocompleteActionPredictor()->initialized())
      return;
    AutocompleteActionPredictorObserverImpl predictor_observer(
        GetAutocompleteActionPredictor());
    predictor_observer.WaitForInitialization();
  }

 private:
  OmniboxView* omnibox() {
    return browser()->window()->GetLocationBar()->GetOmniboxView();
  }

  void FocusOmnibox() {
    // If the omnibox already has focus, just notify OmniboxTabHelper.
    if (omnibox()->model()->has_focus()) {
      OmniboxTabHelper::FromWebContents(GetActiveWebContents())
          ->OnFocusChanged(OMNIBOX_FOCUS_VISIBLE,
                           OMNIBOX_FOCUS_CHANGE_EXPLICIT);
    } else {
      browser()->window()->GetLocationBar()->FocusLocation(false);
    }
  }

  void SetOmniboxText(const std::string& text) {
    FocusOmnibox();
    // Enter user input mode to prevent spurious unelision.
    omnibox()->model()->SetInputInProgress(true);
    omnibox()->OnBeforePossibleChange();
    omnibox()->SetUserText(base::UTF8ToUTF16(text), true);
    omnibox()->OnAfterPossibleChange(true);
  }

  void PressEnter() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](const Browser* browser, bool ctrl_key) {
              EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
                  browser, ui::VKEY_RETURN, ctrl_key, false, false, false));
            },
            browser(), false));
  }

  // Presses enter and waits for Activation
  void PressEnterAndWaitForActivation(const GURL& url) {
    content::test::PrerenderHostObserver prerender_observer(
        *GetActiveWebContents(), url);
    PressEnter();
    prerender_observer.WaitForActivation();
  }

  // WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    last_finished_page_transition_type_ =
        navigation_handle->GetPageTransition();
    is_prerendering_page_ = navigation_handle->IsPrerenderedPageActivation();
  }

  content::test::PrerenderTestHelper prerender_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ui::PageTransition last_finished_page_transition_type_;
  bool is_prerendering_page_;
};

// This test covers the path from starting a omnibox triggered prerendering
// by AutocompleteActionPredictor, and simulate the omnibox input to check
// that prerendering can be activated successfully and the page transition type
// is correctly set as (ui::PAGE_TRANSITION_TYPED |
// ui::PAGE_TRANSITION_FROM_ADDRESS_BAR).
IN_PROC_BROWSER_TEST_F(PrerenderOmniboxUIBrowserTest,
                       PrerenderingByAutocompleteActionPredictorCanActivate) {
  Observe(GetActiveWebContents());
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));

  // Attempt to prerender a direct URL input.
  ASSERT_TRUE(GetAutocompleteActionPredictor());
  WaitForAutocompleteActionPredictorInitialization();
  const GURL kPrerenderingUrl =
      embedded_test_server()->GetURL("/empty.html?prerender");
  AutocompleteMatch match;
  match.type = AutocompleteMatchType::URL_WHAT_YOU_TYPED;
  match.destination_url = kPrerenderingUrl;
  GetAutocompleteActionPredictor()->StartPrerendering(
      match, *GetActiveWebContents(), gfx::Size(50, 50));
  StartOmniboxNavigationAndWaitForActivation(kPrerenderingUrl);
  EXPECT_EQ(static_cast<int>(GetLastPageTransitionType()),
            static_cast<int>(ui::PAGE_TRANSITION_TYPED |
                             ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  EXPECT_TRUE(IsPrerenderingNavigation());
  EXPECT_EQ(GetActiveWebContents()->GetLastCommittedURL(), kPrerenderingUrl);

  histogram_tester.ExpectUniqueSample(
      "AutocompleteActionPredictor.PrerenderStatus",
      PrerenderPredictionStatus::kHitFinished, 1);
  // The prediction result in search suggestion is recorded with kNotStarted.
  histogram_tester.ExpectUniqueSample(
      internal::kHistogramPrerenderPredictionStatusDefaultSearchEngine,
      PrerenderPredictionStatus::kNotStarted, 1);
}

// This test starts two different url prerendering by
// AutocompleteActionPredictor, and checks that the second one is going to
// cancel the first one.
IN_PROC_BROWSER_TEST_F(PrerenderOmniboxUIBrowserTest,
                       CancelAutocompleteActionPredictorOldPrerendering) {
  base::HistogramTester histogram_tester;
  Observe(GetActiveWebContents());
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));

  // Attempt to prerender a direct URL input.
  ASSERT_TRUE(GetAutocompleteActionPredictor());
  WaitForAutocompleteActionPredictorInitialization();
  const GURL kPrerenderingUrl =
      embedded_test_server()->GetURL("/empty.html?prerender");
  content::test::PrerenderHostObserver old_prerender_observer(
      *GetActiveWebContents(), kPrerenderingUrl);
  const GURL kNewUrl = embedded_test_server()->GetURL("/empty.html?newUrl");
  AutocompleteMatch match1;
  match1.type = AutocompleteMatchType::URL_WHAT_YOU_TYPED;
  match1.destination_url = kPrerenderingUrl;
  GetAutocompleteActionPredictor()->StartPrerendering(
      match1, *GetActiveWebContents(), gfx::Size(50, 50));
  EXPECT_NE(prerender_helper().GetHostForUrl(kPrerenderingUrl),
            content::RenderFrameHost::kNoFrameTreeNodeId);
  AutocompleteMatch match2;
  match2.type = AutocompleteMatchType::URL_WHAT_YOU_TYPED;
  match2.destination_url = kNewUrl;
  GetAutocompleteActionPredictor()->StartPrerendering(
      match2, *GetActiveWebContents(), gfx::Size(50, 50));

  old_prerender_observer.WaitForDestroyed();
  StartOmniboxNavigationAndWaitForActivation(kNewUrl);

  EXPECT_TRUE(IsPrerenderingNavigation());
  EXPECT_EQ(GetActiveWebContents()->GetLastCommittedURL(), kNewUrl);

  // Prerender was attempted twice and the first one was cancelled.
  histogram_tester.ExpectBucketCount(
      "AutocompleteActionPredictor.PrerenderStatus",
      PrerenderPredictionStatus::kCancelled, 1);
  histogram_tester.ExpectBucketCount(
      "AutocompleteActionPredictor.PrerenderStatus",
      PrerenderPredictionStatus::kHitFinished, 1);
  // The prediction result in search suggestion is recorded with kNotStarted.
  histogram_tester.ExpectUniqueSample(
      internal::kHistogramPrerenderPredictionStatusDefaultSearchEngine,
      PrerenderPredictionStatus::kNotStarted, 1);
}

// This test starts same url prerendering twice by AutocompleteActionPredictor,
// and checks that the second one will not trigger cancellation mechanism.
IN_PROC_BROWSER_TEST_F(PrerenderOmniboxUIBrowserTest,
                       AutocompleteActionPredictorSameURL) {
  base::HistogramTester histogram_tester;
  Observe(GetActiveWebContents());
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));

  // Attempt to prerender a direct URL input.
  ASSERT_TRUE(GetAutocompleteActionPredictor());
  WaitForAutocompleteActionPredictorInitialization();
  const GURL kPrerenderingUrl =
      embedded_test_server()->GetURL("/empty.html?prerender");
  AutocompleteMatch match;
  match.type = AutocompleteMatchType::URL_WHAT_YOU_TYPED;
  match.destination_url = kPrerenderingUrl;
  GetAutocompleteActionPredictor()->StartPrerendering(
      match, *GetActiveWebContents(), gfx::Size(50, 50));
  GetAutocompleteActionPredictor()->StartPrerendering(
      match, *GetActiveWebContents(), gfx::Size(50, 50));

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_DirectURLInput",
      /*PrerenderHost::FinalStatus::kTriggerDestroyed*/ 1, 0);

  StartOmniboxNavigationAndWaitForActivation(kPrerenderingUrl);

  EXPECT_TRUE(IsPrerenderingNavigation());
  EXPECT_EQ(GetActiveWebContents()->GetLastCommittedURL(), kPrerenderingUrl);

  histogram_tester.ExpectUniqueSample(
      "AutocompleteActionPredictor.PrerenderStatus",
      PrerenderPredictionStatus::kHitFinished, 1);
  // The prediction result in search suggestion is recorded with kNotStarted.
  histogram_tester.ExpectUniqueSample(
      internal::kHistogramPrerenderPredictionStatusDefaultSearchEngine,
      PrerenderPredictionStatus::kNotStarted, 1);
}

// Tests that NavigationHandle::IsRendererInitiated() returns RendererInitiated
// = false correctly.
IN_PROC_BROWSER_TEST_F(PrerenderOmniboxUIBrowserTest,
                       NavigationHandleIsRendererInitiatedFalse) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));

  ASSERT_TRUE(GetAutocompleteActionPredictor());
  WaitForAutocompleteActionPredictorInitialization();
  const GURL kPrerenderingUrl =
      embedded_test_server()->GetURL("/empty.html?prerender");
  {
    base::RunLoop run_loop;
    content::DidFinishNavigationObserver observer(
        GetActiveWebContents(),
        base::BindLambdaForTesting(
            [&run_loop](content::NavigationHandle* navigation_handle) {
              EXPECT_TRUE(navigation_handle->IsInPrerenderedMainFrame());
              EXPECT_FALSE(navigation_handle->IsRendererInitiated());
              run_loop.Quit();
            }));
    AutocompleteMatch match;
    match.type = AutocompleteMatchType::URL_WHAT_YOU_TYPED;
    match.destination_url = kPrerenderingUrl;
    GetAutocompleteActionPredictor()->StartPrerendering(
        match, *GetActiveWebContents(), gfx::Size(50, 50));
    run_loop.Run();
  }
  StartOmniboxNavigationAndWaitForActivation(kPrerenderingUrl);
  EXPECT_EQ(GetActiveWebContents()->GetLastCommittedURL(), kPrerenderingUrl);

  histogram_tester.ExpectUniqueSample(
      "AutocompleteActionPredictor.PrerenderStatus",
      PrerenderPredictionStatus::kHitFinished, 1);
  // The prediction result in search suggestion is recorded with kNotStarted.
  histogram_tester.ExpectUniqueSample(
      internal::kHistogramPrerenderPredictionStatusDefaultSearchEngine,
      PrerenderPredictionStatus::kNotStarted, 1);
}

// Verifies that same url can be prerendered after activation.
IN_PROC_BROWSER_TEST_F(PrerenderOmniboxUIBrowserTest,
                       SameUrlPrerenderingCanBeUsedAgainAfterActivation) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));

  ASSERT_TRUE(GetAutocompleteActionPredictor());
  WaitForAutocompleteActionPredictorInitialization();
  const GURL kPrerenderingUrl =
      embedded_test_server()->GetURL("/empty.html?prerendering");

  AutocompleteMatch match;
  match.type = AutocompleteMatchType::URL_WHAT_YOU_TYPED;
  match.destination_url = kPrerenderingUrl;
  GetAutocompleteActionPredictor()->StartPrerendering(
      match, *GetActiveWebContents(), gfx::Size(50, 50));
  StartOmniboxNavigationAndWaitForActivation(kPrerenderingUrl);

  // Test whether same prerendering url can be started successfully again and be
  // activated.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  GetAutocompleteActionPredictor()->StartPrerendering(
      match, *GetActiveWebContents(), gfx::Size(50, 50));
  StartOmniboxNavigationAndWaitForActivation(kPrerenderingUrl);

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_DirectURLInput",
      /*PrerenderHost::FinalStatus::kActivated*/ 0, 2);

  // The prediction result is recorded in each activation.
  histogram_tester.ExpectBucketCount(
      "AutocompleteActionPredictor.PrerenderStatus",
      PrerenderPredictionStatus::kHitFinished, 2);
  // The prediction result in search suggestion is recorded with kNotStarted.
  histogram_tester.ExpectBucketCount(
      internal::kHistogramPrerenderPredictionStatusDefaultSearchEngine,
      PrerenderPredictionStatus::kNotStarted, 2);
}

// TODO(https://crbug.com/1282624): Make it a Platform test after test
// infrastructure is ready and allows native code to manipulate omnibox on the
// Java side.
class PrerenderOmniboxSearchSuggestionUIBrowserTest
    : public PrerenderOmniboxUIBrowserTest {
 public:
  PrerenderOmniboxSearchSuggestionUIBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &PrerenderOmniboxUIBrowserTest::GetActiveWebContents,
            base::Unretained(this))) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSupportSearchSuggestionForPrerender2);
  }

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    // Set up a generic server.
    embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    ASSERT_TRUE(embedded_test_server()->Start());

    // Set up server for search engine.
    search_engine_server_.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    search_engine_server_.RegisterRequestHandler(base::BindRepeating(
        &PrerenderOmniboxSearchSuggestionUIBrowserTest::HandleSearchRequest,
        base::Unretained(this)));
    ASSERT_TRUE(search_engine_server_.Start());

    // Set up server for suggestion service.
    search_suggest_server_.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    search_suggest_server_.RegisterRequestHandler(
        base::BindRepeating(&PrerenderOmniboxSearchSuggestionUIBrowserTest::
                                HandleSearchSuggestRequest,
                            base::Unretained(this)));
    ASSERT_TRUE(search_suggest_server_.Start());

    TemplateURLService* model = TemplateURLServiceFactory::GetForProfile(
        chrome_test_utils::GetProfile(this));
    ASSERT_TRUE(model);
    search_test_utils::WaitForTemplateURLServiceToLoad(model);
    ASSERT_TRUE(model->loaded());
    TemplateURLData data;
    data.SetShortName(kSearchDomain16);
    data.SetKeyword(data.short_name());
    data.SetURL(
        search_engine_server_
            .GetURL(kSearchDomain,
                    "/search_page.html?q={searchTerms}&{google:prefetchSource}"
                    "{google:originalQueryForSuggestion}")
            .spec());
    data.suggestions_url =
        search_suggest_server_.GetURL(kSuggestDomain, "/?q={searchTerms}")
            .spec();
    TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
    ASSERT_TRUE(template_url);
    model->SetUserSelectedDefaultSearchProvider(template_url);
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(search_engine_server_.ShutdownAndWaitUntilComplete());
    ASSERT_TRUE(search_suggest_server_.ShutdownAndWaitUntilComplete());
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleSearchSuggestRequest(
      const net::test_server::HttpRequest& request) {
    std::string content = "";
    // $1    : origin query
    // $2    : suggested results. It should be like: "suggestion_1",
    // "suggestion_2", ..., "suggestion_n".
    std::string content_template = R"([
      "$1",
      [$2],
      ["", ""],
      [],
      {
        "google:clientdata": {
          "pre": 0
        }
      }])";
    for (auto it = search_suggestion_rules_.rbegin();
         it != search_suggestion_rules_.rend(); it++) {
      const SearchSuggestionTuple& suggestion_rule = *it;
      // Origin query matches a predefined rule.
      if (request.GetURL().spec().find(suggestion_rule.origin_query) !=
          std::string::npos) {
        // Make up suggestion list to resect the protocol of a suggestion
        // response.
        std::vector<std::string> formatted_suggestions(
            suggestion_rule.suggestions.size());
        for (size_t i = 0; i < suggestion_rule.suggestions.size(); ++i) {
          formatted_suggestions[i] =
              "\"" + suggestion_rule.suggestions[i] + "\"";
        }
        std::string suggestions_string =
            base::JoinString(formatted_suggestions, ",");
        content = base::ReplaceStringPlaceholders(
            content_template,
            {suggestion_rule.origin_query, suggestions_string}, nullptr);
        break;
      }
    }

    auto resp = std::make_unique<net::test_server::BasicHttpResponse>();
    resp->set_code(net::HTTP_OK);
    resp->set_content_type("application/json");
    resp->set_content(content);
    return resp;
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleSearchRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().spec().find("favicon") != std::string::npos)
      return nullptr;

    std::unique_ptr<net::test_server::BasicHttpResponse> resp =
        std::make_unique<net::test_server::BasicHttpResponse>();
    resp->set_code(net::HTTP_OK);
    resp->set_content_type("text/html");
    std::string content = "<html><body> HI PRERENDER! </body></html>";
    resp->set_content(content);
    return resp;
  }

  GURL GetSearchUrl(const std::string& query,
                    std::string search_terms,
                    bool is_prerender) {
    // $1: the search terms that will be retrieved.
    // $2: flag for prefetch/prerender request. Should be &pf=cs if the url is
    // expected to be used for a prerendering navigation. Otherwise it should be
    // an empty string.
    // $3: origin query. This might differ than search terms. For example, an
    //     origin query of "prerend" can have the search term of "prerender",
    //     since the suggestion service suggests to retrieve the term.
    std::string url_template = "/search_page.html?q=$1$2&oq=$3&";
    return search_engine_server_.GetURL(
        kSearchDomain,
        base::ReplaceStringPlaceholders(
            url_template, {search_terms, is_prerender ? "&pf=cs" : "", query},
            nullptr));
  }

  AutocompleteController* GetAutocompleteController() {
    OmniboxView* omnibox =
        browser()->window()->GetLocationBar()->GetOmniboxView();
    return omnibox->model()->autocomplete_controller();
  }

 protected:
  void AddNewSuggestionRule(std::string origin_query,
                            std::vector<std::string> suggestions) {
    search_suggestion_rules_.emplace_back(
        SearchSuggestionTuple(origin_query, suggestions));
  }

  void InputSearchQuery(base::StringPiece search_query) {
    // Trigger an omnibox suggest that has a prerender hint.
    AutocompleteInput input(base::ASCIIToUTF16(search_query),
                            metrics::OmniboxEventProto::BLANK,
                            ChromeAutocompleteSchemeClassifier(
                                chrome_test_utils::GetProfile(this)));
    AutocompleteController* autocomplete_controller =
        GetAutocompleteController();
    // After receiving `input`, the controller should ask suggestion service for
    // search suggestion.
    autocomplete_controller->Start(input);
  }

  int InputSearchQueryAndWaitForTrigger(base::StringPiece search_query,
                                        const GURL& expected_url) {
    content::test::PrerenderHostRegistryObserver registry_observer(
        *GetActiveWebContents());
    InputSearchQuery(search_query);
    // The suggestion service should hint a search term which is be displayed in
    // the page with `expected_url`.
    registry_observer.WaitForTrigger(expected_url);
    int host_id = prerender_helper().GetHostForUrl(expected_url);
    return host_id;
  }

 private:
  struct SearchSuggestionTuple {
    SearchSuggestionTuple(std::string origin_query,
                          std::vector<std::string> suggestions)
        : origin_query(origin_query), suggestions(suggestions) {}
    std::string origin_query;
    std::vector<std::string> suggestions;
  };

  // Stores some hard-coded rules for testing.
  // Tests can also call AddNewSuggestionRule to append a new rule.
  // Note: they are order-sensitive! The last rule(the newest added rule) has
  // the highest priority.
  std::vector<SearchSuggestionTuple> search_suggestion_rules_{
      SearchSuggestionTuple("prerenderp",
                            {"prerenderprefetch", "prerenderprefetchall"}),
      SearchSuggestionTuple("prerender2", {"prerender222", "prerender223"})};

  constexpr static char kSearchDomain[] = "a.test";
  constexpr static char kSuggestDomain[] = "b.test";
  constexpr static char16_t kSearchDomain16[] = u"a.test";
  content::test::PrerenderTestHelper prerender_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
  net::test_server::EmbeddedTestServer search_engine_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};
  net::test_server::EmbeddedTestServer search_suggest_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};
};

// Tests the basic functionality of prerendering a search suggestion with search
// suggestion hints.
IN_PROC_BROWSER_TEST_F(PrerenderOmniboxSearchSuggestionUIBrowserTest,
                       SearchPrerenderSuggestion) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));

  ASSERT_TRUE(GetAutocompleteActionPredictor());
  WaitForAutocompleteActionPredictorInitialization();
  Observe(GetActiveWebContents());
  std::string search_query = "prerender2";
  GURL expected_prerender_url =
      GetSearchUrl(search_query, "prerender222", /*is_prerender=*/true);
  int host_id =
      InputSearchQueryAndWaitForTrigger(search_query, expected_prerender_url);
  ASSERT_NE(host_id, content::RenderFrameHost::kNoFrameTreeNodeId);

  // Ensure there is a search hint.
  auto is_prerender_match = [](const AutocompleteMatch& match) {
    return BaseSearchProvider::ShouldPrerender(match);
  };
  AutocompleteController* autocomplete_controller = GetAutocompleteController();
  auto prerender_match = std::find_if(
      std::begin(autocomplete_controller->result()),
      std::end(autocomplete_controller->result()), is_prerender_match);
  ASSERT_NE(prerender_match, std::end(autocomplete_controller->result()));

  SelectAutocompleteMatchAndWaitForActivation(*prerender_match, host_id);
  EXPECT_TRUE(IsPrerenderingNavigation());

  // The displayed url shouldn't contain the parameter of pf=cs.
  GURL expected_displayed_url = GetSearchUrl(search_query, "prerender222",
                                             /*is_prerender=*/false);
  EXPECT_EQ(GetActiveWebContents()->GetLastCommittedURL(),
            expected_displayed_url);

  histogram_tester.ExpectUniqueSample(
      internal::kHistogramPrerenderPredictionStatusDefaultSearchEngine,
      PrerenderPredictionStatus::kHitFinished, 1);
  // The prediction result in Direct URL Input is recorded with kNotStarted.
  histogram_tester.ExpectUniqueSample(
      "AutocompleteActionPredictor.PrerenderStatus",
      PrerenderPredictionStatus::kNotStarted, 1);
}

// Tests that prerender maintain the previous prerendered page if the new
// prerendering aims to load a same url to the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderOmniboxSearchSuggestionUIBrowserTest,
                       SameSuggestion) {
  base::HistogramTester histogram_tester;
  AddNewSuggestionRule("prerender22", {"prerender222", "prerender223"});
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));

  ASSERT_TRUE(GetAutocompleteActionPredictor());
  WaitForAutocompleteActionPredictorInitialization();
  Observe(GetActiveWebContents());
  std::string search_query_1 = "prerender2";
  GURL expected_prerender_url =
      GetSearchUrl(search_query_1, "prerender222", /*is_prerender=*/true);

  // Trigger an omnibox suggest that has a prerender hint.
  int host_id =
      InputSearchQueryAndWaitForTrigger(search_query_1, expected_prerender_url);
  ASSERT_NE(host_id, content::RenderFrameHost::kNoFrameTreeNodeId);

  std::string search_query_2 = "prerender22";
  InputSearchQuery(search_query_2);
  base::RunLoop().RunUntilIdle();

  AutocompleteController* autocomplete_controller = GetAutocompleteController();
  // Ensure there is a search hint.
  auto is_prerender_match = [](const AutocompleteMatch& match) {
    return BaseSearchProvider::ShouldPrerender(match);
  };
  auto prerender_match = std::find_if(
      std::begin(autocomplete_controller->result()),
      std::end(autocomplete_controller->result()), is_prerender_match);
  ASSERT_NE(prerender_match, std::end(autocomplete_controller->result()));
  SelectAutocompleteMatchAndWaitForActivation(*prerender_match, host_id);
  EXPECT_TRUE(IsPrerenderingNavigation());
  GURL expected_displayed_url =
      GetSearchUrl(search_query_1, "prerender222", /*is_prerender=*/false);
  EXPECT_EQ(GetActiveWebContents()->GetLastCommittedURL(),
            expected_displayed_url);

  histogram_tester.ExpectUniqueSample(
      internal::kHistogramPrerenderPredictionStatusDefaultSearchEngine,
      PrerenderPredictionStatus::kHitFinished, 1);
  // The prediction result in Direct URL Input is recorded with kNotStarted.
  histogram_tester.ExpectUniqueSample(
      "AutocompleteActionPredictor.PrerenderStatus",
      PrerenderPredictionStatus::kNotStarted, 1);
}

// Tests that prerender is cancelled if a different prerendering starts.
IN_PROC_BROWSER_TEST_F(PrerenderOmniboxSearchSuggestionUIBrowserTest,
                       DifferentSuggestion) {
  base::HistogramTester histogram_tester;

  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));

  ASSERT_TRUE(GetAutocompleteActionPredictor());
  WaitForAutocompleteActionPredictorInitialization();
  Observe(GetActiveWebContents());

  AddNewSuggestionRule("prerender22", {"prerender222", "prerender223"});
  std::string search_query_1 = "prerender22";
  GURL prerender_url =
      GetSearchUrl(search_query_1, "prerender222", /*is_prerender=*/true);
  int host_id =
      InputSearchQueryAndWaitForTrigger(search_query_1, prerender_url);
  ASSERT_NE(host_id, content::RenderFrameHost::kNoFrameTreeNodeId);

  // Start the second prerendering with the different suggestion.
  AddNewSuggestionRule("prerender33", {"prerender333", "prerender334"});
  std::string search_query_2 = "prerender33";
  GURL prerender_url2 =
      GetSearchUrl(search_query_2, "prerender333", /*is_prerender=*/true);
  int host_id2 =
      InputSearchQueryAndWaitForTrigger(search_query_2, prerender_url2);
  ASSERT_NE(host_id, host_id2);

  AutocompleteController* autocomplete_controller = GetAutocompleteController();
  // Ensure there is a search hint.
  auto is_prerender_match = [](const AutocompleteMatch& match) {
    return BaseSearchProvider::ShouldPrerender(match);
  };
  auto prerender_match = std::find_if(
      std::begin(autocomplete_controller->result()),
      std::end(autocomplete_controller->result()), is_prerender_match);
  ASSERT_NE(prerender_match, std::end(autocomplete_controller->result()));
  SelectAutocompleteMatchAndWaitForActivation(*prerender_match, host_id2);
  EXPECT_TRUE(IsPrerenderingNavigation());
  GURL expected_displayed_url =
      GetSearchUrl(search_query_2, "prerender333", /*is_prerender=*/false);
  EXPECT_EQ(GetActiveWebContents()->GetLastCommittedURL(),
            expected_displayed_url);

  histogram_tester.ExpectBucketCount(
      internal::kHistogramPrerenderPredictionStatusDefaultSearchEngine,
      PrerenderPredictionStatus::kCancelled, 1);
  histogram_tester.ExpectBucketCount(
      internal::kHistogramPrerenderPredictionStatusDefaultSearchEngine,
      PrerenderPredictionStatus::kHitFinished, 1);
  // The prediction result in Direct URL Input is recorded with kNotStarted.
  histogram_tester.ExpectUniqueSample(
      "AutocompleteActionPredictor.PrerenderStatus",
      PrerenderPredictionStatus::kNotStarted, 1);
}

// Tests whether prerendering a search suggestion will have pf=cs parameter
// attached correctly.
IN_PROC_BROWSER_TEST_F(PrerenderOmniboxSearchSuggestionUIBrowserTest,
                       SearchPrerenderParameterVerification) {
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  Observe(GetActiveWebContents());
  std::string search_query = "prerender2";

  GURL expected_prerender_url =
      GetSearchUrl(search_query, "prerender222", /*is_prerender=*/true);
  ASSERT_TRUE(base::Contains(expected_prerender_url.spec(), "pf=cs"));
  int host_id =
      InputSearchQueryAndWaitForTrigger(search_query, expected_prerender_url);
  EXPECT_NE(host_id, content::RenderFrameHost::kNoFrameTreeNodeId);
}

}  // namespace
