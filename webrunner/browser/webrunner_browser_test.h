// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBRUNNER_BROWSER_WEBRUNNER_BROWSER_TEST_H_
#define WEBRUNNER_BROWSER_WEBRUNNER_BROWSER_TEST_H_

#include <memory>

#include "base/macros.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "webrunner/browser/context_impl.h"
#include "webrunner/fidl/chromium/web/cpp/fidl.h"

namespace content {
class BrowserContext;
}

namespace webrunner {

// Base test class used for testing the WebRunner Context FIDL service in
// integration.
class WebRunnerBrowserTest : public content::BrowserTestBase {
 public:
  WebRunnerBrowserTest();
  ~WebRunnerBrowserTest() override;

  // Sets the Context client channel which will be bound to a Context FIDL
  // object by WebRunnerBrowserTest.
  static void SetContextClientChannel(zx::channel channel);

  // Gets the client object for the Context service.
  chromium::web::ContextPtr& context() { return context_; }

  // Gets the underlying ContextImpl service instance.
  ContextImpl* context_impl() const;

  // content::BrowserTestBase implementation.
  void PreRunTestOnMainThread() override;
  void PostRunTestOnMainThread() override;

 private:
  chromium::web::ContextPtr context_;

  DISALLOW_COPY_AND_ASSIGN(WebRunnerBrowserTest);
};

}  // namespace webrunner

#endif  // WEBRUNNER_BROWSER_WEBRUNNER_BROWSER_TEST_H_
