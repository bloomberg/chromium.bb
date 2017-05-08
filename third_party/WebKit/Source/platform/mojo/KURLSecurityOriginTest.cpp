// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/mojo/url_test.mojom-blink.h"
#include "url/url_constants.h"

namespace blink {
namespace {

class UrlTestImpl : public url::mojom::blink::UrlTest {
 public:
  explicit UrlTestImpl(url::mojom::blink::UrlTestRequest request)
      : binding_(this, std::move(request)) {}

  // UrlTest:
  void BounceUrl(const KURL& in, const BounceUrlCallback& callback) override {
    callback.Run(in);
  }

  void BounceOrigin(const RefPtr<SecurityOrigin>& in,
                    const BounceOriginCallback& callback) override {
    callback.Run(in);
  }

 private:
  mojo::Binding<UrlTest> binding_;
};

}  // namespace

// Mojo version of chrome IPC test in url/ipc/url_param_traits_unittest.cc.
TEST(KURLSecurityOriginStructTraitsTest, Basic) {
  base::MessageLoop message_loop;

  url::mojom::blink::UrlTestPtr proxy;
  UrlTestImpl impl(MakeRequest(&proxy));

  const char* serialize_cases[] = {
      "http://www.google.com/", "http://user:pass@host.com:888/foo;bar?baz#nop",
  };

  for (const char* test_case : serialize_cases) {
    KURL input(KURL(), test_case);
    KURL output;
    EXPECT_TRUE(proxy->BounceUrl(input, &output));

    // We want to test each component individually to make sure its range was
    // correctly serialized and deserialized, not just the spec.
    EXPECT_EQ(input.GetString(), output.GetString());
    EXPECT_EQ(input.IsValid(), output.IsValid());
    EXPECT_EQ(input.Protocol(), output.Protocol());
    EXPECT_EQ(input.User(), output.User());
    EXPECT_EQ(input.Pass(), output.Pass());
    EXPECT_EQ(input.Host(), output.Host());
    EXPECT_EQ(input.Port(), output.Port());
    EXPECT_EQ(input.GetPath(), output.GetPath());
    EXPECT_EQ(input.Query(), output.Query());
    EXPECT_EQ(input.FragmentIdentifier(), output.FragmentIdentifier());
  }

  // Test an excessively long GURL.
  {
    const std::string url =
        std::string("http://example.org/").append(url::kMaxURLChars + 1, 'a');
    KURL input(KURL(), url.c_str());
    KURL output;
    EXPECT_TRUE(proxy->BounceUrl(input, &output));
    EXPECT_TRUE(output.IsEmpty());
  }

  // Test basic Origin serialization.
  RefPtr<SecurityOrigin> non_unique =
      SecurityOrigin::Create("http", "www.google.com", 80);
  RefPtr<SecurityOrigin> output;
  EXPECT_TRUE(proxy->BounceOrigin(non_unique, &output));
  EXPECT_TRUE(non_unique->IsSameSchemeHostPort(output.Get()));
  EXPECT_FALSE(non_unique->IsUnique());

  RefPtr<SecurityOrigin> unique = SecurityOrigin::CreateUnique();
  EXPECT_TRUE(proxy->BounceOrigin(unique, &output));
  EXPECT_TRUE(output->IsUnique());
}

}  // namespace url
