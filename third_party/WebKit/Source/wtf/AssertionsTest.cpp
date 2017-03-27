// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/Assertions.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/StringBuilder.h"
#include <stdio.h>

namespace WTF {

TEST(AssertionsTest, Assertions) {
  ASSERT(true);
#if DCHECK_IS_ON()
  EXPECT_DEATH_IF_SUPPORTED(ASSERT(false), "");
  EXPECT_DEATH_IF_SUPPORTED(ASSERT_NOT_REACHED(), "");
#endif

  RELEASE_ASSERT(true);
  EXPECT_DEATH_IF_SUPPORTED(RELEASE_ASSERT(false), "");

  SECURITY_DCHECK(true);
#if ENABLE(SECURITY_ASSERT)
  EXPECT_DEATH_IF_SUPPORTED(SECURITY_DCHECK(false), "");
#endif

  SECURITY_CHECK(true);
  EXPECT_DEATH_IF_SUPPORTED(SECURITY_CHECK(false), "");

  EXPECT_DEATH_IF_SUPPORTED(CRASH(), "");
  EXPECT_DEATH_IF_SUPPORTED(IMMEDIATE_CRASH(), "");
};

#if !LOG_DISABLED
static const int kPrinterBufferSize = 256;
static char gBuffer[kPrinterBufferSize];
static StringBuilder gBuilder;

static void vprint(const char* format, va_list args) {
  int written = vsnprintf(gBuffer, kPrinterBufferSize, format, args);
  if (written > 0 && written < kPrinterBufferSize)
    gBuilder.append(gBuffer);
}

TEST(AssertionsTest, ScopedLogger) {
  ScopedLogger::setPrintFuncForTests(vprint);
  {
    WTF_CREATE_SCOPED_LOGGER(a, "a1");
    {
      WTF_CREATE_SCOPED_LOGGER_IF(b, false, "b1");
      {
        WTF_CREATE_SCOPED_LOGGER(c, "c");
        { WTF_CREATE_SCOPED_LOGGER(d, "d %d %s", -1, "hello"); }
      }
      WTF_APPEND_SCOPED_LOGGER(b, "b2");
    }
    WTF_APPEND_SCOPED_LOGGER(a, "a2 %.1f", 0.5);
  }

  EXPECT_EQ(
      "( a1\n"
      "  ( c\n"
      "    ( d -1 hello )\n"
      "  )\n"
      "  a2 0.5\n"
      ")\n",
      gBuilder.toString());
};
#endif  // !LOG_DISABLED

}  // namespace WTF
