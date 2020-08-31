// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "url/gurl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/public/tab.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/stub_autofill_provider.h"
#include "weblayer/test/test_navigation_observer.h"

namespace weblayer {

namespace {

// Navigates to |url| in |tab| and waits for |event| to occur.
void NavigateAndWaitForEvent(const GURL& url,
                             Tab* tab,
                             TestNavigationObserver::NavigationEvent event) {
  TestNavigationObserver test_observer(url, event, tab);
  tab->GetNavigationController()->Navigate(url);
  test_observer.Wait();
}

}  // namespace

void NavigateAndWaitForCompletion(const GURL& url, Shell* shell) {
  NavigateAndWaitForEvent(url, shell->tab(),
                          TestNavigationObserver::NavigationEvent::kCompletion);
}

void NavigateAndWaitForCompletion(const GURL& url, Tab* tab) {
  NavigateAndWaitForEvent(url, tab,
                          TestNavigationObserver::NavigationEvent::kCompletion);
}

void NavigateAndWaitForFailure(const GURL& url, Shell* shell) {
  NavigateAndWaitForEvent(url, shell->tab(),
                          TestNavigationObserver::NavigationEvent::kFailure);
}

void NavigateAndWaitForStart(const GURL& url, Tab* tab) {
  NavigateAndWaitForEvent(url, tab,
                          TestNavigationObserver::NavigationEvent::kStart);
}

base::Value ExecuteScript(Shell* shell,
                          const std::string& script,
                          bool use_separate_isolate) {
  base::Value final_result;
  base::RunLoop run_loop;
  shell->tab()->ExecuteScript(
      base::ASCIIToUTF16(script), use_separate_isolate,
      base::BindLambdaForTesting(
          [&run_loop, &final_result](base::Value result) {
            final_result = std::move(result);
            run_loop.Quit();
          }));
  run_loop.Run();
  return final_result;
}

void ExecuteScriptWithUserGesture(Shell* shell, const std::string& script) {
  TabImpl* tab_impl = static_cast<TabImpl*>(shell->tab());
  tab_impl->ExecuteScriptWithUserGestureForTests(base::ASCIIToUTF16(script));
}

const base::string16& GetTitle(Shell* shell) {
  TabImpl* tab_impl = static_cast<TabImpl*>(shell->tab());

  return tab_impl->web_contents()->GetTitle();
}

void InitializeAutofillWithEventForwarding(
    Shell* shell,
    const base::RepeatingCallback<void(const autofill::FormData&)>&
        on_received_form_data) {
  TabImpl* tab_impl = static_cast<TabImpl*>(shell->tab());

  tab_impl->InitializeAutofillForTests(
      std::make_unique<StubAutofillProvider>(on_received_form_data));
}

}  // namespace weblayer
