// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_html.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_html_or_trusted_script_or_trusted_script_url_or_trusted_url.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_script.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_script_url.h"
#include "third_party/blink/renderer/bindings/core/v8/usv_string_or_trusted_url.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_html.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script_url.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_url.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

// Functions for checking throwing cases.
void GetStringFromTrustedTypeThrows(
    const StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL&
        string_or_trusted_type) {
  Document* document = Document::CreateForTest();
  document->SetRequireTrustedTypes();
  DummyExceptionStateForTesting exception_state;
  ASSERT_FALSE(exception_state.HadException());
  String s = GetStringFromTrustedType(string_or_trusted_type, document,
                                      exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kTypeError, exception_state.CodeAs<ESErrorType>());
  exception_state.ClearException();
}

void GetStringFromTrustedHTMLThrows(
    const StringOrTrustedHTML& string_or_trusted_html) {
  Document* document = Document::CreateForTest();
  document->SetRequireTrustedTypes();
  DummyExceptionStateForTesting exception_state;
  ASSERT_FALSE(exception_state.HadException());
  String s = GetStringFromTrustedHTML(string_or_trusted_html, document,
                                      exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kTypeError, exception_state.CodeAs<ESErrorType>());
  exception_state.ClearException();
}

void GetStringFromTrustedScriptThrows(
    const StringOrTrustedScript& string_or_trusted_script) {
  Document* document = Document::CreateForTest();
  document->SetRequireTrustedTypes();
  DummyExceptionStateForTesting exception_state;
  ASSERT_FALSE(exception_state.HadException());
  String s = GetStringFromTrustedScript(string_or_trusted_script, document,
                                        exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kTypeError, exception_state.CodeAs<ESErrorType>());
  exception_state.ClearException();
}

void GetStringFromTrustedScriptURLThrows(
    const StringOrTrustedScriptURL& string_or_trusted_script_url) {
  Document* document = Document::CreateForTest();
  document->SetRequireTrustedTypes();
  DummyExceptionStateForTesting exception_state;
  ASSERT_FALSE(exception_state.HadException());
  String s = GetStringFromTrustedScriptURL(string_or_trusted_script_url,
                                           document, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kTypeError, exception_state.CodeAs<ESErrorType>());
  exception_state.ClearException();
}

void GetStringFromTrustedURLThrows(
    const USVStringOrTrustedURL& string_or_trusted_url) {
  Document* document = Document::CreateForTest();
  document->SetRequireTrustedTypes();
  DummyExceptionStateForTesting exception_state;
  ASSERT_FALSE(exception_state.HadException());
  String s =
      GetStringFromTrustedURL(string_or_trusted_url, document, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kTypeError, exception_state.CodeAs<ESErrorType>());
  exception_state.ClearException();
}

// Functions for checking non-throwing cases.
void GetStringFromTrustedTypeWorks(
    const StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL&
        string_or_trusted_type,
    String expected) {
  Document* document = Document::CreateForTest();
  document->SetRequireTrustedTypes();
  DummyExceptionStateForTesting exception_state;
  String s = GetStringFromTrustedType(string_or_trusted_type, document,
                                      exception_state);
  ASSERT_EQ(s, expected);
}

void GetStringFromTrustedHTMLWorks(
    const StringOrTrustedHTML& string_or_trusted_html,
    String expected) {
  Document* document = Document::CreateForTest();
  document->SetRequireTrustedTypes();
  DummyExceptionStateForTesting exception_state;
  String s = GetStringFromTrustedHTML(string_or_trusted_html, document,
                                      exception_state);
  ASSERT_EQ(s, expected);
}

void GetStringFromTrustedScriptWorks(
    const StringOrTrustedScript& string_or_trusted_script,
    String expected) {
  Document* document = Document::CreateForTest();
  document->SetRequireTrustedTypes();
  DummyExceptionStateForTesting exception_state;
  String s = GetStringFromTrustedScript(string_or_trusted_script, document,
                                        exception_state);
  ASSERT_EQ(s, expected);
}

