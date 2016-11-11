// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_ATTRIBUTES_H_
#define REMOTING_HOST_HOST_ATTRIBUTES_H_

#include <string>

namespace remoting {

// Returns a comma-separated string to represent host attributes. The result may
// vary if any system configurations change. So consumers should not cache the
// result.
// This function is thread-safe.
std::string GetHostAttributes();

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_ATTRIBUTES_H_
