// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DEVTOOLS_BOUND_OBJECT_H_
#define WEBKIT_GLUE_DEVTOOLS_BOUND_OBJECT_H_

#include <wtf/Noncopyable.h>

#include "v8.h"

// BoundObject is a helper class that lets you map JavaScript method calls
// directly to C++ method calls. It should be destroyed once JS object is
// built.
class BoundObject : public Noncopyable {
 public:
  BoundObject(v8::Handle<v8::Context> context,
              void* v8_this,
              const char* object_name);
  virtual ~BoundObject();

  void AddProtoFunction(const char* name, v8::InvocationCallback callback);
  void Build();

 private:
  v8::HandleScope handle_scope_;
  const char* object_name_;
  v8::Handle<v8::Context> context_;
  v8::Persistent<v8::FunctionTemplate> host_template_;
  void* v8_this_;
};

#endif  // WEBKIT_GLUE_DEVTOOLS_BOUND_OBJECT_H_
