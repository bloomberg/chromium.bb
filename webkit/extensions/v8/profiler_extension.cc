// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/extensions/v8/profiler_extension.h"

#include "build/build_config.h"

#if defined(QUANTIFY)
// this #define is used to prevent people from directly using pure.h
// instead of profiler.h
#define PURIFY_PRIVATE_INCLUDE
#include "base/third_party/purify/pure.h"
#endif // QUANTIFY

#if defined(USE_TCMALLOC) && defined(OS_POSIX) && !defined(OS_MACOSX)
#include "third_party/tcmalloc/chromium/src/google/profiler.h"
#endif

namespace extensions_v8 {

const char kProfilerExtensionName[] = "v8/Profiler";

class ProfilerWrapper : public v8::Extension {
 public:
  ProfilerWrapper() :
      v8::Extension(kProfilerExtensionName,
        "if (typeof(chromium) == 'undefined') {"
        "  chromium = {};"
        "}"
        "chromium.Profiler = function() {"
        "  native function ProfilerStart();"
        "  native function ProfilerStop();"
        "  native function ProfilerFlush();"
        "  native function ProfilerClearData();"
        "  native function ProfilerSetThreadName();"
        "  this.start = function() {"
        "    ProfilerStart();"
        "  };"
        "  this.stop = function() {"
        "    ProfilerStop();"
        "  };"
        "  this.clear = function() {"
        "    ProfilerClearData();"
        "  };"
        "  this.flush = function() {"
        "    ProfilerFlush();"
        "  };"
        "  this.setThreadName = function(name) {"
        "    ProfilerSetThreadName(name);"
        "  };"
        "};") {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("ProfilerStart"))) {
      return v8::FunctionTemplate::New(ProfilerStart);
    } else if (name->Equals(v8::String::New("ProfilerStop"))) {
      return v8::FunctionTemplate::New(ProfilerStop);
    } else if (name->Equals(v8::String::New("ProfilerClearData"))) {
      return v8::FunctionTemplate::New(ProfilerClearData);
    } else if (name->Equals(v8::String::New("ProfilerFlush"))) {
      return v8::FunctionTemplate::New(ProfilerFlush);
    } else if (name->Equals(v8::String::New("ProfilerSetThreadName"))) {
      return v8::FunctionTemplate::New(ProfilerSetThreadName);
    }
    return v8::Handle<v8::FunctionTemplate>();
  }

  static v8::Handle<v8::Value> ProfilerStart(
      const v8::Arguments& args) {
#if defined(QUANTIFY)
    QuantifyStartRecordingData();
#elif defined(USE_TCMALLOC) && defined(OS_POSIX) && !defined(OS_MACOSX)
    ::ProfilerStart("chrome-profile");
#endif
    return v8::Undefined();
  }

  static v8::Handle<v8::Value> ProfilerStop(
      const v8::Arguments& args) {
#if defined(QUANTIFY)
    QuantifyStopRecordingData();
#elif defined(USE_TCMALLOC) && defined(OS_POSIX) && !defined(OS_MACOSX)
    ::ProfilerStop();
#endif
    return v8::Undefined();
  }

  static v8::Handle<v8::Value> ProfilerClearData(
      const v8::Arguments& args) {
#if defined(QUANTIFY)
    QuantifyClearData();
#endif
    return v8::Undefined();
  }

  static v8::Handle<v8::Value> ProfilerFlush(
      const v8::Arguments& args) {
#if defined(USE_TCMALLOC) && defined(OS_POSIX) && !defined(OS_MACOSX)
    ::ProfilerFlush();
#endif
    return v8::Undefined();
  }


  static v8::Handle<v8::Value> ProfilerSetThreadName(
      const v8::Arguments& args) {
    if (args.Length() >= 1 && args[0]->IsString()) {
      v8::Local<v8::String> inputString = args[0]->ToString();
      char nameBuffer[256];
      inputString->WriteAscii(nameBuffer, 0, sizeof(nameBuffer)-1);
#if defined(QUANTIFY)
      // make a copy since the Quantify function takes a char*, not const char*
      char buffer[512];
      base::snprintf(buffer, arraysize(buffer)-1, "%s", name);
      QuantifySetThreadName(buffer);
#endif
    }
    return v8::Undefined();
  }
};

v8::Extension* ProfilerExtension::Get() {
  return new ProfilerWrapper();
}

}  // namespace extensions_v8
