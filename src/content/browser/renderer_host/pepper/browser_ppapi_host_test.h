// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_BROWSER_PPAPI_HOST_TEST_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_BROWSER_PPAPI_HOST_TEST_H_

#include <memory>

#include "base/macros.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "ppapi/proxy/resource_message_test_sink.h"

namespace content {

class BrowserPpapiHostImpl;

// Test harness for testing Pepper resource hosts in the browser. This will
// construct a BrowserPpapiHost connected to a test sink for testing messages.
class BrowserPpapiHostTest {
 public:
  BrowserPpapiHostTest();
  virtual ~BrowserPpapiHostTest();

  ppapi::proxy::ResourceMessageTestSink& sink() { return sink_; }
  BrowserPpapiHost* GetBrowserPpapiHost();

 private:
  ppapi::proxy::ResourceMessageTestSink sink_;

  std::unique_ptr<BrowserPpapiHostImpl> ppapi_host_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPpapiHostTest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_BROWSER_PPAPI_HOST_TEST_H_
