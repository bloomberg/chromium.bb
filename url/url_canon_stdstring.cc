// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/url_canon_stdstring.h"

namespace url_canon {

void StdStringCanonOutput::Resize(int sz) {
  str_->resize(sz);
  buffer_ = str_->empty() ? NULL : &(*str_)[0];
  buffer_len_ = sz;
}

}  // namespace url_canon
