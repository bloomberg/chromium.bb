// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteSecurityContext_h
#define RemoteSecurityContext_h

#include "core/CoreExport.h"
#include "core/dom/SecurityContext.h"
#include "platform/heap/Handle.h"

namespace blink {

class CORE_EXPORT RemoteSecurityContext : public RefCountedWillBeGarbageCollectedFinalized<RemoteSecurityContext>, public SecurityContext {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(RemoteSecurityContext);
public:
    DECLARE_VIRTUAL_TRACE();

    static PassRefPtrWillBeRawPtr<RemoteSecurityContext> create();
    void setReplicatedOrigin(PassRefPtr<SecurityOrigin>);

    // FIXME: implement
    void didUpdateSecurityOrigin() override { }

private:
    RemoteSecurityContext();
};

} // namespace blink

#endif // RemoteSecurityContext_h
