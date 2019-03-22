// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_TEST_SERVICE_MANAGER_CONTEXT_H_
#define IOS_WEB_PUBLIC_TEST_TEST_SERVICE_MANAGER_CONTEXT_H_

#include <memory>

#include "base/macros.h"

namespace web {

class ServiceManagerContext;

// Provides a public helper for client unit tests to construct a
// ServiceManagerContext, which is generally private to the ios/web
// implementation. Requires an IO thread to use.
class TestServiceManagerContext {
 public:
  TestServiceManagerContext();
  ~TestServiceManagerContext();

 private:
  std::unique_ptr<ServiceManagerContext> context_;

  DISALLOW_COPY_AND_ASSIGN(TestServiceManagerContext);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_TEST_SERVICE_MANAGER_CONTEXT_H_