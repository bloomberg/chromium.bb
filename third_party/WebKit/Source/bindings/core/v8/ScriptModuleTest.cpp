// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptModule.h"

#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8PerContextData.h"
#include "core/dom/ScriptModuleResolver.h"
#include "core/testing/DummyModulator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class TestScriptModuleResolver final : public ScriptModuleResolver {
 public:
  TestScriptModuleResolver() {}
  virtual ~TestScriptModuleResolver() {}

  size_t resolveCount() const { return m_specifiers.size(); }
  const Vector<String>& specifiers() const { return m_specifiers; }
  void pushScriptModule(ScriptModule scriptModule) {
    m_scriptModules.push_back(scriptModule);
  }

 private:
  // Implements ScriptModuleResolver:

  void registerModuleScript(ModuleScript*) override { NOTREACHED(); }

  ScriptModule resolve(const String& specifier,
                       const ScriptModule&,
                       ExceptionState&) override {
    m_specifiers.push_back(specifier);
    return m_scriptModules.takeFirst();
  }

  Vector<String> m_specifiers;
  Deque<ScriptModule> m_scriptModules;
};

class ScriptModuleTestModulator final : public DummyModulator {
 public:
  ScriptModuleTestModulator();
  virtual ~ScriptModuleTestModulator() {}

  DECLARE_TRACE();

  TestScriptModuleResolver* testScriptModuleResolver() {
    return m_resolver.get();
  }

 private:
  // Implements Modulator:

  ScriptModuleResolver* scriptModuleResolver() override {
    return m_resolver.get();
  }

  Member<TestScriptModuleResolver> m_resolver;
};

ScriptModuleTestModulator::ScriptModuleTestModulator()
    : m_resolver(new TestScriptModuleResolver) {}

DEFINE_TRACE(ScriptModuleTestModulator) {
  visitor->trace(m_resolver);
  DummyModulator::trace(visitor);
}

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

TEST(ScriptModuleTest, instantiateNoDeps) {
  V8TestingScope scope;

  auto modulator = new ScriptModuleTestModulator();
  auto resolver = modulator->testScriptModuleResolver();

  auto contextData = V8PerContextData::from(scope.context());
  contextData->setModulator(modulator);

  ScriptModule module =
      ScriptModule::compile(scope.isolate(), "export const a = 42;", "foo.js");
  ASSERT_FALSE(module.isNull());
  ScriptValue exception = module.instantiate(scope.getScriptState());
  ASSERT_TRUE(exception.isEmpty());

  EXPECT_EQ(0u, resolver->resolveCount());
}

TEST(ScriptModuleTest, instantiateWithDeps) {
  V8TestingScope scope;

  auto modulator = new ScriptModuleTestModulator();
  auto resolver = modulator->testScriptModuleResolver();

  auto contextData = V8PerContextData::from(scope.context());
  contextData->setModulator(modulator);

  ScriptModule moduleA =
      ScriptModule::compile(scope.isolate(), "export const a = 'a';", "foo.js");
  ASSERT_FALSE(moduleA.isNull());
  resolver->pushScriptModule(moduleA);

  ScriptModule moduleB =
      ScriptModule::compile(scope.isolate(), "export const b = 'b';", "foo.js");
  ASSERT_FALSE(moduleB.isNull());
  resolver->pushScriptModule(moduleB);

  ScriptModule module = ScriptModule::compile(
      scope.isolate(), "import 'a'; import 'b'; export const c = 123;", "c.js");
  ASSERT_FALSE(module.isNull());
  ScriptValue exception = module.instantiate(scope.getScriptState());
  ASSERT_TRUE(exception.isEmpty());

  ASSERT_EQ(2u, resolver->resolveCount());
  EXPECT_EQ("a", resolver->specifiers()[0]);
  EXPECT_EQ("b", resolver->specifiers()[1]);
}

}  // namespace

}  // namespace blink