void GetStringFromTrustedScriptURLWorks(
    const StringOrTrustedScriptURL& string_or_trusted_script_url,
    String expected) {
  Document* document = Document::CreateForTest();
  document->SetRequireTrustedTypes();
  DummyExceptionStateForTesting exception_state;
  String s = GetStringFromTrustedScriptURL(string_or_trusted_script_url,
                                           document, exception_state);
  ASSERT_EQ(s, expected);
}

void GetStringFromTrustedURLWorks(
    const USVStringOrTrustedURL& string_or_trusted_url,
    String expected) {
  Document* document = Document::CreateForTest();
  document->SetRequireTrustedTypes();
  DummyExceptionStateForTesting exception_state;
  String s =
      GetStringFromTrustedURL(string_or_trusted_url, document, exception_state);
  ASSERT_EQ(s, expected);
}

// GetStringFromTrustedType() tests
TEST(TrustedTypesUtilTest, GetStringFromTrustedType_TrustedHTML) {
  TrustedHTML* html = TrustedHTML::Create("A string");
  StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL
      trusted_value =
          StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL::
              FromTrustedHTML(html);
  GetStringFromTrustedTypeWorks(trusted_value, "A string");
}

TEST(TrustedTypesUtilTest, GetStringFromTrustedType_TrustedScript) {
  TrustedScript* script = TrustedScript::Create("A string");
  StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL
      trusted_value =
          StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL::
              FromTrustedScript(script);
  GetStringFromTrustedTypeWorks(trusted_value, "A string");
}

TEST(TrustedTypesUtilTest, GetStringFromTrustedType_TrustedScriptURL) {
  KURL url_address("http://www.example.com/");
  TrustedScriptURL* script_url = TrustedScriptURL::Create(url_address);
  StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL
      trusted_value =
          StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL::
              FromTrustedScriptURL(script_url);
  GetStringFromTrustedTypeWorks(trusted_value, "http://www.example.com/");
}

TEST(TrustedTypesUtilTest, GetStringFromTrustedType_TrustedURL) {
  KURL url_address("http://www.example.com/");
  TrustedURL* url = TrustedURL::Create(url_address);
  StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL
      trusted_value =
          StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL::
              FromTrustedURL(url);
  GetStringFromTrustedTypeWorks(trusted_value, "http://www.example.com/");
}

TEST(TrustedTypesUtilTest, GetStringFromTrustedType_String) {
  StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL
      string_value =
          StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL::
              FromString("A string");
  GetStringFromTrustedTypeThrows(string_value);
}

// GetStringFromTrustedTypeWithoutCheck() tests
TEST(TrustedTypesUtilTest, GetStringFromTrustedTypeWithoutCheck_TrustedHTML) {
  TrustedHTML* html = TrustedHTML::Create("A string");
  StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL
      trusted_value =
          StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL::
              FromTrustedHTML(html);
  String s = GetStringFromTrustedTypeWithoutCheck(trusted_value);
  ASSERT_EQ(s, "A string");
}

TEST(TrustedTypesUtilTest, GetStringFromTrustedTypeWithoutCheck_TrustedScript) {
  TrustedScript* script = TrustedScript::Create("A string");
  StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL
      trusted_value =
          StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL::
              FromTrustedScript(script);
  String s = GetStringFromTrustedTypeWithoutCheck(trusted_value);
  ASSERT_EQ(s, "A string");
}

TEST(TrustedTypesUtilTest,
     GetStringFromTrustedTypeWithoutCheck_TrustedScriptURL) {
  KURL url_address("http://www.example.com/");
  TrustedScriptURL* script_url = TrustedScriptURL::Create(url_address);
  StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL
      trusted_value =
          StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL::
              FromTrustedScriptURL(script_url);
  String s = GetStringFromTrustedTypeWithoutCheck(trusted_value);
  ASSERT_EQ(s, "http://www.example.com/");
}

