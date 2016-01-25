// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItemClient_h
#define DisplayItemClient_h

#include "platform/PlatformExport.h"
#include "platform/geometry/IntRect.h"
#include "wtf/text/WTFString.h"

namespace blink {

// The interface for objects that can be associated with display items.
// A DisplayItemClient object should live at least longer than the document cycle
// in which its display items are created during painting.
// After the document cycle, a pointer/reference to DisplayItemClient should be
// no longer dereferenced unless we can make sure the client is still valid.
class PLATFORM_EXPORT DisplayItemClient {
public:
#if ENABLE(ASSERT)
    DisplayItemClient();
    virtual ~DisplayItemClient();
#else
    virtual ~DisplayItemClient() { }
#endif

    virtual String debugName() const = 0;

    // The visual rect of this DisplayItemClient, in the space of its containing GraphicsLayer.
    virtual IntRect visualRect() const = 0;

#if ENABLE(ASSERT)
    // Tests if a DisplayItemClient object has been created and has not been deleted yet.
    static bool isAlive(const DisplayItemClient&);
#endif
};

inline bool operator==(const DisplayItemClient& client1, const DisplayItemClient& client2) { return &client1 == &client2; }
inline bool operator!=(const DisplayItemClient& client1, const DisplayItemClient& client2) { return &client1 != &client2; }

} // namespace blink

#endif // DisplayItemClient_h
