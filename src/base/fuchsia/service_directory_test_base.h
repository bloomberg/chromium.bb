// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_SERVICE_DIRECTORY_TEST_BASE_H_
#define BASE_FUCHSIA_SERVICE_DIRECTORY_TEST_BASE_H_

#include <lib/sys/cpp/outgoing_directory.h>
#include <lib/sys/cpp/service_directory.h>
#include <zircon/types.h>
#include <memory>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_interface_impl.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/testfidl/cpp/fidl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class ServiceDirectoryTestBase : public testing::Test {
 public:
  ServiceDirectoryTestBase();

  ServiceDirectoryTestBase(const ServiceDirectoryTestBase&) = delete;
  ServiceDirectoryTestBase& operator=(const ServiceDirectoryTestBase&) = delete;

  ~ServiceDirectoryTestBase() override;

  void VerifyTestInterface(fidl::InterfacePtr<testfidl::TestInterface>* stub,
                           zx_status_t expected_error);

 protected:
  const test::ScopedRunLoopTimeout run_timeout_;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  std::unique_ptr<sys::OutgoingDirectory> outgoing_directory_;
  TestInterfaceImpl test_service_;
  std::unique_ptr<ScopedServiceBinding<testfidl::TestInterface>>
      service_binding_;

  std::unique_ptr<sys::ServiceDirectory> public_service_directory_;
  std::unique_ptr<sys::ServiceDirectory> debug_service_directory_;
  std::unique_ptr<sys::ServiceDirectory> root_service_directory_;
};

}  // namespace base

#endif  // BASE_FUCHSIA_SERVICE_DIRECTORY_TEST_BASE_H_
