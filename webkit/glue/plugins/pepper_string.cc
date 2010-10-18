// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_string.h"

namespace pepper {

String::String(const char* str, uint32 len) : value_(str, len) {}

String::~String() {}

}  // namespace pepper
