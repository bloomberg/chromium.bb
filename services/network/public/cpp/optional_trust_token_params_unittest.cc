// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/optional_trust_token_params.h"

#include "base/optional.h"
#include "base/test/gtest_util.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/mojom/origin_mojom_traits.h"
#include "url/origin.h"

namespace network {

namespace {
// For tests that require a populated OptionalTrustTokenParams, use this helper
// to avoid needing to update several tests every time the format of
// mojom::TrustTokenParams (and, consequently, the signature to its constructor)
// changes.
OptionalTrustTokenParams NonemptyTrustTokenParams() {
  return mojom::TrustTokenParams(
      mojom::TrustTokenOperationType::kRedemption,
      mojom::TrustTokenRefreshPolicy::kRefresh,
      mojom::TrustTokenSignRequestData::kInclude,
      /*include_timestamp_header=*/true,
      url::Origin::Create(GURL("https://issuer.com")),
      std::vector<std::string>{"some_header", "another_header"});
}
}  // namespace

TEST(OptionalTrustTokenParams, Empty) {
  EXPECT_EQ(OptionalTrustTokenParams(), OptionalTrustTokenParams());
  EXPECT_FALSE(OptionalTrustTokenParams().has_value());
  EXPECT_FALSE(OptionalTrustTokenParams(base::nullopt).has_value());

  EXPECT_EQ(OptionalTrustTokenParams(base::nullopt),
            OptionalTrustTokenParams());
  EXPECT_NE(OptionalTrustTokenParams(), NonemptyTrustTokenParams());
}

TEST(OptionalTrustTokenParams, CopyAndMove) {
  {
    OptionalTrustTokenParams in = NonemptyTrustTokenParams();
    EXPECT_TRUE(in.has_value());
    EXPECT_EQ(in, OptionalTrustTokenParams(in));

    OptionalTrustTokenParams assigned = in;
    EXPECT_EQ(in, assigned);

    OptionalTrustTokenParams moved(std::move(assigned));
    EXPECT_EQ(in, moved);

    OptionalTrustTokenParams move_assigned = std::move(moved);
    EXPECT_EQ(in, move_assigned);
  }

  {
    const OptionalTrustTokenParams const_in = NonemptyTrustTokenParams();
    EXPECT_TRUE(const_in.has_value());
    EXPECT_EQ(const_in, OptionalTrustTokenParams(const_in));

    const OptionalTrustTokenParams assigned = const_in;
    EXPECT_EQ(const_in, assigned);

    OptionalTrustTokenParams moved(std::move(assigned));
    EXPECT_EQ(const_in, moved);

    OptionalTrustTokenParams move_assigned = std::move(moved);
    EXPECT_EQ(const_in, move_assigned);
  }
}

TEST(OptionalTrustTokenParams, Dereference) {
  OptionalTrustTokenParams in = NonemptyTrustTokenParams();
  EXPECT_EQ(in->type, mojom::TrustTokenOperationType::kRedemption);
  EXPECT_EQ(in.as_ptr()->type, mojom::TrustTokenOperationType::kRedemption);
  EXPECT_EQ(in.value().type, mojom::TrustTokenOperationType::kRedemption);
}

TEST(OptionalTrustTokenParams, DereferenceEmpty) {
  OptionalTrustTokenParams in = base::nullopt;
  EXPECT_CHECK_DEATH(ignore_result(in->type));
  EXPECT_CHECK_DEATH(ignore_result(in.value()));
  EXPECT_EQ(in.as_ptr(), mojom::TrustTokenParamsPtr());
}

}  // namespace network
