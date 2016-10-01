/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/platform/WebURLResponse.h"

#include "platform/weborigin/KURL.h"
#include "public/platform/WebURL.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class TestExtraData : public WebURLResponse::ExtraData {
 public:
  explicit TestExtraData(bool* alive) : m_alive(alive) { *alive = true; }

  ~TestExtraData() override { *m_alive = false; }

 private:
  bool* m_alive;
};

}  // anonymous namespace

TEST(WebURLResponseTest, ExtraData) {
  bool alive = false;
  {
    WebURLResponse urlResponse;
    TestExtraData* extraData = new TestExtraData(&alive);
    EXPECT_TRUE(alive);

    urlResponse.setExtraData(extraData);
    EXPECT_EQ(extraData, urlResponse.getExtraData());
    {
      WebURLResponse otherUrlResponse = urlResponse;
      EXPECT_TRUE(alive);
      EXPECT_EQ(extraData, otherUrlResponse.getExtraData());
      EXPECT_EQ(extraData, urlResponse.getExtraData());
    }
    EXPECT_TRUE(alive);
    EXPECT_EQ(extraData, urlResponse.getExtraData());
  }
  EXPECT_FALSE(alive);
}

TEST(WebURLResponseTest, NewInstanceIsNull) {
  WebURLResponse instance;
  EXPECT_TRUE(instance.isNull());
}

TEST(WebURLResponseTest, NotNullAfterSetURL) {
  WebURLResponse instance;
  instance.setURL(KURL(ParsedURLString, "http://localhost/"));
  EXPECT_FALSE(instance.isNull());
}

}  // namespace blink
