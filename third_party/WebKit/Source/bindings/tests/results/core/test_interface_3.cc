// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/web_agent_api_interface.cc.tmpl
// by the script code_generator_web_agent_api.py.
// DO NOT MODIFY!

// clang-format off

#include "web/api/test_interface_3.h"

// TODO(dglazkov): Properly sort the includes.
#include "bindings/tests/idls/core/TestInterface3.h"
#include "platform/wtf/text/WTFString.h"

namespace web {

TestInterface3* TestInterface3::Create(blink::TestInterface3* test_interface_3) {
  return test_interface_3 ? new TestInterface3(test_interface_3) : nullptr;
}

DEFINE_TRACE(TestInterface3) {
  visitor->trace(test_interface_3_);
}

// TODO(dglazkov): Implement constant generation

// TODO(dglazkov): Implement attribute getter/setter generation
// unsigned long length
// DOMString readonlyStringifierAttribute

// TODO(dglazkov): Implement method generation
// void TestInterface3::VoidMethodDocument
//   Document document

TestInterface3::TestInterface3(blink::TestInterface3* test_interface_3)
    : test_interface_3_(test_interface_3) {}

blink::TestInterface3* TestInterface3::test_interface_3() const {
  return test_interface_3_;
}

}  // namespace web
