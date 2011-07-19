// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_CACHEINVALIDATION_MD5_H_
#define GOOGLE_CACHEINVALIDATION_MD5_H_

#include "base/md5.h"

namespace invalidation {

inline void ComputeMd5Digest(const string& data, string* digest) {
  *digest = MD5String(data);
}

}  // namespace invalidation

#endif  // GOOGLE_CACHEINVALIDATION_MD5_H_
