// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/base/gcm_util.h"

#include "net/base/escape.h"

namespace gcm {

void BuildFormEncoding(const std::string& key,
                       const std::string& value,
                       std::string* out) {
  if (!out->empty())
    out->append("&");
  out->append(key + "=" + net::EscapeUrlEncodedData(value, true));
}

}  // namespace gcm
