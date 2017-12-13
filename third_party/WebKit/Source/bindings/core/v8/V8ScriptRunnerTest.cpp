// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8ScriptRunner.h"

#include "bindings/core/v8/ReferrerScriptInfo.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/loader/resource/ScriptResource.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/CachedMetadata.h"
#include "platform/loader/fetch/CachedMetadataHandler.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/text/TextEncoding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class V8ScriptRunnerTest : public ::testing::Test {
 public:
  V8ScriptRunnerTest() {}
  ~V8ScriptRunnerTest() override {}

  void SetUp() override {
    // To trick various layers of caching, increment a counter for each
    // test and use it in Code() and Url().
    counter_++;
  }

  WTF::String Code() const {
    // Simple function for testing. Note:
    // - Add counter to trick V8 code cache.
    // - Pad counter to 1000 digits, to trick minimal cacheability threshold.
    return WTF::String::Format("a = function() { 1 + 1; } // %01000d\n",
                               counter_);
  }
  KURL Url() const {
    return KURL(WTF::String::Format("http://bla.com/bla%d", counter_));
  }
  unsigned TagForParserCache(CachedMetadataHandler* cache_handler) const {
    return V8ScriptRunner::TagForParserCache(cache_handler);
  }
  unsigned TagForCodeCache(CachedMetadataHandler* cache_handler) const {
    return V8ScriptRunner::TagForCodeCache(cache_handler);
  }
  void SetCacheTimeStamp(CachedMetadataHandler* cache_handler) {
    V8ScriptRunner::SetCacheTimeStamp(cache_handler);
  }

  bool CompileScript(ScriptState* script_state,
                     const ScriptSourceCode& source_code,
                     V8CacheOptions cache_options) {
    return !V8ScriptRunner::CompileScript(script_state, source_code,
                                          kNotSharableCrossOrigin,
                                          cache_options, ReferrerScriptInfo())
                .IsEmpty();
  }

  ScriptResource* CreateEmptyResource() {
    return ScriptResource::CreateForTest(NullURL(), UTF8Encoding());
  }

  ScriptResource* CreateResource(const WTF::TextEncoding& encoding) {
    ScriptResource* resource = ScriptResource::CreateForTest(Url(), encoding);
    String code = Code();
    ResourceResponse response(Url());
    response.SetHTTPStatusCode(200);
    resource->SetResponse(response);
    resource->AppendData(code.Utf8().data(), code.Utf8().length());
    resource->FinishForTest();
    return resource;
  }

 protected:
  static int counter_;
};

int V8ScriptRunnerTest::counter_ = 0;

TEST_F(V8ScriptRunnerTest, resourcelessShouldPass) {
  V8TestingScope scope;
  ScriptSourceCode source_code(Code(), ScriptSourceLocationType::kInternal,
                               nullptr /* cache_handler */, Url());
  EXPECT_TRUE(
      CompileScript(scope.GetScriptState(), source_code, kV8CacheOptionsNone));
  EXPECT_TRUE(
      CompileScript(scope.GetScriptState(), source_code, kV8CacheOptionsParse));
  EXPECT_TRUE(
      CompileScript(scope.GetScriptState(), source_code, kV8CacheOptionsCode));
}

TEST_F(V8ScriptRunnerTest, emptyResourceDoesNotHaveCacheHandler) {
  Resource* resource = CreateEmptyResource();
  EXPECT_FALSE(resource->CacheHandler());
}

TEST_F(V8ScriptRunnerTest, parseOption) {
  V8TestingScope scope;
  ScriptSourceCode source_code(nullptr, CreateResource(UTF8Encoding()));
  EXPECT_TRUE(
      CompileScript(scope.GetScriptState(), source_code, kV8CacheOptionsParse));
  CachedMetadataHandler* cache_handler = source_code.CacheHandler();
  EXPECT_TRUE(
      cache_handler->GetCachedMetadata(TagForParserCache(cache_handler)));
  EXPECT_FALSE(
      cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler)));
  // The cached data is associated with the encoding.
  ScriptResource* another_resource =
      CreateResource(UTF16LittleEndianEncoding());
  EXPECT_FALSE(cache_handler->GetCachedMetadata(
      TagForParserCache(another_resource->CacheHandler())));
}

TEST_F(V8ScriptRunnerTest, codeOption) {
  V8TestingScope scope;
  ScriptSourceCode source_code(nullptr, CreateResource(UTF8Encoding()));
  CachedMetadataHandler* cache_handler = source_code.CacheHandler();
  SetCacheTimeStamp(cache_handler);

  EXPECT_TRUE(
      CompileScript(scope.GetScriptState(), source_code, kV8CacheOptionsCode));

  EXPECT_FALSE(
      cache_handler->GetCachedMetadata(TagForParserCache(cache_handler)));
  EXPECT_TRUE(cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler)));
  // The cached data is associated with the encoding.
  ScriptResource* another_resource =
      CreateResource(UTF16LittleEndianEncoding());
  EXPECT_FALSE(cache_handler->GetCachedMetadata(
      TagForCodeCache(another_resource->CacheHandler())));
}

}  // namespace

}  // namespace blink
