// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_SHOPPING_SERVICE_TEST_BASE_H_
#define COMPONENTS_COMMERCE_CORE_SHOPPING_SERVICE_TEST_BASE_H_

#include <memory>
#include <vector>

#include "base/values.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/web_wrapper.h"
#include "components/optimization_guide/core/new_optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using optimization_guide::OptimizationGuideDecision;
using optimization_guide::OptimizationGuideDecisionCallback;
using optimization_guide::OptimizationMetadata;
using optimization_guide::proto::OptimizationType;

class TestingPrefServiceSimple;

namespace bookmarks {
class BookmarkModel;
}

namespace commerce {

// A mock Optimization Guide decider that allows us to specify the response for
// a particular URL.
class MockOptGuideDecider
    : public optimization_guide::NewOptimizationGuideDecider {
 public:
  MockOptGuideDecider();
  MockOptGuideDecider(const MockOptGuideDecider&) = delete;
  MockOptGuideDecider operator=(const MockOptGuideDecider&) = delete;
  ~MockOptGuideDecider() override;

  void RegisterOptimizationTypes(
      const std::vector<OptimizationType>& optimization_types) override;

  void CanApplyOptimization(
      const GURL& url,
      OptimizationType optimization_type,
      OptimizationGuideDecisionCallback callback) override;

  OptimizationGuideDecision CanApplyOptimization(
      const GURL& url,
      OptimizationType optimization_type,
      OptimizationMetadata* optimization_metadata) override;

  void SetResponse(const GURL& url,
                   const OptimizationType type,
                   const OptimizationGuideDecision decision,
                   const OptimizationMetadata& data);

  OptimizationMetadata BuildPriceTrackingResponse(
      const std::string& title,
      const std::string& image_url,
      const uint64_t offer_id,
      const uint64_t product_cluster_id,
      const std::string& country_code);

  OptimizationMetadata BuildMerchantTrustResponse(
      const float star_rating,
      const uint32_t count_rating,
      const std::string& details_page_url,
      const bool has_return_policy,
      const bool contains_sensitive_content);

 private:
  absl::optional<GURL> response_url_;
  absl::optional<OptimizationType> optimization_type_;
  absl::optional<OptimizationGuideDecision> optimization_decision_;
  absl::optional<OptimizationMetadata> optimization_data_;
};

// A mock WebWrapper where returned values can be manually set.
class MockWebWrapper : public WebWrapper {
 public:
  MockWebWrapper(const GURL& last_committed_url, bool is_off_the_record);
  MockWebWrapper(const MockWebWrapper&) = delete;
  MockWebWrapper operator=(const MockWebWrapper&) = delete;
  ~MockWebWrapper() override;

  const GURL& GetLastCommittedURL() override;

  bool IsOffTheRecord() override;

  void RunJavascript(
      const std::u16string& script,
      base::OnceCallback<void(const base::Value)> callback) override;

  // Set the result of some javascript execution. This object does not take
  // ownership of the provided pointer.
  void SetMockJavaScriptResult(base::Value* result);

 private:
  GURL last_committed_url_;
  bool is_off_the_record_;

  raw_ptr<base::Value> mock_js_result_;
};

class ShoppingServiceTestBase : public testing::Test {
 public:
  ShoppingServiceTestBase();
  ShoppingServiceTestBase(const ShoppingServiceTestBase&) = delete;
  ShoppingServiceTestBase operator=(const ShoppingServiceTestBase&) = delete;
  ~ShoppingServiceTestBase() override;

  void TestBody() override;

  // A direct proxies to the same methods in the ShoppingService class.
  void DidNavigatePrimaryMainFrame(WebWrapper* web);
  void DidNavigateAway(WebWrapper* web, const GURL& url);
  void WebWrapperDestroyed(WebWrapper* web);

  // Get the count of the number of tabs a particular URL is open in from the
  // product info cache.
  int GetProductInfoCacheOpenURLCount(const GURL& url);

  // Get the item in the product info cache if it exists.
  const ProductInfo* GetFromProductInfoCache(const GURL& url);

 protected:
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;

  std::unique_ptr<MockOptGuideDecider> opt_guide_;

  std::unique_ptr<ShoppingService> shopping_service_;

  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_SHOPPING_SERVICE_TEST_BASE_H_
