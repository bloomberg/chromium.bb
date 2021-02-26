// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xfa/fxfa/fm2js/cxfa_fmlexer.h"

#include <memory>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/base/stl_util.h"

TEST(CXFA_FMLexerTest, NullString) {
  WideStringView null_string;
  CXFA_FMLexer lexer(null_string);
  CXFA_FMToken token = lexer.NextToken();
  EXPECT_EQ(TOKeof, token.m_type);
  EXPECT_TRUE(lexer.IsComplete());
}

TEST(CXFA_FMLexerTest, EmptyString) {
  CXFA_FMLexer lexer(L"");
  CXFA_FMToken token = lexer.NextToken();
  EXPECT_EQ(TOKeof, token.m_type);
  EXPECT_TRUE(lexer.IsComplete());
}

TEST(CXFA_FMLexerTest, Numbers) {
  {
    CXFA_FMLexer lexer(L"-12");
    CXFA_FMToken token = lexer.NextToken();
    // TODO(dsinclair): Should this return -12 instead of two tokens?
    EXPECT_EQ(TOKminus, token.m_type);
    token = lexer.NextToken();
    EXPECT_EQ(L"12", token.m_string);
    token = lexer.NextToken();
    EXPECT_EQ(TOKeof, token.m_type);
  }
  {
    CXFA_FMLexer lexer(L"1.5362");
    CXFA_FMToken token = lexer.NextToken();
    EXPECT_EQ(TOKnumber, token.m_type);
    EXPECT_EQ(L"1.5362", token.m_string);
  }
  {
    CXFA_FMLexer lexer(L"0.875");
    CXFA_FMToken token = lexer.NextToken();
    EXPECT_EQ(TOKnumber, token.m_type);
    EXPECT_EQ(L"0.875", token.m_string);
  }
  {
    CXFA_FMLexer lexer(L"5.56e-2");
    CXFA_FMToken token = lexer.NextToken();
    EXPECT_EQ(TOKnumber, token.m_type);
    EXPECT_EQ(L"5.56e-2", token.m_string);
  }
  {
    CXFA_FMLexer lexer(L"1.234E10");
    CXFA_FMToken token = lexer.NextToken();
    EXPECT_EQ(TOKnumber, token.m_type);
    EXPECT_EQ(L"1.234E10", token.m_string);
  }
  {
    CXFA_FMLexer lexer(L"123456789.012345678");
    CXFA_FMToken token = lexer.NextToken();
    EXPECT_EQ(TOKnumber, token.m_type);
    // TODO(dsinclair): This should round as per IEEE 64-bit values.
    // EXPECT_EQ(L"123456789.01234567", token.m_string);
    EXPECT_EQ(L"123456789.012345678", token.m_string);
  }
  {
    CXFA_FMLexer lexer(L"99999999999999999");
    CXFA_FMToken token = lexer.NextToken();
    EXPECT_EQ(TOKnumber, token.m_type);
    // TODO(dsinclair): This is spec'd as rounding when > 16 significant digits
    // prior to the exponent.
    // EXPECT_EQ(L"100000000000000000", token.m_string);
    EXPECT_EQ(L"99999999999999999", token.m_string);
    EXPECT_TRUE(lexer.IsComplete());
  }
}

// The quotes are stripped in CXFA_FMStringExpression::ToJavaScript.
TEST(CXFA_FMLexerTest, Strings) {
  {
    CXFA_FMLexer lexer(L"\"The cat jumped over the fence.\"");
    CXFA_FMToken token = lexer.NextToken();
    EXPECT_EQ(TOKstring, token.m_type);
    EXPECT_EQ(L"\"The cat jumped over the fence.\"", token.m_string);

    token = lexer.NextToken();
    EXPECT_EQ(TOKeof, token.m_type);
  }
  {
    CXFA_FMLexer lexer(L"\"\"");
    CXFA_FMToken token = lexer.NextToken();
    EXPECT_EQ(TOKstring, token.m_type);
    EXPECT_EQ(L"\"\"", token.m_string);
  }
  {
    CXFA_FMLexer lexer(
        L"\"The message reads: \"\"Warning: Insufficient Memory\"\"\"");
    CXFA_FMToken token = lexer.NextToken();
    EXPECT_EQ(TOKstring, token.m_type);
    EXPECT_EQ(L"\"The message reads: \"\"Warning: Insufficient Memory\"\"\"",
              token.m_string);
  }
  {
    CXFA_FMLexer lexer(
        L"\"\\u0047\\u006f\\u0066\\u0069\\u0073\\u0068\\u0021\\u000d\\u000a\"");
    CXFA_FMToken token = lexer.NextToken();
    EXPECT_EQ(TOKstring, token.m_type);
    EXPECT_EQ(
        L"\"\\u0047\\u006f\\u0066\\u0069\\u0073\\u0068\\u0021\\u000d\\u000a\"",
        token.m_string);
    EXPECT_TRUE(lexer.IsComplete());
  }
}

