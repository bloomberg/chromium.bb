// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_COMMON_JAVASCRIPT_INJECTION_H_
#define FUCHSIA_RUNNERS_COMMON_JAVASCRIPT_INJECTION_H_

#include <fuchsia/mem/cpp/fidl.h>

#include "base/files/file_path.h"

namespace chromium {
namespace web {
class Frame;
}
}  // namespace chromium

// Injects the JavaScript file located at |path| on every page load.
// Caller is responsible for ensuring that the file at |path| exists.
void InjectJavaScriptFileIntoFrame(const base::FilePath& path,
                                   chromium::web::Frame* frame);

// Injects the |buffer| containing JavaScript code into |frame| on every
// page load.
void InjectJavaScriptBufferIntoFrame(fuchsia::mem::Buffer buffer,
                                     chromium::web::Frame* frame);

#endif  // FUCHSIA_RUNNERS_COMMON_JAVASCRIPT_INJECTION_H_
