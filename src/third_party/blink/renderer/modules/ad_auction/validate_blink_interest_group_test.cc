// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ad_auction/validate_blink_interest_group.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "mojo/public/cpp/bindings/array_traits_wtf_vector.h"
#include "mojo/public/cpp/bindings/message.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/interest_group/validate_interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-blink.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

// Test fixture for testing both ValidateBlinkInterestGroup() and
// ValidateInterestGroup(), and making sure they behave the same.
class ValidateBlinkInterestGroupTest : public testing::Test {
 public:
  // Check that `blink_interest_group` is valid, if added from its owner origin.
  void ExpectInterestGroupIsValid(
      const mojom::blink::InterestGroupPtr& blink_interest_group) {
    String error_field_name;
    String error_field_value;
    String error;
    EXPECT_TRUE(ValidateBlinkInterestGroup(
        *blink_interest_group->owner, *blink_interest_group, error_field_name,
        error_field_value, error));
    EXPECT_TRUE(error_field_name.IsNull());
    EXPECT_TRUE(error_field_value.IsNull());
    EXPECT_TRUE(error.IsNull());

    mojom::InterestGroupPtr interest_group =
        ConvertBlinkInterestGroup(blink_interest_group);
    ASSERT_TRUE(interest_group);

    EXPECT_TRUE(ValidateInterestGroup(
        blink_interest_group->owner->ToUrlOrigin(), *interest_group));
  }

  // Check that `blink_interest_group` is valid, if added from `blink_origin`,
  // and returns the provided error values.
  void ExpectInterestGroupIsNotValid(
      const mojom::blink::InterestGroupPtr& blink_interest_group,
      scoped_refptr<const SecurityOrigin> blink_origin,
      const std::string& expected_error_field_name,
      const std::string& expected_error_field_value,
      const std::string& expected_error) {
    String error_field_name;
    String error_field_value;
    String error;
    EXPECT_FALSE(
        ValidateBlinkInterestGroup(*blink_origin, *blink_interest_group,
                                   error_field_name, error_field_value, error));
    EXPECT_EQ(String::FromUTF8(expected_error_field_name), error_field_name);
    EXPECT_EQ(String::FromUTF8(expected_error_field_value), error_field_value);
    EXPECT_EQ(String::FromUTF8(expected_error), error);

    mojom::InterestGroupPtr interest_group =
        ConvertBlinkInterestGroup(blink_interest_group);
    ASSERT_TRUE(interest_group);

    EXPECT_FALSE(
        ValidateInterestGroup(blink_origin->ToUrlOrigin(), *interest_group));
  }

  // Converts a mojom::blink::InterestGroupPtr to a mojom::InterestGroupPtr.
  // Based off of mojo::test::SerailizeAndDeserialize(), which can't convert
  // between blink and non-blink types.
  mojom::InterestGroupPtr ConvertBlinkInterestGroup(
      const mojom::blink::InterestGroupPtr& blink_interest_group) {
    mojo::Message message =
        mojom::blink::InterestGroup::SerializeAsMessage(&blink_interest_group);
    mojo::ScopedMessageHandle handle = message.TakeMojoMessage();
    message = mojo::Message::CreateFromMessageHandle(&handle);
    DCHECK(!message.IsNull());

    // mojo::test::SerializeAndDeserialize(blink_interest_group,
    // blink_interest_group);

    mojom::InterestGroupPtr interest_group;
    if (!mojom::InterestGroup::DeserializeFromMessage(std::move(message),
                                                      &interest_group)) {
      return nullptr;
    }
    return interest_group;
  }

  // Creates and returns a minimally populated mojom::blink::InterestGroup.
  mojom::blink::InterestGroupPtr CreateMinimalInterestGroup() {
    mojom::blink::InterestGroupPtr blink_interest_group =
        mojom::blink::InterestGroup::New();
    blink_interest_group->owner = kOrigin;
    blink_interest_group->name = kName;
    return blink_interest_group;
  }

