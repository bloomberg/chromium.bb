// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/mojo/url_test.mojom.h"

namespace url {

class UrlTestImpl : public mojom::UrlTest {
 public:
  explicit UrlTestImpl(mojo::InterfaceRequest<url::mojom::UrlTest> request)
      : binding_(this, std::move(request)) {
  }

  // UrlTest:
  void Bounce(const GURL& in, const BounceCallback& callback) override {
    callback.Run(in);
  }

 private:
  mojo::Binding<UrlTest> binding_;
};

// Mojo version of chrome IPC test in
// content/common/common_param_traits_unittest.cc
TEST(MojoUrlGURLStructTraitsTest, Basic) {
  base::MessageLoop message_loop;

  mojom::UrlTestPtr proxy;
  UrlTestImpl impl(GetProxy(&proxy));

  const char* serialize_cases[] = {
    "http://www.google.com/",
    "http://user:pass@host.com:888/foo;bar?baz#nop",
  };

  for (size_t i = 0; i < arraysize(serialize_cases); i++) {
    GURL input(serialize_cases[i]);
    GURL output;
    EXPECT_TRUE(proxy->Bounce(input, &output));

    // We want to test each component individually to make sure its range was
    // correctly serialized and deserialized, not just the spec.
    EXPECT_EQ(input.possibly_invalid_spec(), output.possibly_invalid_spec());
    EXPECT_EQ(input.is_valid(), output.is_valid());
    EXPECT_EQ(input.scheme(), output.scheme());
    EXPECT_EQ(input.username(), output.username());
    EXPECT_EQ(input.password(), output.password());
    EXPECT_EQ(input.host(), output.host());
    EXPECT_EQ(input.port(), output.port());
    EXPECT_EQ(input.path(), output.path());
    EXPECT_EQ(input.query(), output.query());
    EXPECT_EQ(input.ref(), output.ref());
  }

  // Test an excessively long GURL.
  {
    const std::string url = std::string("http://example.org/").append(
        mojo::kMaxUrlChars + 1, 'a');
    GURL input(url.c_str());
    GURL output;
    EXPECT_TRUE(proxy->Bounce(input, &output));
    EXPECT_TRUE(output.is_empty());
  }
}

}  // namespace url
