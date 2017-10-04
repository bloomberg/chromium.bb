// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/dictionary_impl.cpp.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "TestInterfaceEventInit.h"

namespace blink {

TestInterfaceEventInit::TestInterfaceEventInit() {
}

TestInterfaceEventInit::~TestInterfaceEventInit() {}

TestInterfaceEventInit::TestInterfaceEventInit(const TestInterfaceEventInit&) = default;

TestInterfaceEventInit& TestInterfaceEventInit::operator=(const TestInterfaceEventInit&) = default;

DEFINE_TRACE(TestInterfaceEventInit) {
  EventInit::Trace(visitor);
}

}  // namespace blink