// Note, 'this' is a keyword but is not matched by the lexer.
TEST(CXFA_FMLexerTest, OperatorsAndKeywords) {
  struct {
    const wchar_t* op;
    XFA_FM_TOKEN token;
  } op[] = {{L"+", TOKplus},
            {L"/", TOKdiv},
            {L"-", TOKminus},
            {L"&", TOKand},
            {L"|", TOKor},
            {L"*", TOKmul},
            {L"<", TOKlt},
            {L">", TOKgt},
            {L"==", TOKeq},
            {L"<>", TOKne},
            {L"<=", TOKle},
            {L">=", TOKge},
            {L"and", TOKksand},
            {L"break", TOKbreak},
            {L"continue", TOKcontinue},
            {L"do", TOKdo},
            {L"downto", TOKdownto},
            {L"else", TOKelse},
            {L"elseif", TOKelseif},
            {L"end", TOKend},
            {L"endfor", TOKendfor},
            {L"endfunc", TOKendfunc},
            {L"endif", TOKendif},
            {L"endwhile", TOKendwhile},
            {L"eq", TOKkseq},
            {L"exit", TOKexit},
            {L"for", TOKfor},
            {L"foreach", TOKforeach},
            {L"func", TOKfunc},
            {L"ge", TOKksge},
            {L"gt", TOKksgt},
            {L"if", TOKif},
            {L"in", TOKin},
            {L"infinity", TOKinfinity},
            {L"le", TOKksle},
            {L"lt", TOKkslt},
            {L"nan", TOKnan},
            {L"ne", TOKksne},
            {L"not", TOKksnot},
            {L"null", TOKnull},
            {L"or", TOKksor},
            {L"return", TOKreturn},
            {L"step", TOKstep},
            {L"then", TOKthen},
            {L"throw", TOKthrow},
            {L"upto", TOKupto},
            {L"var", TOKvar},
            {L"while", TOKwhile},

            // The following are defined but aren't in the spec.
            {L"(", TOKlparen},
            {L")", TOKrparen},
            {L",", TOKcomma},
            {L".", TOKdot},
            {L"[", TOKlbracket},
            {L"]", TOKrbracket},
            {L"..", TOKdotdot},
            {L".#", TOKdotscream},
            {L".*", TOKdotstar}};

  for (size_t i = 0; i < pdfium::size(op); ++i) {
    CXFA_FMLexer lexer(op[i].op);
    CXFA_FMToken token = lexer.NextToken();
    EXPECT_EQ(op[i].token, token.m_type);
    EXPECT_TRUE(lexer.IsComplete());
  }
}

