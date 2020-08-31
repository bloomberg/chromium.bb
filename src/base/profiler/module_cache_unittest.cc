// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/profiler/module_cache.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

int AFunctionForTest() {
  return 42;
}

// Provides a module that is guaranteed to be isolated from (and non-contiguous
// with) any other module, by placing the module in the middle of a block of
// heap memory.
class IsolatedModule : public ModuleCache::Module {
 public:
  explicit IsolatedModule(bool is_native = true)
      : is_native_(is_native), memory_region_(new char[kRegionSize]) {}

  // ModuleCache::Module
  uintptr_t GetBaseAddress() const override {
    // Place the module in the middle of the region.
    return reinterpret_cast<uintptr_t>(&memory_region_[kRegionSize / 4]);
  }

  std::string GetId() const override { return ""; }
  FilePath GetDebugBasename() const override { return FilePath(); }
  size_t GetSize() const override { return kRegionSize / 2; }
  bool IsNative() const override { return is_native_; }

 private:
  static const int kRegionSize = 100;

  bool is_native_;
  std::unique_ptr<char[]> memory_region_;
};

// Provides a fake module with configurable base address and size.
class FakeModule : public ModuleCache::Module {
 public:
  FakeModule(uintptr_t base_address,
             size_t size,
             bool is_native = true,
             OnceClosure destruction_closure = OnceClosure())
      : base_address_(base_address),
        size_(size),
        is_native_(is_native),
        destruction_closure_runner_(std::move(destruction_closure)) {}

  FakeModule(const FakeModule&) = delete;
  FakeModule& operator=(const FakeModule&) = delete;

  uintptr_t GetBaseAddress() const override { return base_address_; }
  std::string GetId() const override { return ""; }
  FilePath GetDebugBasename() const override { return FilePath(); }
  size_t GetSize() const override { return size_; }
  bool IsNative() const override { return is_native_; }

 private:
  uintptr_t base_address_;
  size_t size_;
  bool is_native_;
  ScopedClosureRunner destruction_closure_runner_;
};

// Utility function to add a single non-native module during test setup. Returns
// a pointer to the provided module.
const ModuleCache::Module* AddNonNativeModule(
    ModuleCache* cache,
    std::unique_ptr<const ModuleCache::Module> module) {
  const ModuleCache::Module* module_ptr = module.get();
  std::vector<std::unique_ptr<const ModuleCache::Module>> modules;
  modules.push_back(std::move(module));
  cache->UpdateNonNativeModules({}, std::move(modules));
  return module_ptr;
}

#if (defined(OS_POSIX) && !defined(OS_IOS) && !defined(ARCH_CPU_ARM64)) || \
    (defined(OS_FUCHSIA) && !defined(ARCH_CPU_ARM64)) || defined(OS_WIN)
#define MAYBE_TEST(TestSuite, TestName) TEST(TestSuite, TestName)
#else
#define MAYBE_TEST(TestSuite, TestName) TEST(TestSuite, DISABLED_##TestName)
#endif

// Checks that ModuleCache returns the same module instance for
// addresses within the module.
MAYBE_TEST(ModuleCacheTest, LookupCodeAddresses) {
  uintptr_t ptr1 = reinterpret_cast<uintptr_t>(&AFunctionForTest);
  uintptr_t ptr2 = ptr1 + 1;
  ModuleCache cache;
  const ModuleCache::Module* module1 = cache.GetModuleForAddress(ptr1);
  const ModuleCache::Module* module2 = cache.GetModuleForAddress(ptr2);
  EXPECT_EQ(module1, module2);
  EXPECT_NE(nullptr, module1);
  EXPECT_GT(module1->GetSize(), 0u);
  EXPECT_LE(module1->GetBaseAddress(), ptr1);
  EXPECT_GT(module1->GetBaseAddress() + module1->GetSize(), ptr2);
}

