// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/common/javascript_injection.h"

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/fidl/chromium/web/cpp/fidl.h"

void InjectJavaScriptFileIntoFrame(const base::FilePath& path,
                                   chromium::web::Frame* frame) {
  fuchsia::mem::Buffer script_buf = cr_fuchsia::MemBufferFromFile(
      base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ));
  CHECK(script_buf.vmo);

  InjectJavaScriptBufferIntoFrame(std::move(script_buf), frame);
}

void InjectJavaScriptBufferIntoFrame(fuchsia::mem::Buffer buffer,
                                     chromium::web::Frame* frame) {
  // Configure the bundle to be re-injected every time the |frame_| content is
  // loaded, regardless of origin.
  frame->ExecuteJavaScript(
      {"*"}, std::move(buffer), chromium::web::ExecuteMode::ON_PAGE_LOAD,
      [](bool success) { CHECK(success) << "JavaScript injection error."; });
}
