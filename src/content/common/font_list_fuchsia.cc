// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/font_list.h"

#include "base/values.h"

namespace content {

std::unique_ptr<base::ListValue> GetFontList_SlowBlocking() {
  return std::unique_ptr<base::ListValue>(new base::ListValue);
}

}  // namespace content
