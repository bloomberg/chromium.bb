// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptModule.h"

#include "bindings/core/v8/V8BindingForTesting.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

TEST(ScriptModuleTest, compileSuccess) {
  V8TestingScope scope;
  ScriptModule module =
      ScriptModule::compile(scope.isolate(), "export const a = 42;", "foo.js");
  ASSERT_FALSE(module.isNull());
}

TEST(ScriptModuleTest, compileFail) {
  V8TestingScope scope;
  ScriptModule module =
      ScriptModule::compile(scope.isolate(), "123 = 456", "foo.js");
  ASSERT_TRUE(module.isNull());
}

}  // namespace

}  // namespace blink
