// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorMutatorClient_h
#define CompositorMutatorClient_h

#include "platform/PlatformExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebCompositorMutatorClient.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class CompositorMutator;
struct CompositorMutations;
class CompositorMutationsTarget;

class PLATFORM_EXPORT CompositorMutatorClient : public WebCompositorMutatorClient {
public:
    CompositorMutatorClient(CompositorMutator*, CompositorMutationsTarget*);
    virtual ~CompositorMutatorClient();

    void setNeedsMutate();

    // cc::LayerTreeMutator
    bool Mutate(base::TimeTicks monotonicTime) override;
    void SetClient(cc::LayerTreeMutatorClient*) override;
    base::Closure TakeMutations() override;

    CompositorMutator* mutator() { return m_mutator.get(); }

    void setMutationsForTesting(PassOwnPtr<CompositorMutations>);
private:
    cc::LayerTreeMutatorClient* m_client;
    CompositorMutationsTarget* m_mutationsTarget;
    Persistent<CompositorMutator> m_mutator;
    OwnPtr<CompositorMutations> m_mutations;
};

} // namespace blink

#endif // CompositorMutatorClient_h