  // Creates an interest group with all fields populated with valid values.
  mojom::blink::InterestGroupPtr CreateFullyPopulatedInterestGroup() {
    mojom::blink::InterestGroupPtr blink_interest_group =
        CreateMinimalInterestGroup();

    // Url that's allowed in every field. Populate all portions of the URL that
    // are allowed in most places.
    const KURL kAllowedUrl =
        KURL(String::FromUTF8("https://origin.test/foo?bar"));
    blink_interest_group->bidding_url = kAllowedUrl;
    blink_interest_group->update_url = kAllowedUrl;

    // `trusted_bidding_signals_url` doesn't allow query strings, unlike the
    // above ones.
    blink_interest_group->trusted_bidding_signals_url =
        KURL(String::FromUTF8("https://origin.test/foo"));

    blink_interest_group->trusted_bidding_signals_keys.emplace();
    blink_interest_group->trusted_bidding_signals_keys->push_back(
        String::FromUTF8("1"));
    blink_interest_group->trusted_bidding_signals_keys->push_back(
        String::FromUTF8("2"));
    blink_interest_group->user_bidding_signals =
        String::FromUTF8("\"This field isn't actually validated\"");

    // Add two ads. Use different URLs, with references.
    blink_interest_group->ads.emplace();
    auto mojo_ad1 = mojom::blink::InterestGroupAd::New();
    mojo_ad1->render_url =
        KURL(String::FromUTF8("https://origin.test/foo?bar#baz"));
    mojo_ad1->metadata =
        String::FromUTF8("\"This field isn't actually validated\"");
    blink_interest_group->ads->push_back(std::move(mojo_ad1));
    auto mojo_ad2 = mojom::blink::InterestGroupAd::New();
    mojo_ad2->render_url =
        KURL(String::FromUTF8("https://origin.test/foo?bar#baz2"));
    blink_interest_group->ads->push_back(std::move(mojo_ad2));

    return blink_interest_group;
  }

 protected:
  // SecurityOrigin used as the owner in most tests.
  const scoped_refptr<const SecurityOrigin> kOrigin =
      SecurityOrigin::CreateFromString(
          String::FromUTF8("https://origin.test/"));

  const String kName = String::FromUTF8("name");
};

// Test behavior with an InterestGroup with as few fields populated as allowed.
TEST_F(ValidateBlinkInterestGroupTest, MinimallyPopulated) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  ExpectInterestGroupIsValid(blink_interest_group);
}

// Test behavior with an InterestGroup with all fields populated with valid
// values.
TEST_F(ValidateBlinkInterestGroupTest, FullyPopulated) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateFullyPopulatedInterestGroup();
  ExpectInterestGroupIsValid(blink_interest_group);
}

// Make sure that non-HTTPS origins are rejected, both as the frame origin, and
// as the owner. HTTPS frame origins with non-HTTPS owners are currently
// rejected due to origin mismatch, but once sites can add users to 3P interest
// groups, they should still be rejected for being non-HTTPS.
TEST_F(ValidateBlinkInterestGroupTest, NonHttpsOriginRejected) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  blink_interest_group->owner =
      SecurityOrigin::CreateFromString(String::FromUTF8("http://origin.test/"));
  ExpectInterestGroupIsNotValid(
      blink_interest_group, blink_interest_group->owner,
      "frame origin" /* expected_error_field_name */,
      "http://origin.test" /* expected_error_field_value */,
      "frame origin must be HTTPS." /* expected_error */);
  ExpectInterestGroupIsNotValid(
      blink_interest_group, kOrigin, "owner" /* expected_error_field_name */,
      "http://origin.test" /* expected_error_field_value */,
      "frame origin must match owner origin." /* expected_error */);

  blink_interest_group->owner =
      SecurityOrigin::CreateFromString(String::FromUTF8("data:,foo"));
  // Data URLs have opaque origins, which are mapped to the string "null".
  ExpectInterestGroupIsNotValid(
      blink_interest_group, blink_interest_group->owner,
      "frame origin" /* expected_error_field_name */,
      "null" /* expected_error_field_value */,
      "frame origin must be HTTPS." /* expected_error */);
  ExpectInterestGroupIsNotValid(
      blink_interest_group, kOrigin, "owner" /* expected_error_field_name */,
      "null" /* expected_error_field_value */,
      "frame origin must match owner origin." /* expected_error */);
}

