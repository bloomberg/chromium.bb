// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/Response.h"

#include <memory>
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/Frame.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/BytesConsumer.h"
#include "modules/fetch/BytesConsumerTestUtil.h"
#include "modules/fetch/DataConsumerHandleTestUtil.h"
#include "modules/fetch/FetchResponseData.h"
#include "platform/bindings/ScriptState.h"
#include "platform/blob/BlobData.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Vector.h"
#include "public/platform/modules/fetch/fetch_api_request.mojom-blink.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponse.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

std::unique_ptr<WebServiceWorkerResponse> CreateTestWebServiceWorkerResponse() {
  const KURL url(kParsedURLString, "http://www.webresponse.com/");
  const unsigned short kStatus = 200;
  const String status_text = "the best status text";
  struct {
    const char* key;
    const char* value;
  } headers[] = {{"cache-control", "no-cache"},
                 {"set-cookie", "foop"},
                 {"foo", "bar"},
                 {0, 0}};
  Vector<WebURL> url_list;
  url_list.push_back(url);
  std::unique_ptr<WebServiceWorkerResponse> web_response =
      WTF::MakeUnique<WebServiceWorkerResponse>();
  web_response->SetURLList(url_list);
  web_response->SetStatus(kStatus);
  web_response->SetStatusText(status_text);
  web_response->SetResponseType(network::mojom::FetchResponseType::kDefault);
  for (int i = 0; headers[i].key; ++i)
    web_response->SetHeader(WebString::FromUTF8(headers[i].key),
                            WebString::FromUTF8(headers[i].value));
  return web_response;
}

TEST(ServiceWorkerResponseTest, FromFetchResponseData) {
  std::unique_ptr<DummyPageHolder> page =
      DummyPageHolder::Create(IntSize(1, 1));
  const KURL url(kParsedURLString, "http://www.response.com");

  FetchResponseData* fetch_response_data = FetchResponseData::Create();
  Vector<KURL> url_list;
  url_list.push_back(url);
  fetch_response_data->SetURLList(url_list);
  Response* response =
      Response::Create(&page->GetDocument(), fetch_response_data);
  DCHECK(response);
  EXPECT_EQ(url, response->url());
}

TEST(ServiceWorkerResponseTest, FromWebServiceWorkerResponse) {
  V8TestingScope scope;
  std::unique_ptr<WebServiceWorkerResponse> web_response =
      CreateTestWebServiceWorkerResponse();
  Response* response = Response::Create(scope.GetScriptState(), *web_response);
  DCHECK(response);
  ASSERT_EQ(1u, web_response->UrlList().size());
  EXPECT_EQ(web_response->UrlList()[0], response->url());
  EXPECT_EQ(web_response->Status(), response->status());
  EXPECT_STREQ(web_response->StatusText().Utf8().c_str(),
               response->statusText().Utf8().data());

  Headers* response_headers = response->headers();

  WebVector<WebString> keys = web_response->GetHeaderKeys();
  EXPECT_EQ(keys.size(), response_headers->HeaderList()->size());
  for (size_t i = 0, max = keys.size(); i < max; ++i) {
    WebString key = keys[i];
    DummyExceptionStateForTesting exception_state;
    EXPECT_STREQ(web_response->GetHeader(key).Utf8().c_str(),
                 response_headers->get(key, exception_state).Utf8().data());
    EXPECT_FALSE(exception_state.HadException());
  }
}

TEST(ServiceWorkerResponseTest, FromWebServiceWorkerResponseDefault) {
  V8TestingScope scope;
  std::unique_ptr<WebServiceWorkerResponse> web_response =
      CreateTestWebServiceWorkerResponse();
  web_response->SetResponseType(network::mojom::FetchResponseType::kDefault);
  Response* response = Response::Create(scope.GetScriptState(), *web_response);

  Headers* response_headers = response->headers();
  DummyExceptionStateForTesting exception_state;
  EXPECT_STREQ(
      "foop",
      response_headers->get("set-cookie", exception_state).Utf8().data());
  EXPECT_STREQ("bar",
               response_headers->get("foo", exception_state).Utf8().data());
  EXPECT_STREQ(
      "no-cache",
      response_headers->get("cache-control", exception_state).Utf8().data());
  EXPECT_FALSE(exception_state.HadException());
}

TEST(ServiceWorkerResponseTest, FromWebServiceWorkerResponseBasic) {
  V8TestingScope scope;
  std::unique_ptr<WebServiceWorkerResponse> web_response =
      CreateTestWebServiceWorkerResponse();
  web_response->SetResponseType(network::mojom::FetchResponseType::kBasic);
  Response* response = Response::Create(scope.GetScriptState(), *web_response);

  Headers* response_headers = response->headers();
  DummyExceptionStateForTesting exception_state;
  EXPECT_STREQ(
      "", response_headers->get("set-cookie", exception_state).Utf8().data());
  EXPECT_STREQ("bar",
               response_headers->get("foo", exception_state).Utf8().data());
  EXPECT_STREQ(
      "no-cache",
      response_headers->get("cache-control", exception_state).Utf8().data());
  EXPECT_FALSE(exception_state.HadException());
}

