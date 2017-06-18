// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/wtf/Assertions.h"

#include "platform/wtf/text/StringBuilder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <stdio.h>

namespace WTF {

TEST(AssertionsTest, Assertions) {
  DCHECK(true);
#if DCHECK_IS_ON()
  EXPECT_DEATH_IF_SUPPORTED(DCHECK(false), "");
  EXPECT_DEATH_IF_SUPPORTED(NOTREACHED(), "");
  EXPECT_DEATH_IF_SUPPORTED(DCHECK_AT(false, __FILE__, __LINE__), "");
#else
  DCHECK(false);
  NOTREACHED();
  DCHECK_AT(false, __FILE__, __LINE__);
#endif

  CHECK(true);
  EXPECT_DEATH_IF_SUPPORTED(CHECK(false), "");

  SECURITY_DCHECK(true);
#if ENABLE(SECURITY_ASSERT)
  EXPECT_DEATH_IF_SUPPORTED(SECURITY_DCHECK(false), "");
#else
  SECURITY_DCHECK(false);
#endif

  SECURITY_CHECK(true);
  EXPECT_DEATH_IF_SUPPORTED(SECURITY_CHECK(false), "");
};

#if !LOG_DISABLED
static const int kPrinterBufferSize = 256;
static char g_buffer[kPrinterBufferSize];
static StringBuilder g_builder;

static void Vprint(const char* format, va_list args) {
  int written = vsnprintf(g_buffer, kPrinterBufferSize, format, args);
  if (written > 0 && written < kPrinterBufferSize)
    g_builder.Append(g_buffer);
}

TEST(AssertionsTest, ScopedLogger) {
  ScopedLogger::SetPrintFuncForTests(Vprint);
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
      g_builder.ToString());
};
#endif  // !LOG_DISABLED

}  // namespace WTF
