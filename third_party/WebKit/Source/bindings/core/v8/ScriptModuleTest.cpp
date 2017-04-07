// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptModule.h"

#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8PerContextData.h"
#include "core/dom/ScriptModuleResolver.h"
#include "core/testing/DummyModulator.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
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
  ScriptModule module = ScriptModule::compile(
      scope.isolate(), "export const a = 42;", "foo.js", SharableCrossOrigin);
  ASSERT_FALSE(module.isNull());
}

TEST(ScriptModuleTest, compileFail) {
  V8TestingScope scope;
  ScriptModule module = ScriptModule::compile(scope.isolate(), "123 = 456",
                                              "foo.js", SharableCrossOrigin);
  ASSERT_TRUE(module.isNull());
}

TEST(ScriptModuleTest, equalAndHash) {
  V8TestingScope scope;

  ScriptModule moduleNull;
  ScriptModule moduleA = ScriptModule::compile(
      scope.isolate(), "export const a = 'a';", "a.js", SharableCrossOrigin);
  ASSERT_FALSE(moduleA.isNull());
  ScriptModule moduleB = ScriptModule::compile(
      scope.isolate(), "export const b = 'b';", "b.js", SharableCrossOrigin);
  ASSERT_FALSE(moduleB.isNull());
  Vector<char> moduleDeletedBuffer(sizeof(ScriptModule));
  ScriptModule& moduleDeleted =
      *reinterpret_cast<ScriptModule*>(moduleDeletedBuffer.data());
  HashTraits<ScriptModule>::constructDeletedValue(moduleDeleted, true);

  EXPECT_EQ(moduleNull, moduleNull);
  EXPECT_EQ(moduleA, moduleA);
  EXPECT_EQ(moduleB, moduleB);
  EXPECT_EQ(moduleDeleted, moduleDeleted);

  EXPECT_NE(moduleNull, moduleA);
  EXPECT_NE(moduleNull, moduleB);
  EXPECT_NE(moduleNull, moduleDeleted);

  EXPECT_NE(moduleA, moduleNull);
  EXPECT_NE(moduleA, moduleB);
  EXPECT_NE(moduleA, moduleDeleted);

  EXPECT_NE(moduleB, moduleNull);
  EXPECT_NE(moduleB, moduleA);
  EXPECT_NE(moduleB, moduleDeleted);

  EXPECT_NE(moduleDeleted, moduleNull);
  EXPECT_NE(moduleDeleted, moduleA);
  EXPECT_NE(moduleDeleted, moduleB);

  EXPECT_NE(DefaultHash<ScriptModule>::Hash::hash(moduleA),
            DefaultHash<ScriptModule>::Hash::hash(moduleB));
  EXPECT_NE(DefaultHash<ScriptModule>::Hash::hash(moduleNull),
            DefaultHash<ScriptModule>::Hash::hash(moduleA));
  EXPECT_NE(DefaultHash<ScriptModule>::Hash::hash(moduleNull),
            DefaultHash<ScriptModule>::Hash::hash(moduleB));
}

TEST(ScriptModuleTest, moduleRequests) {
  V8TestingScope scope;
  ScriptModule module = ScriptModule::compile(
      scope.isolate(), "import 'a'; import 'b'; export const c = 'c';",
      "foo.js", SharableCrossOrigin);
  ASSERT_FALSE(module.isNull());

  auto requests = module.moduleRequests(scope.getScriptState());
  EXPECT_THAT(requests, ::testing::ContainerEq<Vector<String>>({"a", "b"}));
}

TEST(ScriptModuleTest, instantiateNoDeps) {
  V8TestingScope scope;

  auto modulator = new ScriptModuleTestModulator();
  auto resolver = modulator->testScriptModuleResolver();

  Modulator::setModulator(scope.getScriptState(), modulator);

  ScriptModule module = ScriptModule::compile(
      scope.isolate(), "export const a = 42;", "foo.js", SharableCrossOrigin);
  ASSERT_FALSE(module.isNull());
  ScriptValue exception = module.instantiate(scope.getScriptState());
  ASSERT_TRUE(exception.isEmpty());

  EXPECT_EQ(0u, resolver->resolveCount());
}

TEST(ScriptModuleTest, instantiateWithDeps) {
  V8TestingScope scope;

  auto modulator = new ScriptModuleTestModulator();
  auto resolver = modulator->testScriptModuleResolver();

  Modulator::setModulator(scope.getScriptState(), modulator);

  ScriptModule moduleA = ScriptModule::compile(
      scope.isolate(), "export const a = 'a';", "foo.js", SharableCrossOrigin);
  ASSERT_FALSE(moduleA.isNull());
  resolver->pushScriptModule(moduleA);

  ScriptModule moduleB = ScriptModule::compile(
      scope.isolate(), "export const b = 'b';", "foo.js", SharableCrossOrigin);
  ASSERT_FALSE(moduleB.isNull());
  resolver->pushScriptModule(moduleB);

  ScriptModule module = ScriptModule::compile(
      scope.isolate(), "import 'a'; import 'b'; export const c = 123;", "c.js",
      SharableCrossOrigin);
  ASSERT_FALSE(module.isNull());
  ScriptValue exception = module.instantiate(scope.getScriptState());
  ASSERT_TRUE(exception.isEmpty());

  ASSERT_EQ(2u, resolver->resolveCount());
  EXPECT_EQ("a", resolver->specifiers()[0]);
  EXPECT_EQ("b", resolver->specifiers()[1]);
}

}  // namespace

}  // namespace blink