TEST(ServiceWorkerResponseTest, FromWebServiceWorkerResponseCORS) {
  V8TestingScope scope;
  std::unique_ptr<WebServiceWorkerResponse> web_response =
      CreateTestWebServiceWorkerResponse();
  web_response->SetResponseType(network::mojom::FetchResponseType::kCORS);
  Response* response = Response::Create(scope.GetScriptState(), *web_response);

  Headers* response_headers = response->headers();
  DummyExceptionStateForTesting exception_state;
  EXPECT_STREQ(
      "", response_headers->get("set-cookie", exception_state).Utf8().data());
  EXPECT_STREQ("", response_headers->get("foo", exception_state).Utf8().data());
  EXPECT_STREQ(
      "no-cache",
      response_headers->get("cache-control", exception_state).Utf8().data());
  EXPECT_FALSE(exception_state.HadException());
}

TEST(ServiceWorkerResponseTest, FromWebServiceWorkerResponseOpaque) {
  V8TestingScope scope;
  std::unique_ptr<WebServiceWorkerResponse> web_response =
      CreateTestWebServiceWorkerResponse();
  web_response->SetResponseType(network::mojom::FetchResponseType::kOpaque);
  Response* response = Response::Create(scope.GetScriptState(), *web_response);

  Headers* response_headers = response->headers();
  DummyExceptionStateForTesting exception_state;
  EXPECT_STREQ(
      "", response_headers->get("set-cookie", exception_state).Utf8().data());
  EXPECT_STREQ("", response_headers->get("foo", exception_state).Utf8().data());
  EXPECT_STREQ(
      "",
      response_headers->get("cache-control", exception_state).Utf8().data());
  EXPECT_FALSE(exception_state.HadException());
}

void CheckResponseStream(ScriptState* script_state,
                         Response* response,
                         bool check_response_body_stream_buffer) {
  BodyStreamBuffer* original_internal = response->InternalBodyBuffer();
  if (check_response_body_stream_buffer) {
    EXPECT_EQ(response->BodyBuffer(), original_internal);
  } else {
    EXPECT_FALSE(response->BodyBuffer());
  }

  DummyExceptionStateForTesting exception_state;
  Response* cloned_response = response->clone(script_state, exception_state);
  EXPECT_FALSE(exception_state.HadException());

  if (!response->InternalBodyBuffer())
    FAIL() << "internalBodyBuffer() must not be null.";
  if (!cloned_response->InternalBodyBuffer())
    FAIL() << "internalBodyBuffer() must not be null.";
  EXPECT_TRUE(response->InternalBodyBuffer());
  EXPECT_TRUE(cloned_response->InternalBodyBuffer());
  EXPECT_TRUE(response->InternalBodyBuffer());
  EXPECT_TRUE(cloned_response->InternalBodyBuffer());
  EXPECT_NE(response->InternalBodyBuffer(), original_internal);
  EXPECT_NE(cloned_response->InternalBodyBuffer(), original_internal);
  EXPECT_NE(response->InternalBodyBuffer(),
            cloned_response->InternalBodyBuffer());
  if (check_response_body_stream_buffer) {
    EXPECT_EQ(response->BodyBuffer(), response->InternalBodyBuffer());
    EXPECT_EQ(cloned_response->BodyBuffer(),
              cloned_response->InternalBodyBuffer());
  } else {
    EXPECT_FALSE(response->BodyBuffer());
    EXPECT_FALSE(cloned_response->BodyBuffer());
  }
  BytesConsumerTestUtil::MockFetchDataLoaderClient* client1 =
      new BytesConsumerTestUtil::MockFetchDataLoaderClient();
  BytesConsumerTestUtil::MockFetchDataLoaderClient* client2 =
      new BytesConsumerTestUtil::MockFetchDataLoaderClient();
  EXPECT_CALL(*client1, DidFetchDataLoadedString(String("Hello, world")));
  EXPECT_CALL(*client2, DidFetchDataLoadedString(String("Hello, world")));

  response->InternalBodyBuffer()->StartLoading(
      FetchDataLoader::CreateLoaderAsString(), client1);
  cloned_response->InternalBodyBuffer()->StartLoading(
      FetchDataLoader::CreateLoaderAsString(), client2);
  blink::testing::RunPendingTasks();
}

BodyStreamBuffer* CreateHelloWorldBuffer(ScriptState* script_state) {
  using BytesConsumerCommand = BytesConsumerTestUtil::Command;
  BytesConsumerTestUtil::ReplayingBytesConsumer* src =
      new BytesConsumerTestUtil::ReplayingBytesConsumer(
          ExecutionContext::From(script_state));
  src->Add(BytesConsumerCommand(BytesConsumerCommand::kData, "Hello, "));
  src->Add(BytesConsumerCommand(BytesConsumerCommand::kData, "world"));
  src->Add(BytesConsumerCommand(BytesConsumerCommand::kDone));
  return new BodyStreamBuffer(script_state, src);
}

