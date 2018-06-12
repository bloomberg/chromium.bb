// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/string_extras.h"

#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace WTF {

unsigned strlen_unsigned(const char* string) {
  return SafeCast<unsigned>(strlen(string));
}

}  // namespace WTF
