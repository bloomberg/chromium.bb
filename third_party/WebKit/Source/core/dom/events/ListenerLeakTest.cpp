/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/frame/FrameTestHelpers.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebView.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8-profiler.h"
#include "v8/include/v8.h"

namespace blink {

const v8::HeapGraphNode* GetProperty(v8::Isolate* isolate,
                                     const v8::HeapGraphNode* node,
                                     v8::HeapGraphEdge::Type type,
                                     const char* name) {
  for (int i = 0, count = node->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = node->GetChild(i);
    if (prop->GetType() == type) {
      v8::String::Utf8Value prop_name(isolate, prop->GetName());
      if (!strcmp(name, *prop_name))
        return prop->GetToNode();
    }
  }
  return nullptr;
}

int GetNumObjects(const char* constructor) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope scope(isolate);
  v8::HeapProfiler* profiler = isolate->GetHeapProfiler();
  const v8::HeapSnapshot* snapshot = profiler->TakeHeapSnapshot();
  if (!snapshot)
    return -1;
  int count = 0;
  for (int i = 0; i < snapshot->GetNodesCount(); ++i) {
    const v8::HeapGraphNode* node = snapshot->GetNode(i);
    if (node->GetType() != v8::HeapGraphNode::kObject)
      continue;
    v8::String::Utf8Value node_name(isolate, node->GetName());
    if (!strcmp(constructor, *node_name)) {
      const v8::HeapGraphNode* constructor_prop = GetProperty(
          isolate, node, v8::HeapGraphEdge::kProperty, "constructor");
      // Skip an Object instance named after the constructor.
      if (constructor_prop) {
        v8::String::Utf8Value constructor_name(isolate,
                                               constructor_prop->GetName());
        if (!strcmp(constructor, *constructor_name))
          continue;
      }
      ++count;
    }
  }
  return count;
}

class ListenerLeakTest : public ::testing::Test {
 public:
  void RunTest(const std::string& filename) {
    std::string base_url("http://www.example.com/");
    std::string file_name(filename);
    URLTestHelpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url), blink::testing::CoreTestDataPath(),
        WebString::FromUTF8(file_name));
    web_view_helper.InitializeAndLoad(base_url + file_name);
  }

  void TearDown() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

 protected:
  FrameTestHelpers::WebViewHelper web_view_helper;
};

// This test tries to create a reference cycle between node and its listener.
// See http://crbug/17400.
TEST_F(ListenerLeakTest, ReferenceCycle) {
  RunTest("listener/listener_leak1.html");
  ASSERT_EQ(0, GetNumObjects("EventListenerLeakTestObject1"));
}

// This test sets node onclick many times to expose a possible memory
// leak where all listeners get referenced by the node.
TEST_F(ListenerLeakTest, HiddenReferences) {
  RunTest("listener/listener_leak2.html");
  ASSERT_EQ(1, GetNumObjects("EventListenerLeakTestObject2"));
}

}  // namespace blink