TEST(ServiceWorkerResponseTest, BodyStreamBufferCloneDefault) {
  V8TestingScope scope;
  BodyStreamBuffer* buffer = CreateHelloWorldBuffer(scope.GetScriptState());
  FetchResponseData* fetch_response_data =
      FetchResponseData::CreateWithBuffer(buffer);
  Vector<KURL> url_list;
  url_list.push_back(KURL(kParsedURLString, "http://www.response.com"));
  fetch_response_data->SetURLList(url_list);
  Response* response =
      Response::Create(scope.GetExecutionContext(), fetch_response_data);
  EXPECT_EQ(response->InternalBodyBuffer(), buffer);
  CheckResponseStream(scope.GetScriptState(), response, true);
}

TEST(ServiceWorkerResponseTest, BodyStreamBufferCloneBasic) {
  V8TestingScope scope;
  BodyStreamBuffer* buffer = CreateHelloWorldBuffer(scope.GetScriptState());
  FetchResponseData* fetch_response_data =
      FetchResponseData::CreateWithBuffer(buffer);
  Vector<KURL> url_list;
  url_list.push_back(KURL(kParsedURLString, "http://www.response.com"));
  fetch_response_data->SetURLList(url_list);
  fetch_response_data = fetch_response_data->CreateBasicFilteredResponse();
  Response* response =
      Response::Create(scope.GetExecutionContext(), fetch_response_data);
  EXPECT_EQ(response->InternalBodyBuffer(), buffer);
  CheckResponseStream(scope.GetScriptState(), response, true);
}

TEST(ServiceWorkerResponseTest, BodyStreamBufferCloneCORS) {
  V8TestingScope scope;
  BodyStreamBuffer* buffer = CreateHelloWorldBuffer(scope.GetScriptState());
  FetchResponseData* fetch_response_data =
      FetchResponseData::CreateWithBuffer(buffer);
  Vector<KURL> url_list;
  url_list.push_back(KURL(kParsedURLString, "http://www.response.com"));
  fetch_response_data->SetURLList(url_list);
  fetch_response_data = fetch_response_data->CreateCORSFilteredResponse();
  Response* response =
      Response::Create(scope.GetExecutionContext(), fetch_response_data);
  EXPECT_EQ(response->InternalBodyBuffer(), buffer);
  CheckResponseStream(scope.GetScriptState(), response, true);
}

TEST(ServiceWorkerResponseTest, BodyStreamBufferCloneOpaque) {
  V8TestingScope scope;
  BodyStreamBuffer* buffer = CreateHelloWorldBuffer(scope.GetScriptState());
  FetchResponseData* fetch_response_data =
      FetchResponseData::CreateWithBuffer(buffer);
  Vector<KURL> url_list;
  url_list.push_back(KURL(kParsedURLString, "http://www.response.com"));
  fetch_response_data->SetURLList(url_list);
  fetch_response_data = fetch_response_data->CreateOpaqueFilteredResponse();
  Response* response =
      Response::Create(scope.GetExecutionContext(), fetch_response_data);
  EXPECT_EQ(response->InternalBodyBuffer(), buffer);
  CheckResponseStream(scope.GetScriptState(), response, false);
}

TEST(ServiceWorkerResponseTest, BodyStreamBufferCloneError) {
  V8TestingScope scope;
  BodyStreamBuffer* buffer = new BodyStreamBuffer(
      scope.GetScriptState(),
      BytesConsumer::CreateErrored(BytesConsumer::Error()));
  FetchResponseData* fetch_response_data =
      FetchResponseData::CreateWithBuffer(buffer);
  Vector<KURL> url_list;
  url_list.push_back(KURL(kParsedURLString, "http://www.response.com"));
  fetch_response_data->SetURLList(url_list);
  Response* response =
      Response::Create(scope.GetExecutionContext(), fetch_response_data);
  DummyExceptionStateForTesting exception_state;
  Response* cloned_response =
      response->clone(scope.GetScriptState(), exception_state);
  EXPECT_FALSE(exception_state.HadException());

  BytesConsumerTestUtil::MockFetchDataLoaderClient* client1 =
      new BytesConsumerTestUtil::MockFetchDataLoaderClient();
  BytesConsumerTestUtil::MockFetchDataLoaderClient* client2 =
      new BytesConsumerTestUtil::MockFetchDataLoaderClient();
  EXPECT_CALL(*client1, DidFetchDataLoadFailed());
  EXPECT_CALL(*client2, DidFetchDataLoadFailed());

  response->InternalBodyBuffer()->StartLoading(
      FetchDataLoader::CreateLoaderAsString(), client1);
  cloned_response->InternalBodyBuffer()->StartLoading(
      FetchDataLoader::CreateLoaderAsString(), client2);
  blink::testing::RunPendingTasks();
}

}  // namespace
}  // namespace blink
