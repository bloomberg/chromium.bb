// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <linux/limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "v8/include/v8.h"

using namespace v8;

namespace v8 {
namespace platform {
v8::Platform* CreateDefaultPlatform(int thread_pool_size = 0);
} // namespace platform
} // namespace v8


class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
  public:
    virtual void* Allocate(size_t length) {
      void* data = AllocateUninitialized(length);
      return data == NULL ? data : memset(data, 0, length);
    }
    virtual void* AllocateUninitialized(size_t length) {
      return malloc(length);
    }
    virtual void Free(void* data, size_t) { free(data); }
};

static char *ProgramPath() {
  char *path = new char[PATH_MAX + 1];
  assert(path);
  ssize_t sz = readlink("/proc/self/exe", path, PATH_MAX);
  assert(sz > 0);
  path[sz] = 0;
  return path;
}

static Isolate* Init() {
  V8::InitializeICU();
  V8::InitializeExternalStartupData(ProgramPath());
  Platform* platform = platform::CreateDefaultPlatform();
  V8::InitializePlatform(platform);
  V8::Initialize();

  ArrayBufferAllocator* allocator = new ArrayBufferAllocator();
  Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = allocator;
  Isolate* isolate = Isolate::New(create_params);
  assert(isolate);

  return isolate;
}

static Isolate* isolate = Init();

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const unsigned char *data,
    unsigned long size) {
  Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<String> source =
    String::NewFromOneByte(isolate, data,
        NewStringType::kNormal, size).ToLocalChecked();

  JSON::Parse(source);

  return 0;
}

