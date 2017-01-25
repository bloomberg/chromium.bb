// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationRequest.h"

#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/ExceptionCode.h"
#include "platform/weborigin/KURL.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

TEST(PresentationRequestTest, TestSingleUrlConstructor) {
  V8TestingScope scope;
  PresentationRequest* request = PresentationRequest::create(
      scope.getExecutionContext(), "https://example.com",
      scope.getExceptionState());
  ASSERT_FALSE(scope.getExceptionState().hadException());

  WTF::Vector<KURL> requestUrls = request->urls();
  EXPECT_EQ(static_cast<size_t>(1), requestUrls.size());
  EXPECT_TRUE(requestUrls[0].isValid());
  EXPECT_EQ("https://example.com/", requestUrls[0].getString());
}

TEST(PresentationRequestTest, TestMultipleUrlConstructor) {
  V8TestingScope scope;
  WTF::Vector<String> urls;
  urls.push_back("https://example.com");
  urls.push_back("cast://deadbeef?param=foo");

  PresentationRequest* request = PresentationRequest::create(
      scope.getExecutionContext(), urls, scope.getExceptionState());
  ASSERT_FALSE(scope.getExceptionState().hadException());

  WTF::Vector<KURL> requestUrls = request->urls();
  EXPECT_EQ(static_cast<size_t>(2), requestUrls.size());
  EXPECT_TRUE(requestUrls[0].isValid());
  EXPECT_EQ("https://example.com/", requestUrls[0].getString());
  EXPECT_TRUE(requestUrls[1].isValid());
  EXPECT_EQ("cast://deadbeef?param=foo", requestUrls[1].getString());
}

TEST(PresentationRequestTest, TestMultipleUrlConstructorInvalidURLFamily) {
  V8TestingScope scope;
  WTF::Vector<String> urls;
  urls.push_back("https://example.com");
  urls.push_back("about://deadbeef?param=foo");

  PresentationRequest::create(scope.getExecutionContext(), urls,
                              scope.getExceptionState());
  ASSERT_TRUE(scope.getExceptionState().hadException());

  EXPECT_EQ(SyntaxError, scope.getExceptionState().code());
}

TEST(PresentationRequestTest, TestMultipleUrlConstructorInvalidUrl) {
  V8TestingScope scope;
  WTF::Vector<String> urls;
  urls.push_back("https://example.com");
  urls.push_back("");

  PresentationRequest::create(scope.getExecutionContext(), urls,
                              scope.getExceptionState());
  EXPECT_TRUE(scope.getExceptionState().hadException());
  EXPECT_EQ(SyntaxError, scope.getExceptionState().code());
}

TEST(PresentationRequestTest, TestSingleUrlConstructorMixedContent) {
  V8TestingScope scope;
  scope.getExecutionContext()->securityContext().setSecurityOrigin(
      SecurityOrigin::createFromString("https://example.test"));

  PresentationRequest::create(scope.getExecutionContext(), "http://example.com",
                              scope.getExceptionState());
  EXPECT_TRUE(scope.getExceptionState().hadException());
  EXPECT_EQ(SecurityError, scope.getExceptionState().code());
}

TEST(PresentationRequestTest, TestMultipleUrlConstructorMixedContent) {
  V8TestingScope scope;
  scope.getExecutionContext()->securityContext().setSecurityOrigin(
      SecurityOrigin::createFromString("https://example.test"));

  WTF::Vector<String> urls;
  urls.push_back("http://example.com");
  urls.push_back("https://example1.com");

  PresentationRequest::create(scope.getExecutionContext(), urls,
                              scope.getExceptionState());
  EXPECT_TRUE(scope.getExceptionState().hadException());
  EXPECT_EQ(SecurityError, scope.getExceptionState().code());
}

TEST(PresentationRequestTest, TestMultipleUrlConstructorEmptySequence) {
  V8TestingScope scope;
  WTF::Vector<String> urls;

  PresentationRequest::create(scope.getExecutionContext(), urls,
                              scope.getExceptionState());
  EXPECT_TRUE(scope.getExceptionState().hadException());
  EXPECT_EQ(NotSupportedError, scope.getExceptionState().code());
}

}  // anonymous namespace
}  // namespace blink
