// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/BackgroundFetchManager.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/modules/v8/RequestOrUSVString.h"
#include "bindings/modules/v8/RequestOrUSVStringOrRequestOrUSVStringSequence.h"
#include "core/dom/ExceptionCode.h"
#include "modules/fetch/Request.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRequest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class BackgroundFetchManagerTest : public ::testing::Test {
 protected:
  // Creates a vector of WebServiceWorkerRequest entries for the given
  // |requests| based on the |scope|. Proxied in the fixture to reduce the
  // number of friend declarations necessary in the BackgroundFetchManager.
  Vector<WebServiceWorkerRequest> createWebRequestVector(
      V8TestingScope& scope,
      const RequestOrUSVStringOrRequestOrUSVStringSequence& requests) {
    return BackgroundFetchManager::createWebRequestVector(
        scope.getScriptState(), requests, scope.getExceptionState());
  }

  // Returns a Dictionary object that represents a JavaScript dictionary with
  // a single key-value pair, where the key always is "method" with the value
  // set to |method|.
  Dictionary getDictionaryForMethod(V8TestingScope& scope, const char* method) {
    v8::Isolate* isolate = scope.isolate();
    v8::Local<v8::Object> data = v8::Object::New(isolate);

    data->Set(isolate->GetCurrentContext(), v8String(isolate, "method"),
              v8String(isolate, method))
        .ToChecked();

    return Dictionary(scope.isolate(), data, scope.getExceptionState());
  }
};

TEST_F(BackgroundFetchManagerTest, NullValue) {
  V8TestingScope scope;

  RequestOrUSVStringOrRequestOrUSVStringSequence requests;

  Vector<WebServiceWorkerRequest> webRequests =
      createWebRequestVector(scope, requests);
  ASSERT_TRUE(scope.getExceptionState().hadException());
  EXPECT_EQ(scope.getExceptionState().code(), V8TypeError);
}

TEST_F(BackgroundFetchManagerTest, SingleUSVString) {
  V8TestingScope scope;

  KURL imageUrl(ParsedURLString, "https://www.example.com/my_image.png");

  RequestOrUSVStringOrRequestOrUSVStringSequence requests =
      RequestOrUSVStringOrRequestOrUSVStringSequence::fromUSVString(
          imageUrl.getString());

  Vector<WebServiceWorkerRequest> webRequests =
      createWebRequestVector(scope, requests);
  ASSERT_FALSE(scope.getExceptionState().hadException());

  ASSERT_EQ(webRequests.size(), 1u);

  WebServiceWorkerRequest& webRequest = webRequests[0];
  EXPECT_EQ(webRequest.url(), WebURL(imageUrl));
  EXPECT_EQ(webRequest.method(), "GET");
}

TEST_F(BackgroundFetchManagerTest, SingleRequest) {
  V8TestingScope scope;

  KURL imageUrl(ParsedURLString, "https://www.example.com/my_image.png");

  Request* request = Request::create(
      scope.getScriptState(), imageUrl.getString(),
      getDictionaryForMethod(scope, "POST"), scope.getExceptionState());
  ASSERT_FALSE(scope.getExceptionState().hadException());
  ASSERT_TRUE(request);

  RequestOrUSVStringOrRequestOrUSVStringSequence requests =
      RequestOrUSVStringOrRequestOrUSVStringSequence::fromRequest(request);

  Vector<WebServiceWorkerRequest> webRequests =
      createWebRequestVector(scope, requests);
  ASSERT_FALSE(scope.getExceptionState().hadException());

  ASSERT_EQ(webRequests.size(), 1u);

  WebServiceWorkerRequest& webRequest = webRequests[0];
  EXPECT_EQ(webRequest.url(), WebURL(imageUrl));
  EXPECT_EQ(webRequest.method(), "POST");
}

TEST_F(BackgroundFetchManagerTest, Sequence) {
  V8TestingScope scope;

  KURL imageUrl(ParsedURLString, "https://www.example.com/my_image.png");
  KURL iconUrl(ParsedURLString, "https://www.example.com/my_icon.jpg");
  KURL catVideoUrl(ParsedURLString, "https://www.example.com/my_cat_video.avi");

  RequestOrUSVString imageRequest =
      RequestOrUSVString::fromUSVString(imageUrl.getString());
  RequestOrUSVString iconRequest =
      RequestOrUSVString::fromUSVString(iconUrl.getString());

  Request* request = Request::create(
      scope.getScriptState(), catVideoUrl.getString(),
      getDictionaryForMethod(scope, "DELETE"), scope.getExceptionState());
  ASSERT_FALSE(scope.getExceptionState().hadException());
  ASSERT_TRUE(request);

  RequestOrUSVString catVideoRequest = RequestOrUSVString::fromRequest(request);

  HeapVector<RequestOrUSVString> requestSequence;
  requestSequence.push_back(imageRequest);
  requestSequence.push_back(iconRequest);
  requestSequence.push_back(catVideoRequest);

  RequestOrUSVStringOrRequestOrUSVStringSequence requests =
      RequestOrUSVStringOrRequestOrUSVStringSequence::
          fromRequestOrUSVStringSequence(requestSequence);

  Vector<WebServiceWorkerRequest> webRequests =
      createWebRequestVector(scope, requests);
  ASSERT_FALSE(scope.getExceptionState().hadException());

  ASSERT_EQ(webRequests.size(), 3u);
  EXPECT_EQ(webRequests[0].url(), WebURL(imageUrl));
  EXPECT_EQ(webRequests[0].method(), "GET");

  EXPECT_EQ(webRequests[1].url(), WebURL(iconUrl));
  EXPECT_EQ(webRequests[1].method(), "GET");

  EXPECT_EQ(webRequests[2].url(), WebURL(catVideoUrl));
  EXPECT_EQ(webRequests[2].method(), "DELETE");
}

TEST_F(BackgroundFetchManagerTest, SequenceEmpty) {
  V8TestingScope scope;

  HeapVector<RequestOrUSVString> requestSequence;
  RequestOrUSVStringOrRequestOrUSVStringSequence requests =
      RequestOrUSVStringOrRequestOrUSVStringSequence::
          fromRequestOrUSVStringSequence(requestSequence);

  Vector<WebServiceWorkerRequest> webRequests =
      createWebRequestVector(scope, requests);
  ASSERT_TRUE(scope.getExceptionState().hadException());
  EXPECT_EQ(scope.getExceptionState().code(), V8TypeError);
}

TEST_F(BackgroundFetchManagerTest, SequenceWithNullValue) {
  V8TestingScope scope;

  KURL imageUrl(ParsedURLString, "https://www.example.com/my_image.png");

  RequestOrUSVString nullRequest;
  RequestOrUSVString imageRequest =
      RequestOrUSVString::fromUSVString(imageUrl.getString());

  HeapVector<RequestOrUSVString> requestSequence;
  requestSequence.push_back(imageRequest);
  requestSequence.push_back(nullRequest);

  RequestOrUSVStringOrRequestOrUSVStringSequence requests =
      RequestOrUSVStringOrRequestOrUSVStringSequence::
          fromRequestOrUSVStringSequence(requestSequence);

  Vector<WebServiceWorkerRequest> webRequests =
      createWebRequestVector(scope, requests);
  ASSERT_TRUE(scope.getExceptionState().hadException());
  EXPECT_EQ(scope.getExceptionState().code(), V8TypeError);
}

}  // namespace blink