TEST_F(ValidateBlinkInterestGroupTest, WrongOwnerRejected) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      SecurityOrigin::CreateFromString(
          String::FromUTF8("https://origin2.test/")),
      "owner" /* expected_error_field_name */,
      "https://origin.test" /* expected_error_field_value */,
      "frame origin must match owner origin." /* expected_error */);
}

// Check that `bidding_url`, `update_url`, and `trusted_bidding_signals_url`
// must be same-origin and HTTPS.
//
// Ad URLs do not have to be same origin, so they're checked in a different
// test.
TEST_F(ValidateBlinkInterestGroupTest, RejectedUrls) {
  // Strings when each field has a bad URL, copied from cc file.
  const char kBadBiddingUrlError[] =
      "biddingUrl must have the same origin as the InterestGroup owner "
      "and have no fragment identifier or embedded credentials.";
  const char kBadUpdateUrlError[] =
      "updateUrl must have the same origin as the InterestGroup owner "
      "and have no fragment identifier or embedded credentials.";
  const char kBadTrustedBiddingSignalsUrlError[] =
      "trustedBiddingSignalsUrl must have the same origin as the "
      "InterestGroup owner and have no query string, fragment identifier "
      "or embedded credentials.";

  // Nested URL schemes, like filesystem URLs, are the only cases where a URL
  // being same origin with an HTTPS origin does not imply the URL itself is
  // also HTTPS.
  const KURL kFileSystemUrl =
      KURL(String::FromUTF8("filesystem:https://origin.test/foo"));
  EXPECT_TRUE(
      kOrigin->IsSameOriginWith(SecurityOrigin::Create(kFileSystemUrl).get()));

  const KURL kRejectedUrls[] = {
      // HTTP URLs is rejected: it's both the wrong scheme, and cross-origin.
      KURL(String::FromUTF8("filesystem:http://origin.test/foo")),
      // Cross origin HTTPS URLs are rejected.
      KURL(String::FromUTF8("https://origin2.test/foo")),
      // URL with different ports are cross-origin.
      KURL(String::FromUTF8("https://origin.test:1234/")),
      // URLs with opaque origins are cross-origin.
      KURL(String::FromUTF8("data://text/html,payload")),
      // Unknown scheme.
      KURL(String::FromUTF8("unknown-scheme://foo/")),

      // filesystem URLs are rejected, even if they're same-origin with the page
      // origin.
      kFileSystemUrl,

      // URLs with user/ports are rejected.
      KURL(String::FromUTF8("https://user:pass@origin.test/")),
      // References also aren't allowed, as they aren't sent over HTTP.
      KURL(String::FromUTF8("https://origin.test/#foopy")),

      // Invalid URLs.
      KURL(String::FromUTF8("")),
      KURL(String::FromUTF8("invalid url")),
      KURL(String::FromUTF8("https://!@#$%^&*()/")),
      KURL(String::FromUTF8("https://[1::::::2]/")),
      KURL(String::FromUTF8("https://origin.test/%00")),
  };

  for (const KURL& rejected_url : kRejectedUrls) {
    SCOPED_TRACE(rejected_url.GetString());

    // Test `bidding_url`.
    mojom::blink::InterestGroupPtr blink_interest_group =
        CreateMinimalInterestGroup();
    blink_interest_group->bidding_url = rejected_url;
    ExpectInterestGroupIsNotValid(
        blink_interest_group, kOrigin,
        "biddingUrl" /* expected_error_field_name */,
        rejected_url.GetString().Utf8() /* expected_error_field_value */,
        kBadBiddingUrlError /* expected_error */);

    // Test `update_url`.
    blink_interest_group = CreateMinimalInterestGroup();
    blink_interest_group->update_url = rejected_url;
    ExpectInterestGroupIsNotValid(
        blink_interest_group, kOrigin,
        "updateUrl" /* expected_error_field_name */,
        rejected_url.GetString().Utf8() /* expected_error_field_value */,
        // expected_error
        kBadUpdateUrlError /* expected_error */);

    // Test `trusted_bidding_signals_url`.
    blink_interest_group = CreateMinimalInterestGroup();
    blink_interest_group->trusted_bidding_signals_url = rejected_url;
    ExpectInterestGroupIsNotValid(
        blink_interest_group, kOrigin,
        "trustedBiddingSignalsUrl" /* expected_error_field_name */,
        rejected_url.GetString().Utf8() /* expected_error_field_value */,
        kBadTrustedBiddingSignalsUrlError /* expected_error */);
  }

  // `trusted_bidding_signals_url` also can't include query strings.
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  KURL rejected_url = KURL(String::FromUTF8("https://origin.test/?query"));
  blink_interest_group->trusted_bidding_signals_url = rejected_url;
  ExpectInterestGroupIsNotValid(
      blink_interest_group, kOrigin,
      "trustedBiddingSignalsUrl" /* expected_error_field_name */,
      rejected_url.GetString().Utf8() /* expected_error_field_value */,
      kBadTrustedBiddingSignalsUrlError /* expected_error */);
}

