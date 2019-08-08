// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cookie_manager_mojom_traits.h"

#include <vector>

#include "base/test/gtest_util.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/cpp/cookie_manager_mojom_traits.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

template <typename MojoType, typename NativeType>
bool SerializeAndDeserializeEnum(NativeType in, NativeType* out) {
  MojoType intermediate = mojo::EnumTraits<MojoType, NativeType>::ToMojom(in);
  return mojo::EnumTraits<MojoType, NativeType>::FromMojom(intermediate, out);
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieInclusionStatus) {
  std::vector<net::CanonicalCookie::CookieInclusionStatus> statuses = {
      net::CanonicalCookie::CookieInclusionStatus::INCLUDE,
      net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_HTTP_ONLY,
      net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_SECURE_ONLY,
      net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_DOMAIN_MISMATCH,
      net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_NOT_ON_PATH,
      net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT,
      net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_SAMESITE_LAX,
      net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_SAMESITE_EXTENDED,
      net::CanonicalCookie::CookieInclusionStatus::
          EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX,
      net::CanonicalCookie::CookieInclusionStatus::
          EXCLUDE_SAMESITE_NONE_INSECURE,
      net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES,
      net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_FAILURE_TO_STORE,
      net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_NONCOOKIEABLE_SCHEME,
      net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE,
      net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_OVERWRITE_HTTP_ONLY,
      net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN,
      net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX,
      net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR,
  };

  for (auto status : statuses) {
    int32_t serialized = -1;
    using CookieInclusionStatusSerializer =
        mojo::internal::Serializer<mojom::CookieInclusionStatus,
                                   net::CanonicalCookie::CookieInclusionStatus>;
    CookieInclusionStatusSerializer::Serialize(status, &serialized);
    EXPECT_EQ(static_cast<int>(status), serialized);
    net::CanonicalCookie::CookieInclusionStatus deserialized;
    CookieInclusionStatusSerializer::Deserialize(serialized, &deserialized);
    EXPECT_EQ(serialized, static_cast<int>(deserialized));
  }
}

TEST(CookieManagerTraitsTest, Roundtrips_CanonicalCookie) {
  net::CanonicalCookie original(
      "A", "B", "x.y", "/path", base::Time(), base::Time(), base::Time(), false,
      false, net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_LOW);

  net::CanonicalCookie copied;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CanonicalCookie>(
      &original, &copied));

  EXPECT_EQ(original.Name(), copied.Name());
  EXPECT_EQ(original.Value(), copied.Value());
  EXPECT_EQ(original.Domain(), copied.Domain());
  EXPECT_EQ(original.Path(), copied.Path());
  EXPECT_EQ(original.CreationDate(), copied.CreationDate());
  EXPECT_EQ(original.LastAccessDate(), copied.LastAccessDate());
  EXPECT_EQ(original.ExpiryDate(), copied.ExpiryDate());
  EXPECT_EQ(original.IsSecure(), copied.IsSecure());
  EXPECT_EQ(original.IsHttpOnly(), copied.IsHttpOnly());
  EXPECT_EQ(original.SameSite(), copied.SameSite());
  EXPECT_EQ(original.Priority(), copied.Priority());

  // Serializer returns false if cookie is non-canonical.
  // Example is non-canonical because of newline in name.

  original = net::CanonicalCookie("A\n", "B", "x.y", "/path", base::Time(),
                                  base::Time(), base::Time(), false, false,
                                  net::CookieSameSite::NO_RESTRICTION,
                                  net::COOKIE_PRIORITY_LOW);

  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::CanonicalCookie>(
      &original, &copied));
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieWithStatus) {
  net::CanonicalCookie original_cookie(
      "A", "B", "x.y", "/path", base::Time(), base::Time(), base::Time(), false,
      false, net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_LOW);

  net::CookieWithStatus original = {
      original_cookie, net::CanonicalCookie::CookieInclusionStatus::INCLUDE};

  net::CookieWithStatus copied;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CookieWithStatus>(
      &original, &copied));

  EXPECT_EQ(original.cookie.Name(), copied.cookie.Name());
  EXPECT_EQ(original.cookie.Value(), copied.cookie.Value());
  EXPECT_EQ(original.cookie.Domain(), copied.cookie.Domain());
  EXPECT_EQ(original.cookie.Path(), copied.cookie.Path());
  EXPECT_EQ(original.cookie.CreationDate(), copied.cookie.CreationDate());
  EXPECT_EQ(original.cookie.LastAccessDate(), copied.cookie.LastAccessDate());
  EXPECT_EQ(original.cookie.ExpiryDate(), copied.cookie.ExpiryDate());
  EXPECT_EQ(original.cookie.IsSecure(), copied.cookie.IsSecure());
  EXPECT_EQ(original.cookie.IsHttpOnly(), copied.cookie.IsHttpOnly());
  EXPECT_EQ(original.cookie.SameSite(), copied.cookie.SameSite());
  EXPECT_EQ(original.cookie.Priority(), copied.cookie.Priority());
  EXPECT_EQ(original.status, copied.status);
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieSameSite) {
  for (net::CookieSameSite cookie_state :
       {net::CookieSameSite::NO_RESTRICTION, net::CookieSameSite::LAX_MODE,
        net::CookieSameSite::STRICT_MODE}) {
    net::CookieSameSite roundtrip;
    ASSERT_TRUE(SerializeAndDeserializeEnum<mojom::CookieSameSite>(cookie_state,
                                                                   &roundtrip));
    EXPECT_EQ(cookie_state, roundtrip);
  }

  for (net::CookieOptions::SameSiteCookieContext context_state :
       {net::CookieOptions::SameSiteCookieContext::CROSS_SITE,
        net::CookieOptions::SameSiteCookieContext::SAME_SITE_LAX,
        net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT}) {
    net::CookieOptions::SameSiteCookieContext roundtrip;
    ASSERT_TRUE(SerializeAndDeserializeEnum<mojom::CookieSameSiteContext>(
        context_state, &roundtrip));
    EXPECT_EQ(context_state, roundtrip);
  }
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieOptions) {
  {
    net::CookieOptions least_trusted, copy;
    EXPECT_FALSE(least_trusted.return_excluded_cookies());
    least_trusted.set_return_excluded_cookies();  // differ from default.

    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CookieOptions>(
        &least_trusted, &copy));
    EXPECT_TRUE(copy.exclude_httponly());
    EXPECT_EQ(net::CookieOptions::SameSiteCookieContext::CROSS_SITE,
              copy.same_site_cookie_context());
    EXPECT_TRUE(copy.return_excluded_cookies());
  }

  {
    net::CookieOptions very_trusted, copy;
    very_trusted.set_include_httponly();
    very_trusted.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);

    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CookieOptions>(
        &very_trusted, &copy));
    EXPECT_FALSE(copy.exclude_httponly());
    EXPECT_EQ(net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
              copy.same_site_cookie_context());
    EXPECT_FALSE(copy.return_excluded_cookies());
  }
}

// TODO: Add tests for CookiePriority, more extensive CookieOptions ones

}  // namespace
}  // namespace network
