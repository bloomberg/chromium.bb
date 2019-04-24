// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/contextual/contextual_suggestions_fetch.h"

#include <map>
#include <string>

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/ntp_snippets/contextual/contextual_suggestions_features.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations_http_header_provider.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_suggestions {

namespace {

TEST(ContextualSuggestionsFetch, GetFetchEndpoint_Default) {
  EXPECT_EQ("https://www.google.com/httpservice/web/ExploreService/GetPivots/",
            ContextualSuggestionsFetch::GetFetchEndpoint());
}

TEST(ContextualSuggestionsFetch, GetFetchEndpoint_CommandLine_MissingValue) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII("contextual-suggestions-fetch-endpoint", "");
  EXPECT_EQ("https://www.google.com/httpservice/web/ExploreService/GetPivots/",
            ContextualSuggestionsFetch::GetFetchEndpoint());
}

TEST(ContextualSuggestionsFetch, GetFetchEndpoint_CommandLine_NonHTTPS) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII("contextual-suggestions-fetch-endpoint",
                                  "http://test.com");
  EXPECT_EQ("https://www.google.com/httpservice/web/ExploreService/GetPivots/",
            ContextualSuggestionsFetch::GetFetchEndpoint());
}

TEST(ContextualSuggestionsFetch, GetFetchEndpoint_CommandLine_ProperEndpoint) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII("contextual-suggestions-fetch-endpoint",
                                  "https://test.com");
  EXPECT_EQ("https://test.com/httpservice/web/ExploreService/GetPivots/",
            ContextualSuggestionsFetch::GetFetchEndpoint());
}

TEST(ContextualSuggestionsFetch, GetFetchEndpoint_Feature_NoParameter) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kContextualSuggestionsButton);
  EXPECT_EQ("https://www.google.com/httpservice/web/ExploreService/GetPivots/",
            ContextualSuggestionsFetch::GetFetchEndpoint());
}

TEST(ContextualSuggestionsFetch, GetFetchEndpoint_Feature_EmptyParameter) {
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> parameters;
  parameters["contextual-suggestions-fetch-endpoint"] = "";
  feature_list.InitAndEnableFeatureWithParameters(kContextualSuggestionsButton,
                                                  parameters);
  EXPECT_EQ("https://www.google.com/httpservice/web/ExploreService/GetPivots/",
            ContextualSuggestionsFetch::GetFetchEndpoint());
}

TEST(ContextualSuggestionsFetch, GetFetchEndpoint_Feature_NonHTTPS) {
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> parameters;
  parameters["contextual-suggestions-fetch-endpoint"] = "http://test.com";
  feature_list.InitAndEnableFeatureWithParameters(kContextualSuggestionsButton,
                                                  parameters);
  EXPECT_EQ("https://www.google.com/httpservice/web/ExploreService/GetPivots/",
            ContextualSuggestionsFetch::GetFetchEndpoint());
}

TEST(ContextualSuggestionsFetch, GetFetchEndpoint_Feature_WithParameter) {
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> parameters;
  parameters["contextual-suggestions-fetch-endpoint"] = "https://test.com";
  feature_list.InitAndEnableFeatureWithParameters(kContextualSuggestionsButton,
                                                  parameters);
  EXPECT_EQ("https://test.com/httpservice/web/ExploreService/GetPivots/",
            ContextualSuggestionsFetch::GetFetchEndpoint());
}

TEST(ContextualSuggestionsFetch, MakeResourceRequest_VariationsHeader) {
  variations::VariationsHttpHeaderProvider::GetInstance()->ResetForTesting();
  scoped_refptr<base::TestSimpleTaskRunner> test_task_runner(
      new base::TestSimpleTaskRunner());
  base::ThreadTaskRunnerHandle handle(test_task_runner);
  EXPECT_EQ(variations::VariationsHttpHeaderProvider::ForceIdsResult::SUCCESS,
            variations::VariationsHttpHeaderProvider::GetInstance()
                ->ForceVariationIds({"12345"}, ""));

  ContextualSuggestionsFetch fetch(GURL("http://test.com"), "en-US");
  std::unique_ptr<network::ResourceRequest> resource_request =
      fetch.MakeResourceRequestForTesting();

  EXPECT_TRUE(variations::HasVariationsHeader(*resource_request));
}

TEST(ContextualSuggestionsFetch,
     MakeResourceRequest_VariationsHeader_NonGoogleEndpoint) {
  variations::VariationsHttpHeaderProvider::GetInstance()->ResetForTesting();
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII("contextual-suggestions-fetch-endpoint",
                                  "https://nongoogleendpoint.com");
  scoped_refptr<base::TestSimpleTaskRunner> test_task_runner(
      new base::TestSimpleTaskRunner());
  base::ThreadTaskRunnerHandle handle(test_task_runner);
  EXPECT_EQ(variations::VariationsHttpHeaderProvider::ForceIdsResult::SUCCESS,
            variations::VariationsHttpHeaderProvider::GetInstance()
                ->ForceVariationIds({"12345"}, ""));

  ContextualSuggestionsFetch fetch(GURL("http://test.com"), "en-US");
  std::unique_ptr<network::ResourceRequest> resource_request =
      fetch.MakeResourceRequestForTesting();

  EXPECT_FALSE(variations::HasVariationsHeader(*resource_request));
}

}  // namespace

}  // namespace contextual_suggestions
