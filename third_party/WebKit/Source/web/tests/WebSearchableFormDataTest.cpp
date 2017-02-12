/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/web/WebSearchableFormData.h"

#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebDocument.h"
#include "public/web/WebFrame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/tests/FrameTestHelpers.h"

namespace blink {

namespace {

void registerMockedURLLoadFromBaseURL(const std::string& baseURL,
                                      const std::string& fileName) {
  URLTestHelpers::registerMockedURLLoadFromBase(WebString::fromUTF8(baseURL),
                                                testing::webTestDataPath(),
                                                WebString::fromUTF8(fileName));
}

class WebSearchableFormDataTest : public ::testing::Test {
 protected:
  WebSearchableFormDataTest() {}

  ~WebSearchableFormDataTest() override {
    Platform::current()
        ->getURLLoaderMockFactory()
        ->unregisterAllURLsAndClearMemoryCache();
  }

  FrameTestHelpers::WebViewHelper m_webViewHelper;
};

}  // namespace
TEST_F(WebSearchableFormDataTest, HttpSearchString) {
  std::string baseURL("http://www.test.com/");
  registerMockedURLLoadFromBaseURL(baseURL, "search_form_http.html");
  WebView* webView =
      m_webViewHelper.initializeAndLoad(baseURL + "search_form_http.html");

  WebVector<WebFormElement> forms;
  webView->mainFrame()->document().forms(forms);

  EXPECT_EQ(forms.size(), 1U);

  WebSearchableFormData searchableFormData(forms[0]);
  EXPECT_EQ("http://www.mock.url/search?hl=en&q={searchTerms}&btnM=Mock+Search",
            searchableFormData.url().string());
}

TEST_F(WebSearchableFormDataTest, HttpsSearchString) {
  std::string baseURL("https://www.test.com/");
  registerMockedURLLoadFromBaseURL(baseURL, "search_form_https.html");
  WebView* webView =
      m_webViewHelper.initializeAndLoad(baseURL + "search_form_https.html");

  WebVector<WebFormElement> forms;
  webView->mainFrame()->document().forms(forms);

  EXPECT_EQ(forms.size(), 1U);

  WebSearchableFormData searchableFormData(forms[0]);
  EXPECT_EQ(
      "https://www.mock.url/search?hl=en&q={searchTerms}&btnM=Mock+Search",
      searchableFormData.url().string());
}

}  // namespace blink
