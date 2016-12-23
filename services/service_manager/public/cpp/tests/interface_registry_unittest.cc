// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/interface_registry.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "services/service_manager/public/cpp/interface_binder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace service_manager {

class TestBinder : public InterfaceBinder {
 public:
  explicit TestBinder(int* delete_count) : delete_count_(delete_count) {}
  ~TestBinder() override { (*delete_count_)++; }
  void BindInterface(const Identity& remote_identity,
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
    auto registry = base::MakeUnique<InterfaceRegistry>(std::string());
    InterfaceRegistry::TestApi test_api(registry.get());
    test_api.SetInterfaceBinderForName(new TestBinder(&delete_count), "TC1");
  }
  EXPECT_EQ(1, delete_count);

  // Removal.
  {
    auto registry =
        base::MakeUnique<InterfaceRegistry>(std::string());
    InterfaceBinder* b = new TestBinder(&delete_count);
    InterfaceRegistry::TestApi test_api(registry.get());
    test_api.SetInterfaceBinderForName(b, "TC1");
    registry->RemoveInterface("TC1");
    registry.reset();
    EXPECT_EQ(2, delete_count);
  }

  // Multiple.
  {
    auto registry = base::MakeUnique<InterfaceRegistry>(std::string());
    InterfaceRegistry::TestApi test_api(registry.get());
    test_api.SetInterfaceBinderForName(new TestBinder(&delete_count), "TC1");
    test_api.SetInterfaceBinderForName(new TestBinder(&delete_count), "TC2");
  }
  EXPECT_EQ(4, delete_count);

  // Re-addition.
  {
    auto registry = base::MakeUnique<InterfaceRegistry>(std::string());
    InterfaceRegistry::TestApi test_api(registry.get());
    test_api.SetInterfaceBinderForName(new TestBinder(&delete_count), "TC1");
    test_api.SetInterfaceBinderForName(new TestBinder(&delete_count), "TC1");
    EXPECT_EQ(5, delete_count);
  }
  EXPECT_EQ(6, delete_count);
}

}  // namespace service_manager