MAYBE_TEST(ModuleCacheTest, LookupRange) {
  ModuleCache cache;
  auto to_inject = std::make_unique<IsolatedModule>();
  const ModuleCache::Module* module = to_inject.get();
  cache.AddCustomNativeModule(std::move(to_inject));

  EXPECT_EQ(nullptr, cache.GetModuleForAddress(module->GetBaseAddress() - 1));
  EXPECT_EQ(module, cache.GetModuleForAddress(module->GetBaseAddress()));
  EXPECT_EQ(module, cache.GetModuleForAddress(module->GetBaseAddress() +
                                              module->GetSize() - 1));
  EXPECT_EQ(nullptr, cache.GetModuleForAddress(module->GetBaseAddress() +
                                               module->GetSize()));
}

MAYBE_TEST(ModuleCacheTest, LookupNonNativeModule) {
  ModuleCache cache;
  const ModuleCache::Module* module =
      AddNonNativeModule(&cache, std::make_unique<IsolatedModule>(false));

  EXPECT_EQ(nullptr, cache.GetModuleForAddress(module->GetBaseAddress() - 1));
  EXPECT_EQ(module, cache.GetModuleForAddress(module->GetBaseAddress()));
  EXPECT_EQ(module, cache.GetModuleForAddress(module->GetBaseAddress() +
                                              module->GetSize() - 1));
  EXPECT_EQ(nullptr, cache.GetModuleForAddress(module->GetBaseAddress() +
                                               module->GetSize()));
}

MAYBE_TEST(ModuleCacheTest, LookupOverlaidNonNativeModule) {
  ModuleCache cache;

  auto native_module_to_inject = std::make_unique<IsolatedModule>();
  const ModuleCache::Module* native_module = native_module_to_inject.get();
  cache.AddCustomNativeModule(std::move(native_module_to_inject));

  // Overlay the native module with the non-native module, starting 8 bytes into
  // the native modules and ending 8 bytes before the end of the module.
  const ModuleCache::Module* non_native_module = AddNonNativeModule(
      &cache,
      std::make_unique<FakeModule>(native_module->GetBaseAddress() + 8,
                                   native_module->GetSize() - 16, false));

  EXPECT_EQ(native_module,
            cache.GetModuleForAddress(non_native_module->GetBaseAddress() - 1));
  EXPECT_EQ(non_native_module,
            cache.GetModuleForAddress(non_native_module->GetBaseAddress()));
  EXPECT_EQ(non_native_module,
            cache.GetModuleForAddress(non_native_module->GetBaseAddress() +
                                      non_native_module->GetSize() - 1));
  EXPECT_EQ(native_module,
            cache.GetModuleForAddress(non_native_module->GetBaseAddress() +
                                      non_native_module->GetSize()));
}

MAYBE_TEST(ModuleCacheTest, UpdateNonNativeModulesAdd) {
  ModuleCache cache;
  std::vector<std::unique_ptr<const ModuleCache::Module>> modules;
  modules.push_back(std::make_unique<FakeModule>(1, 1, false));
  const ModuleCache::Module* module = modules.back().get();
  cache.UpdateNonNativeModules({}, std::move(modules));

  EXPECT_EQ(module, cache.GetModuleForAddress(1));
}

MAYBE_TEST(ModuleCacheTest, UpdateNonNativeModulesRemove) {
  ModuleCache cache;
  std::vector<std::unique_ptr<const ModuleCache::Module>> modules;
  modules.push_back(std::make_unique<FakeModule>(1, 1, false));
  const ModuleCache::Module* module = modules.back().get();
  cache.UpdateNonNativeModules({}, std::move(modules));
  cache.UpdateNonNativeModules({module}, {});

  EXPECT_EQ(nullptr, cache.GetModuleForAddress(1));
}

MAYBE_TEST(ModuleCacheTest, UpdateNonNativeModulesRemoveModuleIsNotDestroyed) {
  bool was_destroyed = false;
  {
    ModuleCache cache;
    std::vector<std::unique_ptr<const ModuleCache::Module>> modules;
    modules.push_back(std::make_unique<FakeModule>(
        1, 1, false,
        BindLambdaForTesting([&was_destroyed]() { was_destroyed = true; })));
    const ModuleCache::Module* module = modules.back().get();
    cache.UpdateNonNativeModules({}, std::move(modules));
    cache.UpdateNonNativeModules({module}, {});

    EXPECT_FALSE(was_destroyed);
  }
  EXPECT_TRUE(was_destroyed);
}

