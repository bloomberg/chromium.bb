// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ReferrerScriptInfo.h"

#include "bindings/core/v8/V8BindingForTesting.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {

TEST(ReferrerScriptInfo, IsDefaultValue) {
  EXPECT_TRUE(ReferrerScriptInfo().IsDefaultValue());
  EXPECT_FALSE(
      ReferrerScriptInfo(network::mojom::FetchCredentialsMode::kPassword, "",
                         kNotParserInserted)
          .IsDefaultValue());
}

TEST(ReferrerScriptInfo, ToFromV8) {
  V8TestingScope scope;

  EXPECT_TRUE(ReferrerScriptInfo()
                  .ToV8HostDefinedOptions(scope.GetIsolate())
                  .IsEmpty());

  ReferrerScriptInfo info(network::mojom::FetchCredentialsMode::kPassword,
                          "foobar", kNotParserInserted);
  v8::Local<v8::PrimitiveArray> v8_info =
      info.ToV8HostDefinedOptions(scope.GetIsolate());

  ReferrerScriptInfo decoded =
      ReferrerScriptInfo::FromV8HostDefinedOptions(scope.GetContext(), v8_info);
  EXPECT_EQ(network::mojom::FetchCredentialsMode::kPassword,
            decoded.CredentialsMode());
  EXPECT_EQ("foobar", decoded.Nonce());
  EXPECT_EQ(kNotParserInserted, decoded.ParserState());
}

}  // namespace blink
