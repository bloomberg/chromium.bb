// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/ParsedContentDisposition.h"
#include "platform/network/ParsedContentHeaderFieldParameters.h"
#include "platform/network/ParsedContentType.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

using Mode = ParsedContentHeaderFieldParameters::Mode;

void CheckValidity(bool expected,
                   const String& input,
                   Mode mode = Mode::kNormal) {
  EXPECT_EQ(expected,
            ParsedContentHeaderFieldParameters::CreateForTesting(input, mode)
                .IsValid())
      << input;

  const String disposition_input = "attachment" + input;
  EXPECT_EQ(expected,
            ParsedContentDisposition(disposition_input, mode).IsValid())
      << disposition_input;

  const String type_input = "text/plain" + input;
  EXPECT_EQ(expected, ParsedContentType(type_input, mode).IsValid())
      << type_input;
}

TEST(ParsedContentHeaderFieldParametersTest, Validity) {
  CheckValidity(true, "");
  CheckValidity(true, "  ");
  CheckValidity(true, "  ;p1=v1");
  CheckValidity(true, ";  p1=v1");
  CheckValidity(true, ";p1=v1  ");
  CheckValidity(true, ";p1 = v1");
  CheckValidity(true, ";z=\"ttx&r=z;;\\u\\\"kd==\"");
  CheckValidity(true, "; z=\"\xff\"");

  CheckValidity(false, "\r");
  CheckValidity(false, "\n");
  CheckValidity(false, " p1=v1");
  CheckValidity(false, ";p1=v1;");
  CheckValidity(false, ";");
  CheckValidity(false, ";  ");
  CheckValidity(false, "; p1");
  CheckValidity(false, "; p1;");
  CheckValidity(false, ";\"xx");
  CheckValidity(false, ";\"xx=y");
  CheckValidity(false, "; \"z\"=u");
  CheckValidity(false, "; z=\xff");

  CheckValidity(false, ";z=q/t:()<>@,:\\/[]?");
  CheckValidity(true, ";z=q/t:()<>@,:\\/[]?=", Mode::kRelaxed);
  CheckValidity(false, ";z=q r", Mode::kRelaxed);
  CheckValidity(false, ";z=q;r", Mode::kRelaxed);
  CheckValidity(false, ";z=q\"r", Mode::kRelaxed);
  CheckValidity(false, "; z=\xff", Mode::kRelaxed);
}

TEST(ParsedContentHeaderFieldParametersTest, ParameterName) {
  String input = "; y=z  ; y= u ;  t=r;k= \"t \\u\\\"x\" ;Q=U;T=S";

  CheckValidity(true, input);

  ParsedContentHeaderFieldParameters t =
      ParsedContentHeaderFieldParameters::CreateForTesting(input,
                                                           Mode::kNormal);
  EXPECT_TRUE(t.IsValid());
  EXPECT_EQ(4u, t.ParameterCount());
  EXPECT_EQ(String(), t.ParameterValueForName("a"));
  EXPECT_EQ(String(), t.ParameterValueForName("x"));
  EXPECT_EQ("u", t.ParameterValueForName("y"));
  EXPECT_EQ("S", t.ParameterValueForName("t"));
  EXPECT_EQ("t u\"x", t.ParameterValueForName("k"));
  EXPECT_EQ("U", t.ParameterValueForName("Q"));
  EXPECT_EQ("S", t.ParameterValueForName("T"));

  String kelvin = String::FromUTF8("\xe2\x84\xaa");
  DCHECK_EQ(kelvin.LowerUnicode(AtomicString()), "k");
  EXPECT_EQ(String(), t.ParameterValueForName(kelvin));
}

TEST(ParsedContentHeaderFieldParametersTest, RelaxedParameterName) {
  String input = "; z=q/t:()<>@,:\\/[]?=;y=u";

  CheckValidity(true, input, Mode::kRelaxed);

  ParsedContentHeaderFieldParameters t =
      ParsedContentHeaderFieldParameters::CreateForTesting(input,
                                                           Mode::kRelaxed);
  EXPECT_TRUE(t.IsValid());
  EXPECT_EQ(2u, t.ParameterCount());
  EXPECT_EQ("q/t:()<>@,:\\/[]?=", t.ParameterValueForName("z"));
  EXPECT_EQ("u", t.ParameterValueForName("y"));
}

TEST(ParsedContentHeaderFieldParametersTest, StrictParameterName) {
  CheckValidity(true, "", Mode::kStrict);
  CheckValidity(true, "; p1=a", Mode::kStrict);
  CheckValidity(false, "; p1=a; p1=b", Mode::kStrict);
  CheckValidity(true, "; p1=a; p2=b", Mode::kStrict);
}

}  // namespace

}  // namespace blink