TEST(TrustedTypesUtilTest, GetStringFromTrustedTypeWithoutCheck_TrustedURL) {
  KURL url_address("http://www.example.com/");
  TrustedURL* url = TrustedURL::Create(url_address);
  StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL
      trusted_value =
          StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL::
              FromTrustedURL(url);
  String s = GetStringFromTrustedTypeWithoutCheck(trusted_value);
  ASSERT_EQ(s, "http://www.example.com/");
}

TEST(TrustedTypesUtilTest, GetStringFromTrustedTypeWithoutCheck_String) {
  StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL
      string_value =
          StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL::
              FromString("A string");
  String s = GetStringFromTrustedTypeWithoutCheck(string_value);
  ASSERT_EQ(s, "A string");
}

TEST(TrustedTypesUtilTest, GetStringFromTrustedTypeWithoutCheck_Null) {
  StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL null_value =
      StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL();
  String s = GetStringFromTrustedTypeWithoutCheck(null_value);
  ASSERT_EQ(s, "");
}

// GetStringFromTrustedHTML tests
TEST(TrustedTypesUtilTest, GetStringFromTrustedHTML_TrustedHTML) {
  TrustedHTML* html = TrustedHTML::Create("A string");
  StringOrTrustedHTML trusted_value =
      StringOrTrustedHTML::FromTrustedHTML(html);
  GetStringFromTrustedHTMLWorks(trusted_value, "A string");
}

TEST(TrustedTypesUtilTest, GetStringFromTrustedHTML_String) {
  StringOrTrustedHTML string_value =
      StringOrTrustedHTML::FromString("A string");
  GetStringFromTrustedHTMLThrows(string_value);
}

// GetStringFromTrustedScript tests
TEST(TrustedTypesUtilTest, GetStringFromTrustedScript_TrustedScript) {
  TrustedScript* script = TrustedScript::Create("A string");
  StringOrTrustedScript trusted_value =
      StringOrTrustedScript::FromTrustedScript(script);
  GetStringFromTrustedScriptWorks(trusted_value, "A string");
}

TEST(TrustedTypesUtilTest, GetStringFromTrustedScript_String) {
  StringOrTrustedScript string_value =
      StringOrTrustedScript::FromString("A string");
  GetStringFromTrustedScriptThrows(string_value);
}

// GetStringFromTrustedScriptURL tests
TEST(TrustedTypesUtilTest, GetStringFromTrustedScriptURL_TrustedScriptURL) {
  KURL url_address("http://www.example.com/");
  TrustedScriptURL* script_url = TrustedScriptURL::Create(url_address);
  StringOrTrustedScriptURL trusted_value =
      StringOrTrustedScriptURL::FromTrustedScriptURL(script_url);
  GetStringFromTrustedScriptURLWorks(trusted_value, "http://www.example.com/");
}

TEST(TrustedTypesUtilTest, GetStringFromTrustedScriptURL_String) {
  StringOrTrustedScriptURL string_value =
      StringOrTrustedScriptURL::FromString("A string");
  GetStringFromTrustedScriptURLThrows(string_value);
}

// GetStringFromTrustedURL tests
TEST(TrustedTypesUtilTest, GetStringFromTrustedURL_TrustedURL) {
  KURL url_address("http://www.example.com/");
  TrustedURL* url = TrustedURL::Create(url_address);
  USVStringOrTrustedURL trusted_value =
      USVStringOrTrustedURL::FromTrustedURL(url);
  GetStringFromTrustedURLWorks(trusted_value, "http://www.example.com/");
}

TEST(TrustedTypesUtilTest, GetStringFromTrustedURL_String) {
  USVStringOrTrustedURL string_value =
      USVStringOrTrustedURL::FromUSVString("A string");
  GetStringFromTrustedURLThrows(string_value);
}
}  // namespace blink
