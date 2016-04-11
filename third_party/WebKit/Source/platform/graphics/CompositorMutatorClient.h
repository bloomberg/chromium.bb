// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorMutatorClient_h
#define CompositorMutatorClient_h

#include "platform/PlatformExport.h"
#include "public/platform/WebCompositorMutatorClient.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

struct CompositorMutations;
class CompositorMutationsTarget;

class PLATFORM_EXPORT CompositorMutatorClient : public WebCompositorMutatorClient {
public:
    CompositorMutatorClient(CompositorMutationsTarget*);
    virtual ~CompositorMutatorClient();

    // cc::LayerTreeMutator
    base::Closure TakeMutations() override;

    void setMutationsForTesting(PassOwnPtr<CompositorMutations>);
private:
    CompositorMutationsTarget* m_mutationsTarget;
    OwnPtr<CompositorMutations> m_mutations;
};

} // namespace blink

#endif // CompositorMutatorClient_h
