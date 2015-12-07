// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebNFCPushTarget_h
#define WebNFCPushTarget_h

namespace blink {

// Enumeration that is mapped to NFCPushTarget as specified in IDL.
enum class WebNFCPushTarget {
    Tag,
    Peer,
    Any,
    ENUM_MAX_VALUE = Any
};

} // namespace blink

#endif // WebNFCPushTarget_h
