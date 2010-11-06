// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_OTHER_DEFINES
#define NET_TOOLS_FLIP_SERVER_OTHER_DEFINES
#pragma once

class NullStream {
 public:
  NullStream() {}
  template <typename T>
  NullStream operator<<(T t) { return *this;}
};

#define VLOG(X) NullStream()
#define DVLOG(X) NullStream()


#endif  // NET_TOOLS_FLIP_SERVER_OTHER_DEFINES

