// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_INTEREST_GROUP_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_INTEREST_GROUP_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

// Interest group used by FLEDGE auctions. Typemapped to
// blink::mojom::InterestGroup, primarily so the typemap can include validity
// checks on the origins of the provided URLs.
//
// All URLs and origins must use https, and same origin to `owner`.
//
// https://github.com/WICG/turtledove/blob/main/FLEDGE.md#11-joining-interest-groups
struct BLINK_COMMON_EXPORT InterestGroup {
  // An advertisement to display for an interest group. Typemapped to
  // blink::mojom::InterestGroupAd.
  // https://github.com/WICG/turtledove/blob/main/FLEDGE.md#12-interest-group-attributes
  struct BLINK_COMMON_EXPORT Ad {
    Ad();
    Ad(GURL render_url, absl::optional<std::string> metadata);
    ~Ad();

    // Returns the approximate size of the contents of this InterestGroup::Ad,
    // in bytes.
    size_t EstimateSize() const;

    // Must use https.
    GURL render_url;
    // Opaque JSON data, passed as an object to auction worklet.
    absl::optional<std::string> metadata;

    // Only used in tests, but provided as an operator instead of as
    // IsEqualForTesting() to make it easier to implement InterestGroup's
    // IsEqualForTesting().
    bool operator==(const Ad& other) const;
  };

  InterestGroup();

  // Constructor takes arguments by value. They're unlikely to be independently
  // useful at the point of construction, so caller can std::move() them when
  // invoking the constructor.
  InterestGroup(
      base::Time expiry,
      url::Origin owner,
      std::string name,
      double priority,
      absl::optional<GURL> bidding_url,
      absl::optional<GURL> bidding_wasm_helper_url,
      absl::optional<GURL> daily_update_url,
      absl::optional<GURL> trusted_bidding_signals_url,
      absl::optional<std::vector<std::string>> trusted_bidding_signals_keys,
      absl::optional<std::string> user_bidding_signals,
      absl::optional<std::vector<InterestGroup::Ad>> ads,
      absl::optional<std::vector<InterestGroup::Ad>> ad_components);

  ~InterestGroup();

  // Checks for validity. Performs same checks as IsBlinkInterestGroupValid().
  // Automatically checked when passing InterestGroups over Mojo.
  bool IsValid() const;

  // Returns the approximate size of the contents of this InterestGroup, in
  // bytes.
  size_t EstimateSize() const;

  bool IsEqualForTesting(const InterestGroup& other) const;

  base::Time expiry;
  url::Origin owner;
  std::string name;
  absl::optional<double> priority;  // Needs to be optional for updates.
  absl::optional<GURL> bidding_url;
  absl::optional<GURL> bidding_wasm_helper_url;
  absl::optional<GURL> daily_update_url;
  absl::optional<GURL> trusted_bidding_signals_url;
  absl::optional<std::vector<std::string>> trusted_bidding_signals_keys;
  absl::optional<std::string> user_bidding_signals;
  absl::optional<std::vector<InterestGroup::Ad>> ads, ad_components;

  static_assert(__LINE__ == 94, R"(
If modifying InterestGroup fields, make sure to also modify:

* IsValid(), EstimateSize(), and IsEqualForTesting() in this class
* auction_ad_interest_group.idl
* navigator_auction.cc
* interest_group_types.mojom
* validate_blink_interest_group.cc
* validate_blink_interest_group_test.cc
* interest_group_mojom_traits[.h/.cc/.test].
* bidder_worklet.cc (to pass the InterestGroup to generateBid()).

In interest_group_storage.cc, add the new field and any respective indices,
and also add a new database version and migration, and migration test.

If the new field is to be updatable via dailyUpdateUrl, also update *all* of
these:

* InterestGroupStorage::DoStoreInterestGroupUpdate()
* ParseUpdateJson in interest_group_update_manager.cc
* Update AdAuctionServiceImplTest.UpdateAllUpdatableFields

See crrev.com/c/3517534 for an example (adding the priority field), and also
remember to update bidder_worklet.cc too.
)");
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_INTEREST_GROUP_H_
