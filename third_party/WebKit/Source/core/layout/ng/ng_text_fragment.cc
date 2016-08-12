// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_text_fragment.h"

namespace blink {

String NGTextFragment::text() const {
  return text_list_->Text(start_offset_, end_offset_);
}

}  // namespace blink
