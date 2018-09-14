// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/browser/webrunner_browser_test.h"

#include "base/base_paths_fuchsia.h"
#include "base/files/file_util.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "webrunner/browser/webrunner_browser_context.h"
#include "webrunner/browser/webrunner_browser_main_parts.h"
#include "webrunner/browser/webrunner_content_browser_client.h"
#include "webrunner/service/common.h"
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

  ConfigurePersistentDataDirectory();
}

void WebRunnerBrowserTest::PostRunTestOnMainThread() {
  // Unbind the Context while the message loops are still alive.
  context_.Unbind();
}

void WebRunnerBrowserTest::ConfigurePersistentDataDirectory() {
  // TODO(https://crbug.com/840598): /data runs on minfs, which doesn't
  // support mmap() and therefore is incompatible with the disk cache and
  // other components that require memory-mapped I/O. Use tmpfs temporarily,
  // even though it isn't actually persisted.
  base::FilePath temp_dir;
  CHECK(base::PathService::Get(base::DIR_TEMP, &temp_dir));

  base::ScopedAllowBlockingForTesting allow_blocking;
  CHECK(base::PathExists(temp_dir));
  base::SetPersistentDataPath(temp_dir);
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
