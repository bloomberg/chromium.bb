// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LinkManifest_h
#define LinkManifest_h

#include "core/html/LinkResource.h"
#include "wtf/Allocator.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class HTMLLinkElement;

class LinkManifest final : public LinkResource {
    USING_FAST_MALLOC_WILL_BE_REMOVED(LinkManifest);
public:

    static PassOwnPtrWillBeRawPtr<LinkManifest> create(HTMLLinkElement* owner);

    ~LinkManifest() override;

    // LinkResource
    void process() override;
    Type type() const override { return Manifest; }
    bool hasLoaded() const override;
    void ownerRemoved() override;

private:
    explicit LinkManifest(HTMLLinkElement* owner);
};

} // namespace blink

#endif // LinkManifest_h
