// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/browser/webrunner_browser_test.h"

#include "base/fuchsia/fuchsia_logging.h"
#include "webrunner/browser/webrunner_browser_context.h"
#include "webrunner/browser/webrunner_browser_main_parts.h"
#include "webrunner/browser/webrunner_content_browser_client.h"
#include "webrunner/service/webrunner_main_delegate.h"

namespace webrunner {
namespace {

zx_handle_t g_context_channel = ZX_HANDLE_INVALID;

}  // namespace

WebRunnerBrowserTest::WebRunnerBrowserTest() = default;

WebRunnerBrowserTest::~WebRunnerBrowserTest() = default;

void WebRunnerBrowserTest::PreRunTestOnMainThread() {
  zx_status_t result = context_.Bind(zx::channel(g_context_channel));
  ZX_DCHECK(result == ZX_OK, result) << "Context::Bind";
  g_context_channel = ZX_HANDLE_INVALID;

  embedded_test_server()->ServeFilesFromSourceDirectory(
      "webrunner/browser/test/data");
}

void WebRunnerBrowserTest::PostRunTestOnMainThread() {
  // Unbind the Context while the message loops are still alive.
  context_.Unbind();
}

// static
void WebRunnerBrowserTest::SetContextClientChannel(zx::channel channel) {
  DCHECK(channel);
  g_context_channel = channel.release();
}

ContextImpl* WebRunnerBrowserTest::context_impl() const {
  return WebRunnerMainDelegate::GetInstanceForTest()
      ->browser_client()
      ->main_parts_for_test()
      ->context();
}

}  // namespace webrunner
