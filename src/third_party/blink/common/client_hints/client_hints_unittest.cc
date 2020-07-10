// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/client_hints/client_hints.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(ClientHintsTest, SerializeLangClientHint) {
  std::string header = SerializeLangClientHint("");
  EXPECT_TRUE(header.empty());

  header = SerializeLangClientHint("es");
  EXPECT_EQ(std::string("\"es\""), header);

  header = SerializeLangClientHint("en-US,fr,de");
  EXPECT_EQ(std::string("\"en-US\", \"fr\", \"de\""), header);

  header = SerializeLangClientHint("en-US,fr,de,ko,zh-CN,ja");
  EXPECT_EQ(std::string("\"en-US\", \"fr\", \"de\", \"ko\", \"zh-CN\", \"ja\""),
            header);
}

}  // namespace blink
