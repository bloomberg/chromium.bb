// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_H__
#define UTIL_H__

namespace playground2 {

class Util {
 public:
  static bool sendFds(int transport, const void *buf, size_t len, ...);
  static bool getFds(int transport, void *buf, size_t *len, ...);
  static void closeAllBut(int fd, ...);
};

}  // namespace

#endif  // UTIL_H__
