// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptController.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

TEST(ScriptControllerTest, CompileAndExecuteScriptInMainWorld) {
  V8TestingScope scope;
  ScriptController& script = scope.frame().script();
  CompiledScript* compiled = script.compileScriptInMainWorld(
      ScriptSourceCode("x = 42;"), NotSharableCrossOrigin);
  script.executeScriptInMainWorld(*compiled);
  v8::Local<v8::Value> value =
      script.executeScriptInMainWorldAndReturnValue(ScriptSourceCode("x"));
  EXPECT_EQ(42, value->Int32Value());
}

TEST(ScriptControllerTest, CompileAndExecuteScriptInMainWorldInterleaved) {
  V8TestingScope scope;
  ScriptController& script = scope.frame().script();
  CompiledScript* compiled = script.compileScriptInMainWorld(
      ScriptSourceCode("x *= 2;"), NotSharableCrossOrigin);
  CompiledScript* compiled2 = script.compileScriptInMainWorld(
      ScriptSourceCode("x = 21;"), NotSharableCrossOrigin);
  script.executeScriptInMainWorld(*compiled2);
  script.executeScriptInMainWorld(*compiled);
  v8::Local<v8::Value> value =
      script.executeScriptInMainWorldAndReturnValue(ScriptSourceCode("x"));
  EXPECT_EQ(42, value->Int32Value());
}

}  // namespace
}  // namespace blink
