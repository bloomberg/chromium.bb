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

#include <string>

#include "core/frame/FrameTestHelpers.h"
#include "core/frame/WebLocalFrameBase.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebDocument.h"
#include "public/web/WebFrame.h"
#include "public/web/WebLocalFrame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

void RegisterMockedURLLoadFromBaseURL(const std::string& base_url,
                                      const std::string& file_name) {
  URLTestHelpers::RegisterMockedURLLoadFromBase(WebString::FromUTF8(base_url),
                                                testing::CoreTestDataPath(),
                                                WebString::FromUTF8(file_name));
}

class WebSearchableFormDataTest : public ::testing::Test {
 protected:
  WebSearchableFormDataTest() {}

  ~WebSearchableFormDataTest() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  FrameTestHelpers::WebViewHelper web_view_helper_;
};

}  // namespace
TEST_F(WebSearchableFormDataTest, HttpSearchString) {
  std::string base_url("http://www.test.com/");
  RegisterMockedURLLoadFromBaseURL(base_url, "search_form_http.html");
  WebViewBase* web_view =
      web_view_helper_.InitializeAndLoad(base_url + "search_form_http.html");

  WebVector<WebFormElement> forms;
  web_view->MainFrameImpl()->GetDocument().Forms(forms);

  EXPECT_EQ(forms.size(), 1U);

  WebSearchableFormData searchable_form_data(forms[0]);
  EXPECT_EQ("http://www.mock.url/search?hl=en&q={searchTerms}&btnM=Mock+Search",
            searchable_form_data.Url().GetString());
}

TEST_F(WebSearchableFormDataTest, HttpsSearchString) {
  std::string base_url("https://www.test.com/");
  RegisterMockedURLLoadFromBaseURL(base_url, "search_form_https.html");
  WebViewBase* web_view =
      web_view_helper_.InitializeAndLoad(base_url + "search_form_https.html");

  WebVector<WebFormElement> forms;
  web_view->MainFrameImpl()->GetDocument().Forms(forms);

  EXPECT_EQ(forms.size(), 1U);

  WebSearchableFormData searchable_form_data(forms[0]);
  EXPECT_EQ(
      "https://www.mock.url/search?hl=en&q={searchTerms}&btnM=Mock+Search",
      searchable_form_data.Url().GetString());
}

}  // namespace blink
