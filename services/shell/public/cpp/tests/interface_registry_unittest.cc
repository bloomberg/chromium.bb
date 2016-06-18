// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/public/cpp/interface_registry.h"

#include "base/message_loop/message_loop.h"
#include "services/shell/public/cpp/interface_binder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace shell {
namespace internal {
namespace {

class TestBinder : public InterfaceBinder {
 public:
  explicit TestBinder(int* delete_count) : delete_count_(delete_count) {}
  ~TestBinder() override { (*delete_count_)++; }
  void BindInterface(Connection* connection,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle client_handle) override {}

 private:
  int* delete_count_;
};

TEST(InterfaceRegistryTest, Ownership) {
  base::MessageLoop message_loop_;
  int delete_count = 0;

  // Destruction.
  {
    InterfaceRegistry registry(nullptr);
    InterfaceRegistry::TestApi test_api(&registry);
    test_api.SetInterfaceBinderForName(new TestBinder(&delete_count), "TC1");
  }
  EXPECT_EQ(1, delete_count);

  // Removal.
  {
    std::unique_ptr<InterfaceRegistry> registry(new InterfaceRegistry(nullptr));
    InterfaceBinder* b = new TestBinder(&delete_count);
    InterfaceRegistry::TestApi test_api(registry.get());
    test_api.SetInterfaceBinderForName(b, "TC1");
    registry->RemoveInterface("TC1");
    registry.reset();
    EXPECT_EQ(2, delete_count);
  }

  // Multiple.
  {
    InterfaceRegistry registry(nullptr);
    InterfaceRegistry::TestApi test_api(&registry);
    test_api.SetInterfaceBinderForName(new TestBinder(&delete_count), "TC1");
    test_api.SetInterfaceBinderForName(new TestBinder(&delete_count), "TC2");
  }
  EXPECT_EQ(4, delete_count);

  // Re-addition.
  {
    InterfaceRegistry registry(nullptr);
    InterfaceRegistry::TestApi test_api(&registry);
    test_api.SetInterfaceBinderForName(new TestBinder(&delete_count), "TC1");
    test_api.SetInterfaceBinderForName(new TestBinder(&delete_count), "TC1");
    EXPECT_EQ(5, delete_count);
  }
  EXPECT_EQ(6, delete_count);
}

}  // namespace
}  // namespace internal
}  // namespace shell