// Tests valid and invalid ad render URLs.
TEST_F(ValidateBlinkInterestGroupTest, AdRenderUrlValidation) {
  const char kBadAdUrlError[] =
      "renderUrls must be HTTPS and have no embedded credentials.";

  const struct {
    bool expect_allowed;
    const char* url;
  } kTestCases[] = {
      // Same origin URLs are allowed.
      {true, "https://origin.test/foo?bar"},

      // Cross origin URLs are allowed, as long as they're HTTPS.
      {true, "https://b.test/"},
      {true, "https://a.test:1234/"},

      // URLs with the wrong scheme are rejected.
      {false, "http://a.test/"},
      {false, "data://text/html,payload"},
      {false, "filesystem:https://a.test/foo"},

      // URLs with user/ports are rejected.
      {false, "https://user:pass@a.test/"},

      // References are allowed for ads, though not other requests, since they
      // only have an effect when loading a page in a renderer.
      {true, "https://a.test/#foopy"},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.url);

    KURL test_case_url = KURL(String::FromUTF8(test_case.url));

    // Add an InterestGroup with the test cases's URL as the only ad's URL.
    mojom::blink::InterestGroupPtr blink_interest_group =
        CreateMinimalInterestGroup();
    blink_interest_group->ads.emplace();
    blink_interest_group->ads->emplace_back(mojom::blink::InterestGroupAd::New(
        test_case_url, String() /* metadata */));
    if (test_case.expect_allowed) {
      ExpectInterestGroupIsValid(blink_interest_group);
    } else {
      ExpectInterestGroupIsNotValid(
          blink_interest_group, kOrigin,
          "ad[0].renderUrl" /* expected_error_field_name */,
          test_case_url.GetString().Utf8() /* expected_error_field_value */,
          kBadAdUrlError /* expected_error */);
    }

    // Add an InterestGroup with the test cases's URL as the second ad's URL.
    blink_interest_group = CreateMinimalInterestGroup();
    blink_interest_group->ads.emplace();
    blink_interest_group->ads->emplace_back(mojom::blink::InterestGroupAd::New(
        KURL(String::FromUTF8("https://origin.test/")),
        String() /* metadata */));
    blink_interest_group->ads->emplace_back(mojom::blink::InterestGroupAd::New(
        test_case_url, String() /* metadata */));
    if (test_case.expect_allowed) {
      ExpectInterestGroupIsValid(blink_interest_group);
    } else {
      ExpectInterestGroupIsNotValid(
          blink_interest_group, kOrigin,
          "ad[1].renderUrl" /* expected_error_field_name */,
          test_case_url.GetString().Utf8() /* expected_error_field_value */,
          kBadAdUrlError /* expected_error */);
    }
  }
}

}  // namespace blink
