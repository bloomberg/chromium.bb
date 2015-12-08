// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItemClient_h
#define DisplayItemClient_h

#include "platform/PlatformExport.h"
#include "wtf/text/WTFString.h"

namespace blink {

// The interface for objects that can be associated with display items.
class PLATFORM_EXPORT DisplayItemClient {
public:
    virtual ~DisplayItemClient() { }

    virtual String debugName() const = 0;
};

inline bool operator==(const DisplayItemClient& client1, const DisplayItemClient& client2) { return &client1 == &client2; }
inline bool operator!=(const DisplayItemClient& client1, const DisplayItemClient& client2) { return &client1 != &client2; }

}

#endif // DisplayItemClient_h
