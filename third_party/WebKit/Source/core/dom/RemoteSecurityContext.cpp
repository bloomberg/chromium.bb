// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/RemoteSecurityContext.h"

#include "core/frame/csp/ContentSecurityPolicy.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

RemoteSecurityContext::RemoteSecurityContext()
    : SecurityContext()
{
    // RemoteSecurityContext's origin is expected to stay uninitialized until
    // we set it using replicated origin data from the browser process.
    ASSERT(!haveInitializedSecurityOrigin());

    // CSP will not be replicated for RemoteSecurityContexts, as it is moving
    // to the browser process.  For now, initialize CSP to a default
    // locked-down policy.
    setContentSecurityPolicy(ContentSecurityPolicy::create());

    // FIXME: Document::initSecurityContext has a few other things we may
    // eventually want here, such as enforcing a setting to
    // grantUniversalAccess().
}

PassRefPtr<RemoteSecurityContext> RemoteSecurityContext::create()
{
    return adoptRef(new RemoteSecurityContext());
}

void RemoteSecurityContext::setReplicatedOrigin(PassRefPtr<SecurityOrigin> origin)
{
    // FIXME: Currently, replicated security origins are passed only at
    // RemoteFrame creation time.  Eventually, this class will also need to
    // handle origin updates to handle cases like setting document.domain, but
    // for now, check that this is the first and only time we are setting the
    // origin.
    ASSERT(!haveInitializedSecurityOrigin());
    setSecurityOrigin(origin);
}


} // namespace blink
