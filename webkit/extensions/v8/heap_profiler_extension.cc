// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/extensions/v8/heap_profiler_extension.h"

#include "base/basictypes.h"

#if defined(LINUX_USE_TCMALLOC)
#include "third_party/tcmalloc/tcmalloc/src/google/heap-profiler.h"
#endif

namespace extensions_v8 {

namespace {

const char kHeapProfilerExtensionName[] = "v8/HeapProfiler";

class HeapProfilerWrapper : public v8::Extension {
 public:
  HeapProfilerWrapper()
      : v8::Extension(kHeapProfilerExtensionName,
                      "if (typeof(chromium) == 'undefined') {"
                      "  chromium = {};"
                      "}"
                      "(function() {"
                      "  native function HeapProfilerStart();"
                      "  native function HeapProfilerStop();"
                      "  native function HeapProfilerDump();"
                      "  chromium.HeapProfiler = {};"
                      "  chromium.HeapProfiler.start = function(opt_prefix) {"
                      "    var prefix = opt_prefix;"
                      "    if (!prefix) {"
                      "      var d = new Date();"
                      "      prefix = \"chromium-\" + "
                      "               (1900 + d.getYear()) + "
                      "               \"-\" + d.getMonth() + "
                      "               \"-\" + d.getDay() + "
                      "               \"-\" + d.getTime();"
                      "    }"
                      "    HeapProfilerStart(prefix);"
                      "  };"
                      "  chromium.HeapProfiler.stop = function() {"
                      "    HeapProfilerStop();"
                      "  };"
                      "  chromium.HeapProfiler.dump = function(opt_reason) {"
                      "    HeapProfilerDump(opt_reason || \"\");"
                      "  };"
                      "})();") {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("HeapProfilerStart"))) {
      return v8::FunctionTemplate::New(HeapProfilerStart);
    } else if (name->Equals(v8::String::New("HeapProfilerStop"))) {
      return v8::FunctionTemplate::New(HeapProfilerStop);
    } else if (name->Equals(v8::String::New("HeapProfilerDump"))) {
      return v8::FunctionTemplate::New(HeapProfilerDump);
    }
    return v8::Handle<v8::FunctionTemplate>();
  }

#if defined(LINUX_USE_TCMALLOC)
  static v8::Handle<v8::Value> HeapProfilerStart(const v8::Arguments& args) {
    if (args.Length() >= 1 && args[0]->IsString()) {
      v8::Local<v8::String> js_prefix = args[0]->ToString();
      char prefix[256];
      js_prefix->WriteAscii(prefix, 0, arraysize(prefix) - 1);
      ::HeapProfilerStart(prefix);
    }
    return v8::Undefined();
  }

  static v8::Handle<v8::Value> HeapProfilerStop(const v8::Arguments& args) {
    ::HeapProfilerStop();
    return v8::Undefined();
  }

  static v8::Handle<v8::Value> HeapProfilerDump(const v8::Arguments& args) {
    if (args.Length() >= 1 && args[0]->IsString()) {
      v8::Local<v8::String> js_reason = args[0]->ToString();
      char reason[256];
      js_reason->WriteAscii(reason, 0, arraysize(reason) - 1);
      ::HeapProfilerDump(reason);
    }
    return v8::Undefined();
  }

#else

  static v8::Handle<v8::Value> HeapProfilerStart(const v8::Arguments& args) {
    return v8::Undefined();
  }

  static v8::Handle<v8::Value> HeapProfilerStop(const v8::Arguments& args) {
    return v8::Undefined();
  }

  static v8::Handle<v8::Value> HeapProfilerDump(const v8::Arguments& args) {
    return v8::Undefined();
  }
#endif
};

}  // namespace

v8::Extension* HeapProfilerExtension::Get() {
  return new HeapProfilerWrapper;
}

}  // namespace extensions_v8