MAYBE_TEST(ModuleCacheTest, UpdateNonNativeModulesReplace) {
  ModuleCache cache;
  // Replace a module with another larger module at the same base address.
  std::vector<std::unique_ptr<const ModuleCache::Module>> modules1;
  modules1.push_back(std::make_unique<FakeModule>(1, 1, false));
  const ModuleCache::Module* module1 = modules1.back().get();
  std::vector<std::unique_ptr<const ModuleCache::Module>> modules2;
  modules2.push_back(std::make_unique<FakeModule>(1, 2, false));
  const ModuleCache::Module* module2 = modules2.back().get();

  cache.UpdateNonNativeModules({}, std::move(modules1));
  cache.UpdateNonNativeModules({module1}, std::move(modules2));

  EXPECT_EQ(module2, cache.GetModuleForAddress(2));
}

MAYBE_TEST(ModuleCacheTest,
           UpdateNonNativeModulesMultipleRemovedModulesAtSameAddress) {
  int destroyed_count = 0;
  const auto record_destroyed = [&destroyed_count]() { ++destroyed_count; };
  ModuleCache cache;

  // Checks that non-native modules can be repeatedly added and removed at the
  // same addresses, and that all are retained in the cache.
  std::vector<std::unique_ptr<const ModuleCache::Module>> modules1;
  modules1.push_back(std::make_unique<FakeModule>(
      1, 1, false, BindLambdaForTesting(record_destroyed)));
  const ModuleCache::Module* module1 = modules1.back().get();

  std::vector<std::unique_ptr<const ModuleCache::Module>> modules2;
  modules2.push_back(std::make_unique<FakeModule>(
      1, 1, false, BindLambdaForTesting(record_destroyed)));
  const ModuleCache::Module* module2 = modules2.back().get();

  cache.UpdateNonNativeModules({}, std::move(modules1));
  cache.UpdateNonNativeModules({module1}, std::move(modules2));
  cache.UpdateNonNativeModules({module2}, {});

  EXPECT_EQ(0, destroyed_count);
}

MAYBE_TEST(ModuleCacheTest, UpdateNonNativeModulesCorrectModulesRemoved) {
  ModuleCache cache;

  std::vector<std::unique_ptr<const ModuleCache::Module>> to_add;
  for (int i = 0; i < 5; ++i) {
    to_add.push_back(std::make_unique<FakeModule>(i + 1, 1, false));
  }

  std::vector<const ModuleCache::Module*> to_remove = {to_add[1].get(),
                                                       to_add[3].get()};

  // Checks that the correct modules are removed when removing some but not all
  // modules.
  cache.UpdateNonNativeModules({}, std::move(to_add));
  cache.UpdateNonNativeModules({to_remove}, {});

  DCHECK_NE(nullptr, cache.GetModuleForAddress(1));
  DCHECK_EQ(nullptr, cache.GetModuleForAddress(2));
  DCHECK_NE(nullptr, cache.GetModuleForAddress(3));
  DCHECK_EQ(nullptr, cache.GetModuleForAddress(4));
  DCHECK_NE(nullptr, cache.GetModuleForAddress(5));
}

MAYBE_TEST(ModuleCacheTest, ModulesList) {
  ModuleCache cache;
  uintptr_t ptr = reinterpret_cast<uintptr_t>(&AFunctionForTest);
  const ModuleCache::Module* native_module = cache.GetModuleForAddress(ptr);
  const ModuleCache::Module* non_native_module =
      AddNonNativeModule(&cache, std::make_unique<FakeModule>(1, 2, false));

  EXPECT_NE(nullptr, native_module);
  std::vector<const ModuleCache::Module*> modules = cache.GetModules();
  ASSERT_EQ(2u, modules.size());
  EXPECT_EQ(native_module, modules[0]);
  EXPECT_EQ(non_native_module, modules[1]);
}

MAYBE_TEST(ModuleCacheTest, InvalidModule) {
  ModuleCache cache;
  EXPECT_EQ(nullptr, cache.GetModuleForAddress(1));
}

}  // namespace
}  // namespace base
