// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/SerializedScriptValue.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "core/dom/MessagePort.h"
#include "core/frame/Settings.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/testing/BlinkFuzzerTestSupport.h"
#include "public/platform/WebBlobInfo.h"
#include "public/platform/WebMessagePortChannel.h"
#include "v8/include/v8.h"
#include "wtf/StringHasher.h"

namespace blink {

namespace {

// Intentionally leaked during fuzzing.
// See testing/libfuzzer/efficient_fuzzer.md.
DummyPageHolder* pageHolder = nullptr;
WebBlobInfoArray* blobInfoArray = nullptr;

enum : uint32_t {
  kFuzzMessagePorts = 1 << 0,
  kFuzzBlobInfo = 1 << 1,
};

class WebMessagePortChannelImpl final : public WebMessagePortChannel {
 public:
  // WebMessagePortChannel
  void setClient(WebMessagePortChannelClient* client) override {}
  void postMessage(const WebString&, WebMessagePortChannelArray) {
    NOTIMPLEMENTED();
  }
  bool tryGetMessage(WebString*, WebMessagePortChannelArray&) { return false; }
};

}  // namespace

int LLVMFuzzerInitialize(int* argc, char*** argv) {
  const char kExposeGC[] = "--expose_gc";
  v8::V8::SetFlagsFromString(kExposeGC, sizeof(kExposeGC));
  InitializeBlinkFuzzTest(argc, argv);
  pageHolder = DummyPageHolder::create().release();
  pageHolder->frame().settings()->setScriptEnabled(true);
  blobInfoArray = new WebBlobInfoArray();
  blobInfoArray->emplace_back("d875dfc2-4505-461b-98fe-0cf6cc5eaf44",
                              "text/plain", 12);
  blobInfoArray->emplace_back("d875dfc2-4505-461b-98fe-0cf6cc5eaf44",
                              "/native/path", "path", "text/plain");
  return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Odd sizes are handled in various ways, depending how they arrive.
  // Let's not worry about that case here.
  if (size % sizeof(UChar))
    return 0;

  // Used to control what kind of extra data is provided to the deserializer.
  unsigned hash = StringHasher::hashMemory(data, size);

  // If message ports are requested, make some.
  MessagePortArray* messagePorts = nullptr;
  if (hash & kFuzzMessagePorts) {
    messagePorts = new MessagePortArray(3);
    std::generate(messagePorts->begin(), messagePorts->end(), []() {
      WebMessagePortChannelUniquePtr channel(new WebMessagePortChannelImpl());
      MessagePort* port = MessagePort::create(pageHolder->document());
      port->entangle(std::move(channel));
      return port;
    });
  }

  // If blobs are requested, supply blob info.
  const auto* blobs = (hash & kFuzzBlobInfo) ? blobInfoArray : nullptr;

  // Set up.
  ScriptState* scriptState = ScriptState::forMainWorld(&pageHolder->frame());
  v8::Isolate* isolate = scriptState->isolate();
  ScriptState::Scope scope(scriptState);
  v8::TryCatch tryCatch(isolate);

  // Deserialize.
  RefPtr<SerializedScriptValue> serializedScriptValue =
      SerializedScriptValue::create(reinterpret_cast<const char*>(data), size);
  serializedScriptValue->deserialize(isolate, messagePorts, blobs);
  CHECK(!tryCatch.HasCaught())
      << "deserialize() should return null rather than throwing an exception.";

  // Request a V8 GC. Oilpan will be invoked by the GC epilogue.
  //
  // Multiple GCs may be required to ensure everything is collected (due to
  // a chain of persistent handles), so some objects may not be collected until
  // a subsequent iteration. This is slow enough as is, so we compromise on one
  // major GC, as opposed to the 5 used in V8GCController for unit tests.
  V8PerIsolateData::mainThreadIsolate()->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);

  return 0;
}

}  // namespace blink

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  return blink::LLVMFuzzerInitialize(argc, argv);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return blink::LLVMFuzzerTestOneInput(data, size);
}
