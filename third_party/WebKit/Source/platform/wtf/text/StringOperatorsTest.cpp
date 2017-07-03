/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#define WTF_STRINGTYPEADAPTER_COPIED_WTF_STRING() (++wtfStringCopyCount)

static int wtfStringCopyCount;

#include "build/build_config.h"
#include "platform/wtf/text/WTFString.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

#define EXPECT_N_WTF_STRING_COPIES(count, expr)    \
  do {                                             \
    wtfStringCopyCount = 0;                        \
    String __test_string = expr;                   \
    (void)__test_string;                           \
    EXPECT_EQ(count, wtfStringCopyCount) << #expr; \
  } while (false)

TEST(StringOperatorsTest, DISABLED_StringOperators) {
  String string("String");
  AtomicString atomic_string("AtomicString");
  const char* literal = "ASCIILiteral";

  EXPECT_EQ(0, wtfStringCopyCount);

  EXPECT_N_WTF_STRING_COPIES(2, string + string);
  EXPECT_N_WTF_STRING_COPIES(2, string + atomic_string);
  EXPECT_N_WTF_STRING_COPIES(2, atomic_string + string);
  EXPECT_N_WTF_STRING_COPIES(2, atomic_string + atomic_string);

  EXPECT_N_WTF_STRING_COPIES(1, "C string" + string);
  EXPECT_N_WTF_STRING_COPIES(1, string + "C string");
  EXPECT_N_WTF_STRING_COPIES(1, "C string" + atomic_string);
  EXPECT_N_WTF_STRING_COPIES(1, atomic_string + "C string");

  EXPECT_N_WTF_STRING_COPIES(1, literal + string);
  EXPECT_N_WTF_STRING_COPIES(1, string + literal);
  EXPECT_N_WTF_STRING_COPIES(1, literal + atomic_string);
  EXPECT_N_WTF_STRING_COPIES(1, atomic_string + literal);

  EXPECT_N_WTF_STRING_COPIES(2, "C string" + string + "C string" + string);
  EXPECT_N_WTF_STRING_COPIES(2, "C string" + (string + "C string" + string));
  EXPECT_N_WTF_STRING_COPIES(2, ("C string" + string) + ("C string" + string));
  EXPECT_N_WTF_STRING_COPIES(2, string + "C string" + string + "C string");
  EXPECT_N_WTF_STRING_COPIES(2, string + ("C string" + string + "C string"));
  EXPECT_N_WTF_STRING_COPIES(2, (string + "C string") + (string + "C string"));

  EXPECT_N_WTF_STRING_COPIES(2, literal + string + literal + string);
  EXPECT_N_WTF_STRING_COPIES(2, literal + (string + literal + string));
  EXPECT_N_WTF_STRING_COPIES(2, (literal + string) + (literal + string));
  EXPECT_N_WTF_STRING_COPIES(2, string + literal + string + literal);
  EXPECT_N_WTF_STRING_COPIES(2, string + (literal + string + literal));
  EXPECT_N_WTF_STRING_COPIES(2, (string + literal) + (string + literal));

  EXPECT_N_WTF_STRING_COPIES(2, literal + string + "C string" + string);
  EXPECT_N_WTF_STRING_COPIES(2, literal + (string + "C string" + string));
  EXPECT_N_WTF_STRING_COPIES(2, (literal + string) + ("C string" + string));
  EXPECT_N_WTF_STRING_COPIES(2, "C string" + string + literal + string);
  EXPECT_N_WTF_STRING_COPIES(2, "C string" + (string + literal + string));
  EXPECT_N_WTF_STRING_COPIES(2, ("C string" + string) + (literal + string));

  EXPECT_N_WTF_STRING_COPIES(
      2, literal + atomic_string + "C string" + atomic_string);
  EXPECT_N_WTF_STRING_COPIES(
      2, literal + (atomic_string + "C string" + atomic_string));
  EXPECT_N_WTF_STRING_COPIES(
      2, (literal + atomic_string) + ("C string" + atomic_string));
  EXPECT_N_WTF_STRING_COPIES(
      2, "C string" + atomic_string + literal + atomic_string);
  EXPECT_N_WTF_STRING_COPIES(
      2, "C string" + (atomic_string + literal + atomic_string));
  EXPECT_N_WTF_STRING_COPIES(
      2, ("C string" + atomic_string) + (literal + atomic_string));

  EXPECT_N_WTF_STRING_COPIES(2, literal + atomic_string + "C string" + string);
  EXPECT_N_WTF_STRING_COPIES(2,
                             literal + (atomic_string + "C string" + string));
  EXPECT_N_WTF_STRING_COPIES(2,
                             (literal + atomic_string) + ("C string" + string));
  EXPECT_N_WTF_STRING_COPIES(2, "C string" + atomic_string + literal + string);
  EXPECT_N_WTF_STRING_COPIES(2,
                             "C string" + (atomic_string + literal + string));
  EXPECT_N_WTF_STRING_COPIES(2,
                             ("C string" + atomic_string) + (literal + string));

  EXPECT_N_WTF_STRING_COPIES(2, literal + string + "C string" + atomic_string);
  EXPECT_N_WTF_STRING_COPIES(2,
                             literal + (string + "C string" + atomic_string));
  EXPECT_N_WTF_STRING_COPIES(2,
                             (literal + string) + ("C string" + atomic_string));
  EXPECT_N_WTF_STRING_COPIES(2, "C string" + string + literal + atomic_string);
  EXPECT_N_WTF_STRING_COPIES(2,
                             "C string" + (string + literal + atomic_string));
  EXPECT_N_WTF_STRING_COPIES(2,
                             ("C string" + string) + (literal + atomic_string));

  EXPECT_N_WTF_STRING_COPIES(
      2, "C string" + atomic_string + "C string" + atomic_string);
  EXPECT_N_WTF_STRING_COPIES(
      2, "C string" + (atomic_string + "C string" + atomic_string));
  EXPECT_N_WTF_STRING_COPIES(
      2, ("C string" + atomic_string) + ("C string" + atomic_string));
  EXPECT_N_WTF_STRING_COPIES(
      2, atomic_string + "C string" + atomic_string + "C string");
  EXPECT_N_WTF_STRING_COPIES(
      2, atomic_string + ("C string" + atomic_string + "C string"));
  EXPECT_N_WTF_STRING_COPIES(
      2, (atomic_string + "C string") + (atomic_string + "C string"));

  EXPECT_N_WTF_STRING_COPIES(2,
                             literal + atomic_string + literal + atomic_string);
  EXPECT_N_WTF_STRING_COPIES(
      2, literal + (atomic_string + literal + atomic_string));
  EXPECT_N_WTF_STRING_COPIES(
      2, (literal + atomic_string) + (literal + atomic_string));
  EXPECT_N_WTF_STRING_COPIES(2,
                             atomic_string + literal + atomic_string + literal);
  EXPECT_N_WTF_STRING_COPIES(
      2, atomic_string + (literal + atomic_string + literal));
  EXPECT_N_WTF_STRING_COPIES(
      2, (atomic_string + literal) + (atomic_string + literal));

  EXPECT_N_WTF_STRING_COPIES(2,
                             "C string" + string + "C string" + atomic_string);
  EXPECT_N_WTF_STRING_COPIES(
      2, "C string" + (string + "C string" + atomic_string));
  EXPECT_N_WTF_STRING_COPIES(
      2, ("C string" + string) + ("C string" + atomic_string));
  EXPECT_N_WTF_STRING_COPIES(2,
                             string + "C string" + atomic_string + "C string");
  EXPECT_N_WTF_STRING_COPIES(
      2, string + ("C string" + atomic_string + "C string"));
  EXPECT_N_WTF_STRING_COPIES(
      2, (string + "C string") + (atomic_string + "C string"));

  EXPECT_N_WTF_STRING_COPIES(2, literal + string + literal + atomic_string);
  EXPECT_N_WTF_STRING_COPIES(2, literal + (string + literal + atomic_string));
  EXPECT_N_WTF_STRING_COPIES(2, (literal + string) + (literal + atomic_string));
  EXPECT_N_WTF_STRING_COPIES(2, string + literal + atomic_string + literal);
  EXPECT_N_WTF_STRING_COPIES(2, string + (literal + atomic_string + literal));
  EXPECT_N_WTF_STRING_COPIES(2, (string + literal) + (atomic_string + literal));

  EXPECT_N_WTF_STRING_COPIES(2,
                             "C string" + atomic_string + "C string" + string);
  EXPECT_N_WTF_STRING_COPIES(
      2, "C string" + (atomic_string + "C string" + string));
  EXPECT_N_WTF_STRING_COPIES(
      2, ("C string" + atomic_string) + ("C string" + string));
  EXPECT_N_WTF_STRING_COPIES(2,
                             atomic_string + "C string" + string + "C string");
  EXPECT_N_WTF_STRING_COPIES(
      2, atomic_string + ("C string" + string + "C string"));
  EXPECT_N_WTF_STRING_COPIES(
      2, (atomic_string + "C string") + (string + "C string"));

  EXPECT_N_WTF_STRING_COPIES(2, literal + atomic_string + literal + string);
  EXPECT_N_WTF_STRING_COPIES(2, literal + (atomic_string + literal + string));
  EXPECT_N_WTF_STRING_COPIES(2, (literal + atomic_string) + (literal + string));
  EXPECT_N_WTF_STRING_COPIES(2, atomic_string + literal + string + literal);
  EXPECT_N_WTF_STRING_COPIES(2, atomic_string + (literal + string + literal));
  EXPECT_N_WTF_STRING_COPIES(2, (atomic_string + literal) + (string + literal));

#if defined(COMPILER_MSVC)
  EXPECT_N_WTF_STRING_COPIES(1, L"wide string" + string);
  EXPECT_N_WTF_STRING_COPIES(1, string + L"wide string");
  EXPECT_N_WTF_STRING_COPIES(1, L"wide string" + atomic_string);
  EXPECT_N_WTF_STRING_COPIES(1, atomic_string + L"wide string");

  EXPECT_N_WTF_STRING_COPIES(2,
                             L"wide string" + string + L"wide string" + string);
  EXPECT_N_WTF_STRING_COPIES(
      2, L"wide string" + (string + L"wide string" + string));
  EXPECT_N_WTF_STRING_COPIES(
      2, (L"wide string" + string) + (L"wide string" + string));
  EXPECT_N_WTF_STRING_COPIES(2,
                             string + L"wide string" + string + L"wide string");
  EXPECT_N_WTF_STRING_COPIES(
      2, string + (L"wide string" + string + L"wide string"));
  EXPECT_N_WTF_STRING_COPIES(
      2, (string + L"wide string") + (string + L"wide string"));

  EXPECT_N_WTF_STRING_COPIES(
      2, L"wide string" + atomic_string + L"wide string" + atomic_string);
  EXPECT_N_WTF_STRING_COPIES(
      2, L"wide string" + (atomic_string + L"wide string" + atomic_string));
  EXPECT_N_WTF_STRING_COPIES(
      2, (L"wide string" + atomic_string) + (L"wide string" + atomic_string));
  EXPECT_N_WTF_STRING_COPIES(
      2, atomic_string + L"wide string" + atomic_string + L"wide string");
  EXPECT_N_WTF_STRING_COPIES(
      2, atomic_string + (L"wide string" + atomic_string + L"wide string"));
  EXPECT_N_WTF_STRING_COPIES(
      2, (atomic_string + L"wide string") + (atomic_string + L"wide string"));

  EXPECT_N_WTF_STRING_COPIES(
      2, L"wide string" + string + L"wide string" + atomic_string);
  EXPECT_N_WTF_STRING_COPIES(
      2, L"wide string" + (string + L"wide string" + atomic_string));
  EXPECT_N_WTF_STRING_COPIES(
      2, (L"wide string" + string) + (L"wide string" + atomic_string));
  EXPECT_N_WTF_STRING_COPIES(
      2, string + L"wide string" + atomic_string + L"wide string");
  EXPECT_N_WTF_STRING_COPIES(
      2, string + (L"wide string" + atomic_string + L"wide string"));
  EXPECT_N_WTF_STRING_COPIES(
      2, (string + L"wide string") + (atomic_string + L"wide string"));

  EXPECT_N_WTF_STRING_COPIES(
      2, L"wide string" + atomic_string + L"wide string" + string);
  EXPECT_N_WTF_STRING_COPIES(
      2, L"wide string" + (atomic_string + L"wide string" + string));
  EXPECT_N_WTF_STRING_COPIES(
      2, (L"wide string" + atomic_string) + (L"wide string" + string));
  EXPECT_N_WTF_STRING_COPIES(
      2, atomic_string + L"wide string" + string + L"wide string");
  EXPECT_N_WTF_STRING_COPIES(
      2, atomic_string + (L"wide string" + string + L"wide string"));
  EXPECT_N_WTF_STRING_COPIES(
      2, (atomic_string + L"wide string") + (string + L"wide string"));
#endif
}

}  // namespace WTF
