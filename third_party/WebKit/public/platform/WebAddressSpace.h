// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebAddressSpace_h
#define WebAddressSpace_h

namespace blink {

// The ordering is important, as it's used to determine whether preflights are
// required, as per https://wicg.github.io/cors-rfc1918/#framework
enum WebAddressSpace {
  kWebAddressSpaceLocal = 0,  // loopback, link local
  kWebAddressSpacePrivate,    // Reserved by RFC1918
  kWebAddressSpacePublic,     // Everything else
  kWebAddressSpaceLast = kWebAddressSpacePublic
};

}  // namespace blink

#endif  // WebAddressSpace_h
