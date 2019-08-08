// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/string_conversions_util.h"
#include "components/autofill_assistant/browser/web_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::testing::IsEmpty;

const char* kTargetWebsitePath = "/autofill_assistant_target_website.html";

class WebControllerBrowserTest : public content::ContentBrowserTest,
                                 public content::WebContentsObserver {
 public:
  WebControllerBrowserTest() {}
  ~WebControllerBrowserTest() override {}

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    http_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTP);
    http_server_->ServeFilesFromSourceDirectory(
        "components/test/data/autofill_assistant");
    ASSERT_TRUE(http_server_->Start());
    ASSERT_TRUE(
        NavigateToURL(shell(), http_server_->GetURL(kTargetWebsitePath)));
    web_controller_ =
        WebController::CreateForWebContents(shell()->web_contents());
    Observe(shell()->web_contents());
  }

  void DidCommitAndDrawCompositorFrame() override {
    paint_occurred_during_last_loop_ = true;
  }

  void WaitTillPageIsIdle(base::TimeDelta continuous_paint_timeout) {
    base::TimeTicks finished_load_time = base::TimeTicks::Now();
    bool page_is_loading = false;
    do {
      paint_occurred_during_last_loop_ = false;
      base::RunLoop heart_beat;
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, heart_beat.QuitClosure(), base::TimeDelta::FromSeconds(3));
      heart_beat.Run();
      page_is_loading =
          web_contents()->IsWaitingForResponse() || web_contents()->IsLoading();
      if (page_is_loading) {
        finished_load_time = base::TimeTicks::Now();
      } else if ((base::TimeTicks::Now() - finished_load_time) >
                 continuous_paint_timeout) {
        // |continuous_paint_timeout| has expired since Chrome loaded the page.
        // During this period of time, Chrome has been continuously painting
        // the page. In this case, the page is probably idle, but a bug, a
        // blinking caret or a persistent animation is making Chrome paint at
        // regular intervals. Exit.
        break;
      }
    } while (page_is_loading || paint_occurred_during_last_loop_);
  }

  void RunStrictElementCheck(const Selector& selector, bool result) {
    RunElementCheck(/* strict= */ true, selector, result);
  }

  void RunLaxElementCheck(const Selector& selector, bool result) {
    RunElementCheck(/* strict= */ false, selector, result);
  }

  void RunElementCheck(bool strict, const Selector& selector, bool result) {
    std::vector<Selector> selectors{selector};
    std::vector<bool> results{result};
    RunElementChecks(strict, selectors, results);
  }

  void RunElementChecks(bool strict,
                        const std::vector<Selector>& selectors,
                        const std::vector<bool> results) {
    base::RunLoop run_loop;
    ASSERT_EQ(selectors.size(), results.size());
    size_t pending_number_of_checks = selectors.size();
    for (size_t i = 0; i < selectors.size(); i++) {
      web_controller_->ElementCheck(
          selectors[i], strict,
          base::BindOnce(&WebControllerBrowserTest::CheckElementVisibleCallback,
                         base::Unretained(this), run_loop.QuitClosure(),
                         selectors[i], &pending_number_of_checks, results[i]));
    }
    run_loop.Run();
  }

  void CheckElementVisibleCallback(const base::Closure& done_callback,
                                   const Selector& selector,
                                   size_t* pending_number_of_checks_output,
                                   bool expected_result,
                                   bool result) {
    EXPECT_EQ(expected_result, result) << "selector: " << selector;
    *pending_number_of_checks_output -= 1;
    if (*pending_number_of_checks_output == 0) {
      done_callback.Run();
    }
  }

  void ClickElement(const Selector& selector) {
    base::RunLoop run_loop;
    web_controller_->ClickElement(
        selector,
        base::BindOnce(&WebControllerBrowserTest::ClickElementCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void ClickElementCallback(const base::Closure& done_callback,
                            const ClientStatus& status) {
    EXPECT_EQ(ACTION_APPLIED, status.proto_status());
    done_callback.Run();
  }

  void TapElement(const Selector& selector) {
    base::RunLoop run_loop;
    web_controller_->TapElement(
        selector,
        base::BindOnce(&WebControllerBrowserTest::ClickElementCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void TapElementCallback(const base::Closure& done_callback, bool result) {
    ASSERT_TRUE(result);
    done_callback.Run();
  }

  void WaitForElementRemove(const Selector& selector) {
    base::RunLoop run_loop;
    web_controller_->ElementCheck(
        selector, /* strict= */ false,
        base::BindOnce(&WebControllerBrowserTest::OnWaitForElementRemove,
                       base::Unretained(this), run_loop.QuitClosure(),
                       selector));
    run_loop.Run();
  }

  void OnWaitForElementRemove(const base::Closure& done_callback,
                              const Selector& selector,
                              bool result) {
    done_callback.Run();
    if (result) {
      WaitForElementRemove(selector);
    }
  }

  void FocusElement(const Selector& selector) {
    base::RunLoop run_loop;
    web_controller_->FocusElement(
        selector,
        base::BindOnce(&WebControllerBrowserTest::OnFocusElement,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void OnFocusElement(base::OnceClosure done_callback,
                      const ClientStatus& status) {
    EXPECT_EQ(ACTION_APPLIED, status.proto_status());
    std::move(done_callback).Run();
  }

  ClientStatus SelectOption(const Selector& selector,
                            const std::string& option) {
    base::RunLoop run_loop;
    ClientStatus result;
    web_controller_->SelectOption(
        selector, option,
        base::BindOnce(&WebControllerBrowserTest::OnSelectOption,
                       base::Unretained(this), run_loop.QuitClosure(),
                       &result));
    run_loop.Run();
    return result;
  }

  void OnSelectOption(base::Closure done_callback,
                      ClientStatus* result_output,
                      const ClientStatus& status) {
    *result_output = status;
    std::move(done_callback).Run();
  }

  ClientStatus HighlightElement(const Selector& selector) {
    base::RunLoop run_loop;
    ClientStatus result;
    web_controller_->HighlightElement(
        selector, base::BindOnce(&WebControllerBrowserTest::OnHighlightElement,
                                 base::Unretained(this), run_loop.QuitClosure(),
                                 &result));
    run_loop.Run();
    return result;
  }

  void OnHighlightElement(base::Closure done_callback,
                          ClientStatus* result_output,
                          const ClientStatus& status) {
    *result_output = status;
    std::move(done_callback).Run();
  }

  ClientStatus GetOuterHtml(const Selector& selector,
                            std::string* html_output) {
    base::RunLoop run_loop;
    ClientStatus result;
    web_controller_->GetOuterHtml(
        selector, base::BindOnce(&WebControllerBrowserTest::OnGetOuterHtml,
                                 base::Unretained(this), run_loop.QuitClosure(),
                                 &result, html_output));
    run_loop.Run();
    return result;
  }

  void OnGetOuterHtml(const base::Closure& done_callback,
                      ClientStatus* successful_output,
                      std::string* html_output,
                      const ClientStatus& status,
                      const std::string& html) {
    EXPECT_EQ(ACTION_APPLIED, status.proto_status());
    *successful_output = status;
    *html_output = html;
    done_callback.Run();
  }

  void FindElement(const Selector& selector,
                   ClientStatus* status_out,
                   WebController::FindElementResult* result_out) {
    base::RunLoop run_loop;
    web_controller_->FindElement(
        selector, /* strict_mode= */ true,
        base::BindOnce(&WebControllerBrowserTest::OnFindElement,
                       base::Unretained(this), run_loop.QuitClosure(),
                       base::Unretained(status_out),
                       base::Unretained(result_out)));
    run_loop.Run();
  }

  void OnFindElement(const base::Closure& done_callback,
                     ClientStatus* status_out,
                     WebController::FindElementResult* result_out,
                     const ClientStatus& status,
                     std::unique_ptr<WebController::FindElementResult> result) {
    ASSERT_TRUE(result);
    done_callback.Run();
    if (status_out)
      *status_out = status;

    if (result_out)
      *result_out = *result;
  }

  void FindElementAndCheck(const Selector& selector,
                           size_t expected_index,
                           bool is_main_frame) {
    SCOPED_TRACE(::testing::Message() << selector << " strict");
    ClientStatus status;
    WebController::FindElementResult result;
    FindElement(selector, &status, &result);
    EXPECT_EQ(ACTION_APPLIED, status.proto_status());
    CheckFindElementResult(result, expected_index, is_main_frame);
  }

  void FindElementExpectEmptyResult(const Selector& selector) {
    SCOPED_TRACE(::testing::Message() << selector << " strict");
    ClientStatus status;
    WebController::FindElementResult result;
    FindElement(selector, &status, &result);
    EXPECT_EQ(ELEMENT_RESOLUTION_FAILED, status.proto_status());
    EXPECT_THAT(result.object_id, IsEmpty());
  }

  void CheckFindElementResult(const WebController::FindElementResult& result,
                              size_t expected_index,
                              bool is_main_frame) {
    if (is_main_frame) {
      EXPECT_EQ(shell()->web_contents()->GetMainFrame(),
                result.container_frame_host);
    } else {
      EXPECT_NE(shell()->web_contents()->GetMainFrame(),
                result.container_frame_host);
    }
    EXPECT_EQ(result.container_frame_selector_index, expected_index);
    EXPECT_FALSE(result.object_id.empty());
  }

  void GetFieldsValue(const std::vector<Selector>& selectors,
                      const std::vector<std::string>& expected_values) {
    base::RunLoop run_loop;
    ASSERT_EQ(selectors.size(), expected_values.size());
    size_t pending_number_of_checks = selectors.size();
    for (size_t i = 0; i < selectors.size(); i++) {
      web_controller_->GetFieldValue(
          selectors[i],
          base::BindOnce(&WebControllerBrowserTest::OnGetFieldValue,
                         base::Unretained(this), run_loop.QuitClosure(),
                         &pending_number_of_checks, expected_values[i]));
    }
    run_loop.Run();
  }

  void OnGetFieldValue(const base::Closure& done_callback,
                       size_t* pending_number_of_checks_output,
                       const std::string& expected_value,
                       bool exists,
                       const std::string& value) {
    // Don't use ASSERT_EQ here: if the check fails, this would result in
    // an endless loop without meaningful test results.
    EXPECT_EQ(expected_value, value);
    *pending_number_of_checks_output -= 1;
    if (*pending_number_of_checks_output == 0) {
      std::move(done_callback).Run();
    }
  }

  ClientStatus SetFieldValue(const Selector& selector,
                             const std::string& value,
                             bool simulate_key_presses) {
    base::RunLoop run_loop;
    ClientStatus result;
    web_controller_->SetFieldValue(
        selector, value, simulate_key_presses, 0,
        base::BindOnce(&WebControllerBrowserTest::OnSetFieldValue,
                       base::Unretained(this), run_loop.QuitClosure(),
                       &result));
    run_loop.Run();
    return result;
  }

  void OnSetFieldValue(const base::Closure& done_callback,
                       ClientStatus* result_output,
                       const ClientStatus& status) {
    *result_output = status;
    std::move(done_callback).Run();
  }

  ClientStatus SendKeyboardInput(const Selector& selector,
                                 const std::vector<UChar32>& codepoints,
                                 int delay_in_milli) {
    base::RunLoop run_loop;
    ClientStatus result;
    web_controller_->SendKeyboardInput(
        selector, codepoints, delay_in_milli,
        base::BindOnce(&WebControllerBrowserTest::OnSendKeyboardInput,
                       base::Unretained(this), run_loop.QuitClosure(),
                       &result));
    run_loop.Run();
    return result;
  }

  ClientStatus SendKeyboardInput(const Selector& selector,
                                 const std::vector<UChar32>& codepoints) {
    return SendKeyboardInput(selector, codepoints, -1);
  }

  void OnSendKeyboardInput(const base::Closure& done_callback,
                           ClientStatus* result_output,
                           const ClientStatus& status) {
    *result_output = status;
    std::move(done_callback).Run();
  }

  ClientStatus SetAttribute(const Selector& selector,
                            const std::vector<std::string>& attribute,
                            const std::string& value) {
    base::RunLoop run_loop;
    ClientStatus result;
    web_controller_->SetAttribute(
        selector, attribute, value,
        base::BindOnce(&WebControllerBrowserTest::OnSetAttribute,
                       base::Unretained(this), run_loop.QuitClosure(),
                       &result));
    run_loop.Run();
    return result;
  }

  void OnSetAttribute(const base::Closure& done_callback,
                      ClientStatus* result_output,
                      const ClientStatus& status) {
    *result_output = status;
    std::move(done_callback).Run();
  }

  bool HasCookie() {
    base::RunLoop run_loop;
    bool result;
    web_controller_->HasCookie(base::BindOnce(
        &WebControllerBrowserTest::OnCookieResult, base::Unretained(this),
        run_loop.QuitClosure(), &result));
    run_loop.Run();
    return result;
  }

  bool SetCookie() {
    base::RunLoop run_loop;
    bool result;
    web_controller_->SetCookie(
        "http://example.com",
        base::BindOnce(&WebControllerBrowserTest::OnCookieResult,
                       base::Unretained(this), run_loop.QuitClosure(),
                       &result));
    run_loop.Run();
    return result;
  }

  void OnCookieResult(const base::Closure& done_callback,
                      bool* result_output,
                      bool result) {
    *result_output = result;
    std::move(done_callback).Run();
  }

 protected:
  std::unique_ptr<WebController> web_controller_;

 private:
  std::unique_ptr<net::EmbeddedTestServer> http_server_;
  bool paint_occurred_during_last_loop_ = false;

  DISALLOW_COPY_AND_ASSIGN(WebControllerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ElementExistenceCheck) {
  // A visible element
  RunLaxElementCheck(Selector({"#button"}), true);

  // A hidden element.
  RunLaxElementCheck(Selector({"#hidden"}), true);

  // A nonexistent element.
  RunLaxElementCheck(Selector({"#doesnotexist"}), false);

  // A pseudo-element
  RunLaxElementCheck(Selector({"#terms-and-conditions"}, BEFORE), true);

  // An invisible pseudo-element
  //
  // TODO(b/129461999): This is wrong; it should exist. Fix it.
  RunLaxElementCheck(Selector({"#button"}, BEFORE), false);

  // A non-existent pseudo-element
  RunLaxElementCheck(Selector({"#button"}, AFTER), false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, VisibilityRequirementCheck) {
  // A visible element
  RunLaxElementCheck(Selector({"#button"}).MustBeVisible(), true);

  // A hidden element.
  RunLaxElementCheck(Selector({"#hidden"}).MustBeVisible(), false);

  // A non-existent element
  RunLaxElementCheck(Selector({"#doesnotexist"}).MustBeVisible(), false);

  // A pseudo-element
  RunLaxElementCheck(
      Selector({"#terms-and-conditions"}, BEFORE).MustBeVisible(), true);

  // An invisible pseudo-element
  RunLaxElementCheck(Selector({"#button"}, BEFORE).MustBeVisible(), false);

  // A non-existent pseudo-element
  RunLaxElementCheck(Selector({"#button"}, AFTER).MustBeVisible(), false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, MultipleVisibleElementCheck) {
  // both visible
  RunLaxElementCheck(Selector({"#button,#select"}).MustBeVisible(), true);
  RunStrictElementCheck(Selector({"#button,#select"}).MustBeVisible(), false);

  // one visible (first non-visible)
  RunLaxElementCheck(Selector({"#hidden,#select"}).MustBeVisible(), true);
  RunStrictElementCheck(Selector({"#hidden,#select"}).MustBeVisible(), true);

  // one visible (first visible)
  RunLaxElementCheck(Selector({"#button,#hidden"}).MustBeVisible(), true);
  RunStrictElementCheck(Selector({"#hidden,#select"}).MustBeVisible(), true);

  // one invisible, one non-existent
  RunLaxElementCheck(Selector({"#doesnotexist,#hidden"}).MustBeVisible(),
                     false);
  RunStrictElementCheck(Selector({"#doesnotexist,#hidden"}).MustBeVisible(),
                        false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, InnerTextCondition) {
  Selector selector({"#with_inner_text span"});
  selector.must_be_visible = true;
  RunLaxElementCheck(selector, true);
  RunStrictElementCheck(selector.MustBeVisible(), false);

  // No matches
  selector.inner_text_pattern = "no match";
  selector.must_be_visible = false;
  RunLaxElementCheck(selector, false);
  selector.must_be_visible = true;
  RunLaxElementCheck(selector, false);

  // Matches exactly one visible element.
  selector.inner_text_pattern = "hello, world";
  selector.must_be_visible = false;
  RunLaxElementCheck(selector, true);
  RunStrictElementCheck(selector, true);
  selector.must_be_visible = true;
  RunLaxElementCheck(selector, true);
  RunStrictElementCheck(selector, true);

  // Matches two visible elements
  selector.inner_text_pattern = "^hello";
  selector.must_be_visible = false;
  RunLaxElementCheck(selector, true);
  RunStrictElementCheck(selector, false);
  selector.must_be_visible = true;
  RunLaxElementCheck(selector, true);
  RunStrictElementCheck(selector, false);

  // Matches one visible, one invisible element
  selector.inner_text_pattern = "world$";
  selector.must_be_visible = false;
  RunLaxElementCheck(selector, true);
  RunStrictElementCheck(selector, false);
  selector.must_be_visible = true;
  RunLaxElementCheck(selector, true);
  RunStrictElementCheck(selector, true);

  // Inner text conditions are applied before looking for the pseudo-type.
  selector.pseudo_type = PseudoType::BEFORE;
  selector.inner_text_pattern = "world";
  selector.must_be_visible = false;
  RunLaxElementCheck(selector, true);
  selector.inner_text_pattern = "before";  // matches :before content
  RunLaxElementCheck(selector, false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ValueCondition) {
  // One match
  RunLaxElementCheck(Selector({"#input1"}).MatchingValue("helloworld1"), true);
  RunStrictElementCheck(Selector({"#input1"}).MatchingValue("helloworld1"),
                        true);

  // No matches
  RunLaxElementCheck(Selector({"#input2"}).MatchingValue("doesnotmatch"),
                     false);
  RunStrictElementCheck(Selector({"#input2"}).MatchingValue("doesnotmatch"),
                        false);

  // Multiple matches
  RunLaxElementCheck(Selector({"#input1,#input2"}).MatchingValue("^hello"),
                     true);
  RunStrictElementCheck(Selector({"#input1,#input2"}).MatchingValue("^hello"),
                        false);

  // Multiple selector matches, one value match
  RunLaxElementCheck(Selector({"#input1,#input2"}).MatchingValue("helloworld1"),
                     true);
  RunStrictElementCheck(
      Selector({"#input1,#input2"}).MatchingValue("helloworld1"), true);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       ConcurrentElementsVisibilityCheck) {
  std::vector<Selector> selectors;
  std::vector<bool> results;

  Selector a_selector;
  a_selector.must_be_visible = true;
  a_selector.selectors.emplace_back("#button");
  selectors.emplace_back(a_selector);
  results.emplace_back(true);

  a_selector.selectors.emplace_back("#whatever");
  selectors.emplace_back(a_selector);
  results.emplace_back(false);

  // IFrame.
  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#iframe");
  a_selector.selectors.emplace_back("#button");
  selectors.emplace_back(a_selector);
  results.emplace_back(true);

  a_selector.selectors.emplace_back("#whatever");
  selectors.emplace_back(a_selector);
  results.emplace_back(false);

  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#iframe");
  a_selector.selectors.emplace_back("[name=name]");
  selectors.emplace_back(a_selector);
  results.emplace_back(true);

  // Shadow DOM.
  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#iframe");
  a_selector.selectors.emplace_back("#shadowsection");
  a_selector.selectors.emplace_back("#shadowbutton");
  selectors.emplace_back(a_selector);
  results.emplace_back(true);

  a_selector.selectors.emplace_back("#whatever");
  selectors.emplace_back(a_selector);
  results.emplace_back(false);

  // IFrame inside IFrame.
  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#iframe");
  a_selector.selectors.emplace_back("#iframe");
  a_selector.selectors.emplace_back("#button");
  selectors.emplace_back(a_selector);
  results.emplace_back(true);

  a_selector.selectors.emplace_back("#whatever");
  selectors.emplace_back(a_selector);
  results.emplace_back(false);

  // Hidden element.
  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#hidden");
  selectors.emplace_back(a_selector);
  results.emplace_back(false);

  RunElementChecks(/* strict= */ false, selectors, results);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ClickElement) {
  Selector selector;
  selector.selectors.emplace_back("#button");
  ClickElement(selector);

  WaitForElementRemove(selector);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       ClickElementInIFrame) {
  Selector selector;
  selector.selectors.emplace_back("#iframe");
  selector.selectors.emplace_back("#shadowsection");
  selector.selectors.emplace_back("#shadowbutton");
  ClickElement(selector);

  selector.selectors.clear();
  selector.selectors.emplace_back("#iframe");
  selector.selectors.emplace_back("#button");
  WaitForElementRemove(selector);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, TapElement) {
  Selector selector;
  selector.selectors.emplace_back("#touch_area_two");
  TapElement(selector);
  WaitForElementRemove(selector);

  selector.selectors.clear();
  selector.selectors.emplace_back("#touch_area_one");
  TapElement(selector);
  WaitForElementRemove(selector);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, TapElementMovingOutOfView) {
  Selector selector;
  selector.selectors.emplace_back("#touch_area_three");
  TapElement(selector);
  WaitForElementRemove(selector);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, TapElementAfterPageIsIdle) {
  // Set a very long timeout to make sure either the page is idle or the test
  // timeout.
  WaitTillPageIsIdle(base::TimeDelta::FromHours(1));

  Selector selector;
  selector.selectors.emplace_back("#touch_area_one");
  TapElement(selector);

  WaitForElementRemove(selector);
}

// TODO(crbug.com/920948) Disabled for strong flakiness.
IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, DISABLED_TapElementInIFrame) {
  Selector selector;
  selector.selectors.emplace_back("#iframe");
  selector.selectors.emplace_back("#touch_area");
  TapElement(selector);

  WaitForElementRemove(selector);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ClickPseudoElement) {
  const std::string javascript = R"(
    document.querySelector("#terms-and-conditions").checked;
  )";
  EXPECT_FALSE(content::EvalJs(shell(), javascript).ExtractBool());
  Selector selector({R"(label[for="terms-and-conditions"])"},
                    PseudoType::BEFORE);
  ClickElement(selector);
  EXPECT_TRUE(content::EvalJs(shell(), javascript).ExtractBool());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, FindElement) {
  Selector selector;
  selector.selectors.emplace_back("#button");
  FindElementAndCheck(selector, 0, true);
  selector.must_be_visible = true;
  FindElementAndCheck(selector, 0, true);

  // IFrame.
  selector.selectors.clear();
  selector.selectors.emplace_back("#iframe");
  selector.selectors.emplace_back("#button");
  selector.must_be_visible = false;
  FindElementAndCheck(selector, 0, false);
  selector.must_be_visible = true;
  FindElementAndCheck(selector, 0, false);

  selector.selectors.clear();
  selector.selectors.emplace_back("#iframe");
  selector.selectors.emplace_back("[name=name]");
  selector.must_be_visible = false;
  FindElementAndCheck(selector, 0, false);
  selector.must_be_visible = true;
  FindElementAndCheck(selector, 0, false);

  // IFrame inside IFrame.
  selector.selectors.clear();
  selector.selectors.emplace_back("#iframe");
  selector.selectors.emplace_back("#iframe");
  selector.selectors.emplace_back("#button");
  selector.must_be_visible = false;
  FindElementAndCheck(selector, 1, false);
  selector.must_be_visible = true;
  FindElementAndCheck(selector, 1, false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, FindElementNotFound) {
  FindElementExpectEmptyResult(Selector({"#notfound"}));
  FindElementExpectEmptyResult(Selector({"#hidden"}).MustBeVisible());
  FindElementExpectEmptyResult(Selector({"#iframe", "#iframe", "#notfound"}));
  FindElementExpectEmptyResult(
      Selector({"#iframe", "#iframe", "#hidden"}).MustBeVisible());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, FindElementErrorStatus) {
  ClientStatus status;

  FindElement(
      Selector(ElementReferenceProto::default_instance()).MustBeVisible(),
      &status, nullptr);
  EXPECT_EQ(INVALID_SELECTOR, status.proto_status());

  FindElement(Selector({"#doesnotexist"}).MustBeVisible(), &status, nullptr);
  EXPECT_EQ(ELEMENT_RESOLUTION_FAILED, status.proto_status());

  FindElement(Selector({"div"}), &status, nullptr);
  EXPECT_EQ(TOO_MANY_ELEMENTS, status.proto_status());

  FindElement(Selector({"div"}).MustBeVisible(), &status, nullptr);
  EXPECT_EQ(TOO_MANY_ELEMENTS, status.proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, FocusElement) {
  Selector selector;
  selector.selectors.emplace_back("#iframe");
  selector.selectors.emplace_back("#focus");

  // The element is not visible initially.
  const std::string checkNotVisibleScript = R"(
      let iframe = document.querySelector("#iframe");
      let div = iframe.contentDocument.querySelector("#focus");
      let iframeRect = iframe.getBoundingClientRect();
      let divRect = div.getBoundingClientRect();
      iframeRect.y + divRect.y > window.innerHeight;
  )";
  EXPECT_EQ(true, content::EvalJs(shell(), checkNotVisibleScript));
  FocusElement(selector);

  // Verify that the scroll moved the div in the iframe into view.
  const std::string checkVisibleScript = R"(
    const scrollTimeoutMs = 500;
    var timer = null;

    function check() {
      let iframe = document.querySelector("#iframe");
      let div = iframe.contentDocument.querySelector("#focus");
      let iframeRect = iframe.getBoundingClientRect();
      let divRect = div.getBoundingClientRect();
      return iframeRect.y + divRect.y < window.innerHeight;
    }
    function onScrollDone() {
      window.removeEventListener("scroll", onScroll);
      domAutomationController.send(check());
    }
    function onScroll(e) {
      if (timer != null) {
        clearTimeout(timer);
      }
      timer = setTimeout(onScrollDone, scrollTimeoutMs);
    }
    if (check()) {
      // Scrolling finished before this script started. Just return the result.
      domAutomationController.send(true);
    } else {
      window.addEventListener("scroll", onScroll);
    }
  )";
  EXPECT_EQ(true, content::EvalJsWithManualReply(shell(), checkVisibleScript));
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, SelectOption) {
  Selector selector;
  selector.selectors.emplace_back("#select");
  EXPECT_EQ(OPTION_VALUE_NOT_FOUND,
            SelectOption(selector, "incorrect_label").proto_status());
  EXPECT_EQ(ACTION_APPLIED, SelectOption(selector, "two").proto_status());

  const std::string javascript = R"(
    let select = document.querySelector("#select");
    select.options[select.selectedIndex].label;
  )";
  EXPECT_EQ("Two", content::EvalJs(shell(), javascript));

  EXPECT_EQ(ACTION_APPLIED, SelectOption(selector, "one").proto_status());
  EXPECT_EQ("One", content::EvalJs(shell(), javascript));

  selector.selectors.clear();
  selector.selectors.emplace_back("#incorrect_selector");
  EXPECT_EQ(ELEMENT_RESOLUTION_FAILED,
            SelectOption(selector, "two").proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, SelectOptionInIframe) {
  Selector selector;
  selector.selectors.emplace_back("#iframe");
  selector.selectors.emplace_back("select[name=state]");
  EXPECT_EQ(ACTION_APPLIED, SelectOption(selector, "NY").proto_status());

  const std::string javascript = R"(
    let iframe = document.querySelector("iframe").contentDocument;
    let select = iframe.querySelector("select[name=state]");
    select.options[select.selectedIndex].label;
  )";
  EXPECT_EQ("NY", content::EvalJs(shell(), javascript));
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, GetOuterHtml) {
  Selector selector;
  selector.selectors.emplace_back("#testOuterHtml");
  std::string html;
  ASSERT_EQ(ACTION_APPLIED, GetOuterHtml(selector, &html).proto_status());
  EXPECT_EQ(
      R"(<div id="testOuterHtml"><span>Span</span><p>Paragraph</p></div>)",
      html);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, GetAndSetFieldValue) {
  std::vector<Selector> selectors;
  std::vector<std::string> expected_values;

  Selector a_selector;
  a_selector.selectors.emplace_back("#input1");
  selectors.emplace_back(a_selector);
  expected_values.emplace_back("helloworld1");
  GetFieldsValue(selectors, expected_values);

  EXPECT_EQ(ACTION_APPLIED,
            SetFieldValue(a_selector, "foo", /* simulate_key_presses= */ false)
                .proto_status());
  expected_values.clear();
  expected_values.emplace_back("foo");
  GetFieldsValue(selectors, expected_values);

  selectors.clear();
  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#uppercase_input");
  selectors.emplace_back(a_selector);
  EXPECT_EQ(ACTION_APPLIED,
            SetFieldValue(a_selector, /* Zürich */ "Z\xc3\xbcrich",
                          /* simulate_key_presses= */ true)
                .proto_status());
  expected_values.clear();
  expected_values.emplace_back(/* ZÜRICH */ "Z\xc3\x9cRICH");
  GetFieldsValue(selectors, expected_values);

  selectors.clear();
  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#invalid_selector");
  selectors.emplace_back(a_selector);
  expected_values.clear();
  expected_values.emplace_back("");
  GetFieldsValue(selectors, expected_values);

  EXPECT_EQ(
      ELEMENT_RESOLUTION_FAILED,
      SetFieldValue(a_selector, "foobar", /* simulate_key_presses= */ false)
          .proto_status());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, SendKeyboardInput) {
  auto input = UTF8ToUnicode("Zürich");
  std::string expected_output = "Zürich";

  std::vector<Selector> selectors;
  Selector a_selector;
  a_selector.selectors.emplace_back("#input6");
  selectors.emplace_back(a_selector);
  EXPECT_EQ(ACTION_APPLIED,
            SendKeyboardInput(a_selector, input).proto_status());
  GetFieldsValue(selectors, {expected_output});
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       SendKeyboardInputSetsKeyProperty) {
  auto input = UTF8ToUnicode("Zürich\r");
  std::string expected_output = "ZürichEnter";

  std::vector<Selector> selectors;
  Selector a_selector;
  a_selector.selectors.emplace_back("#input_js_event_listener");
  selectors.emplace_back(a_selector);
  EXPECT_EQ(ACTION_APPLIED,
            SendKeyboardInput(a_selector, input).proto_status());
  GetFieldsValue(selectors, {expected_output});
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       SendKeyboardInputSetsKeyPropertyWithTimeout) {
  // Sends input keys to a field where JS will intercept KeyDown and
  // at index 3 it will set a timeout whick inserts an space after the
  // timeout. If key press delay is enabled, it should handle this
  // correctly.
  auto input = UTF8ToUnicode("012345");
  std::string expected_output = "012 345";

  std::vector<Selector> selectors;
  Selector a_selector;
  a_selector.selectors.emplace_back("#input_js_event_with_timeout");
  selectors.emplace_back(a_selector);
  EXPECT_EQ(ACTION_APPLIED,
            SendKeyboardInput(a_selector, input, /*delay_in_milli*/ 100)
                .proto_status());
  GetFieldsValue(selectors, {expected_output});
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, SetAttribute) {
  Selector selector;
  std::vector<std::string> attribute;

  selector.selectors.emplace_back("#full_height_section");
  attribute.emplace_back("style");
  attribute.emplace_back("backgroundColor");
  std::string value = "red";

  EXPECT_EQ(ACTION_APPLIED,
            SetAttribute(selector, attribute, value).proto_status());
  const std::string javascript = R"(
    document.querySelector("#full_height_section").style.backgroundColor;
  )";
  EXPECT_EQ(value, content::EvalJs(shell(), javascript));
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ConcurrentGetFieldsValue) {
  std::vector<Selector> selectors;
  std::vector<std::string> expected_values;

  Selector a_selector;
  a_selector.selectors.emplace_back("#input1");
  selectors.emplace_back(a_selector);
  expected_values.emplace_back("helloworld1");

  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#input2");
  selectors.emplace_back(a_selector);
  expected_values.emplace_back("helloworld2");

  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#input3");
  selectors.emplace_back(a_selector);
  expected_values.emplace_back("helloworld3");

  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#input4");
  selectors.emplace_back(a_selector);
  expected_values.emplace_back("helloworld4");

  a_selector.selectors.clear();
  a_selector.selectors.emplace_back("#input5");
  selectors.emplace_back(a_selector);
  expected_values.emplace_back("helloworld5");

  GetFieldsValue(selectors, expected_values);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, NavigateToUrl) {
  EXPECT_EQ(kTargetWebsitePath,
            shell()->web_contents()->GetLastCommittedURL().path());
  web_controller_->LoadURL(GURL(url::kAboutBlankURL));
  WaitForLoadStop(shell()->web_contents());
  EXPECT_EQ(url::kAboutBlankURL,
            shell()->web_contents()->GetLastCommittedURL().spec());
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, HighlightElement) {
  Selector selector;
  selector.selectors.emplace_back("#select");

  const std::string javascript = R"(
    let select = document.querySelector("#select");
    select.style.boxShadow;
  )";
  EXPECT_EQ("", content::EvalJs(shell(), javascript));
  EXPECT_EQ(ACTION_APPLIED, HighlightElement(selector).proto_status());
  // We only make sure that the element has a non-empty boxShadow style without
  // requiring an exact string match.
  EXPECT_NE("", content::EvalJs(shell(), javascript));
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, GetAndSetCookie) {
  EXPECT_FALSE(HasCookie());
  EXPECT_TRUE(SetCookie());
}

}  // namespace