TEST(CXFA_FMLexerTest, Comments) {
  {
    CXFA_FMLexer lexer(L"// Empty.");
    CXFA_FMToken token = lexer.NextToken();
    EXPECT_EQ(TOKeof, token.m_type);
  }
  {
    CXFA_FMLexer lexer(L"//");
    CXFA_FMToken token = lexer.NextToken();
    EXPECT_EQ(TOKeof, token.m_type);
  }
  {
    CXFA_FMLexer lexer(L"123 // Empty.\n\"str\"");
    CXFA_FMToken token = lexer.NextToken();
    EXPECT_EQ(TOKnumber, token.m_type);
    EXPECT_EQ(L"123", token.m_string);

    token = lexer.NextToken();
    EXPECT_EQ(TOKstring, token.m_type);
    EXPECT_EQ(L"\"str\"", token.m_string);

    token = lexer.NextToken();
    EXPECT_EQ(TOKeof, token.m_type);
  }
  {
    CXFA_FMLexer lexer(L";");
    CXFA_FMToken token = lexer.NextToken();
    EXPECT_EQ(TOKeof, token.m_type);
  }
  {
    CXFA_FMLexer lexer(L"; Empty.");
    CXFA_FMToken token = lexer.NextToken();
    EXPECT_EQ(TOKeof, token.m_type);
  }
  {
    CXFA_FMLexer lexer(L"123 ;Empty.\n\"str\"");
    CXFA_FMToken token = lexer.NextToken();
    EXPECT_EQ(TOKnumber, token.m_type);
    EXPECT_EQ(L"123", token.m_string);

    token = lexer.NextToken();
    EXPECT_EQ(TOKstring, token.m_type);
    EXPECT_EQ(L"\"str\"", token.m_string);

    token = lexer.NextToken();
    EXPECT_EQ(TOKeof, token.m_type);
    EXPECT_TRUE(lexer.IsComplete());
  }
}

TEST(CXFA_FMLexerTest, ValidIdentifiers) {
  std::vector<const wchar_t*> identifiers = {
      L"a", L"an_identifier", L"_ident", L"$ident", L"!ident", L"GetAddr"};
  for (const auto* ident : identifiers) {
    CXFA_FMLexer lexer(ident);
    CXFA_FMToken token = lexer.NextToken();
    EXPECT_EQ(TOKidentifier, token.m_type);
    EXPECT_EQ(ident, token.m_string);
    EXPECT_TRUE(lexer.IsComplete());
  }
}

TEST(CXFA_FMLexerTest, InvalidIdentifiers) {
  {
    CXFA_FMLexer lexer(L"#a");
    CXFA_FMToken token = lexer.NextToken();
    EXPECT_EQ(TOKreserver, token.m_type);
  }
  {
    CXFA_FMLexer lexer(L"1a");
    CXFA_FMToken token = lexer.NextToken();
    EXPECT_EQ(TOKreserver, token.m_type);
  }
  {
    CXFA_FMLexer lexer(L"an@identifier");
    CXFA_FMToken token = lexer.NextToken();
    EXPECT_NE(TOKreserver, token.m_type);
    token = lexer.NextToken();
    EXPECT_EQ(TOKreserver, token.m_type);
    token = lexer.NextToken();
    EXPECT_EQ(TOKreserver, token.m_type);
  }
  {
    CXFA_FMLexer lexer(L"_ident@");
    CXFA_FMToken token = lexer.NextToken();
    EXPECT_NE(TOKreserver, token.m_type);
    token = lexer.NextToken();
    EXPECT_EQ(TOKreserver, token.m_type);
    EXPECT_FALSE(lexer.IsComplete());
  }
}

TEST(CXFA_FMLexerTest, Whitespace) {
  {
    CXFA_FMLexer lexer(L" \t\xc\x9\xb");
    CXFA_FMToken token = lexer.NextToken();
    EXPECT_EQ(TOKeof, token.m_type);
  }
  {
    CXFA_FMLexer lexer(L"123 \t\xc\x9\xb 456");
    CXFA_FMToken token = lexer.NextToken();
    EXPECT_EQ(TOKnumber, token.m_type);
    EXPECT_EQ(L"123", token.m_string);

    token = lexer.NextToken();
    EXPECT_EQ(TOKnumber, token.m_type);
    EXPECT_EQ(L"456", token.m_string);

    token = lexer.NextToken();
    EXPECT_EQ(TOKeof, token.m_type);
    EXPECT_TRUE(lexer.IsComplete());
  }
}

TEST(CXFA_FMLexerTest, NullData) {
  CXFA_FMLexer lexer(WideStringView(L"\x2d\x32\x00\x2d\x32", 5));
  CXFA_FMToken token = lexer.NextToken();
  EXPECT_EQ(TOKminus, token.m_type);

  token = lexer.NextToken();
  EXPECT_EQ(TOKnumber, token.m_type);
  EXPECT_EQ(L"2", token.m_string);

  token = lexer.NextToken();
  EXPECT_EQ(TOKeof, token.m_type);
  EXPECT_FALSE(lexer.IsComplete());
}
