// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/utility/run_loop.h"
#include "mojo/public/interfaces/bindings/tests/sample_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

class ServiceImpl : public sample::Service {
 public:
  ServiceImpl() {}
  ~ServiceImpl() override {}

 private:
  // sample::Service implementation
  void Frobinate(sample::FooPtr foo,
                 BazOptions options,
                 sample::PortPtr port,
                 const FrobinateCallback& callback) override {
    callback.Run(1);
  }
  void GetPort(InterfaceRequest<sample::Port> port) override {}
};

class RecordingErrorHandler : public ErrorHandler {
 public:
  RecordingErrorHandler() : error_(false) {}
  ~RecordingErrorHandler() override {}

  bool encountered_error() const { return error_; }

 private:
  // ErrorHandler implementation.
  void OnConnectionError() override { error_ = true; }

  bool error_;
};

class BindingTest : public testing::Test {
 public:
  BindingTest() {}
  ~BindingTest() override {}

 protected:
  RecordingErrorHandler handler_;
  ServiceImpl impl_;
  Environment env_;
  RunLoop loop_;
};

// Tests that destroying a mojo::Binding closes the bound message pipe handle.
TEST_F(BindingTest, DestroyClosesMessagePipe) {
  sample::ServicePtr ptr;
  auto request = GetProxy(&ptr);
  ptr.set_error_handler(&handler_);
  bool called = false;
  auto called_cb = [&called](int32_t result) { called = true; };
  {
    Binding<sample::Service> binding(&impl_, request.Pass());
    ptr->Frobinate(nullptr, sample::Service::BAZ_OPTIONS_REGULAR, nullptr,
                   called_cb);
    loop_.RunUntilIdle();
    EXPECT_TRUE(called);
    EXPECT_FALSE(handler_.encountered_error());
  }
  // Now that the Binding is out of scope we should detect an error on the other
  // end of the pipe.
  loop_.RunUntilIdle();
  EXPECT_TRUE(handler_.encountered_error());

  // And calls should fail.
  called = false;
  ptr->Frobinate(nullptr, sample::Service::BAZ_OPTIONS_REGULAR, nullptr,
                 called_cb);
  loop_.RunUntilIdle();
  EXPECT_FALSE(called);
}

// Tests that explicitly calling Unbind followed by rebinding works.
TEST_F(BindingTest, Unbind) {
  sample::ServicePtr ptr;
  Binding<sample::Service> binding(&impl_, GetProxy(&ptr));

  bool called = false;
  auto called_cb = [&called](int32_t result) { called = true; };
  ptr->Frobinate(nullptr, sample::Service::BAZ_OPTIONS_REGULAR, nullptr,
                 called_cb);
  loop_.RunUntilIdle();
  EXPECT_TRUE(called);

  called = false;
  auto request = binding.Unbind();
  EXPECT_FALSE(binding.is_bound());
  // All calls should fail when not bound...
  ptr->Frobinate(nullptr, sample::Service::BAZ_OPTIONS_REGULAR, nullptr,
                 called_cb);
  loop_.RunUntilIdle();
  EXPECT_FALSE(called);

  called = false;
  binding.Bind(request.Pass());
  EXPECT_TRUE(binding.is_bound());
  // ...and should succeed again when the rebound.
  ptr->Frobinate(nullptr, sample::Service::BAZ_OPTIONS_REGULAR, nullptr,
                 called_cb);
  loop_.RunUntilIdle();
  EXPECT_TRUE(called);
}

}  // namespace
}  // mojo
