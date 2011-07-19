// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/completion_callback.h"

namespace pp {

CompletionCallback BlockUntilComplete() {
  return CompletionCallback();
}

}  // namespace pp
