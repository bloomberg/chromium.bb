// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/dictionary_impl.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef TestInterfaceEventInit_h
#define TestInterfaceEventInit_h

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_init.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CORE_EXPORT TestInterfaceEventInit : public EventInit {
  DISALLOW_NEW();
 public:
  TestInterfaceEventInit();
  virtual ~TestInterfaceEventInit();
  TestInterfaceEventInit(const TestInterfaceEventInit&);
  TestInterfaceEventInit& operator=(const TestInterfaceEventInit&);

  bool hasStringMember() const { return !string_member_.IsNull(); }
  const String& stringMember() const {
    return string_member_;
  }
  inline void setStringMember(const String&);

  v8::Local<v8::Value> ToV8Impl(v8::Local<v8::Object>, v8::Isolate*) const override;
  void Trace(blink::Visitor*) override;

 private:

  String string_member_;

  friend class V8TestInterfaceEventInit;
};

void TestInterfaceEventInit::setStringMember(const String& value) {
  string_member_ = value;
}

}  // namespace blink

#endif  // TestInterfaceEventInit_h
