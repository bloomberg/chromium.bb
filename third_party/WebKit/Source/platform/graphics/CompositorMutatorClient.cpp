// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CompositorMutatorClient.h"

#include "base/bind.h"
#include "base/callback.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/CompositorMutation.h"
#include "platform/graphics/CompositorMutationsTarget.h"
#include "platform/graphics/CompositorMutator.h"

namespace blink {

CompositorMutatorClient::CompositorMutatorClient(CompositorMutator* mutator, CompositorMutationsTarget* mutationsTarget)
    : m_client(nullptr)
    , m_mutationsTarget(mutationsTarget)
    , m_mutator(mutator)
    , m_mutations(nullptr)
{
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"), "CompositorMutatorClient::CompositorMutatorClient");
}

CompositorMutatorClient::~CompositorMutatorClient()
{
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"), "CompositorMutatorClient::~CompositorMutatorClient");
}

bool CompositorMutatorClient::Mutate(
    base::TimeTicks monotonicTime)
{
    TRACE_EVENT0("compositor-worker", "CompositorMutatorClient::Mutate");
    double monotonicTimeNow = (monotonicTime - base::TimeTicks()).InSecondsF();
    if (!m_mutations)
        m_mutations = adoptPtr(new CompositorMutations);
    bool shouldReinvoke = m_mutator->mutate(monotonicTimeNow);
    return shouldReinvoke;
}

void CompositorMutatorClient::SetClient(cc::LayerTreeMutatorClient* client)
{
    TRACE_EVENT0("compositor-worker", "CompositorMutatorClient::SetClient");
    m_client = client;
    setNeedsMutate();
}

base::Closure CompositorMutatorClient::TakeMutations()
{
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"), "CompositorMutatorClient::TakeMutations");
    if (!m_mutations)
        return base::Closure();

    return base::Bind(&CompositorMutationsTarget::applyMutations,
        base::Unretained(m_mutationsTarget),
        base::Owned(m_mutations.leakPtr()));
}

void CompositorMutatorClient::setNeedsMutate()
{
    TRACE_EVENT0("compositor-worker", "CompositorMutatorClient::setNeedsMutate");
    m_client->SetNeedsMutate();
}

void CompositorMutatorClient::setMutationsForTesting(PassOwnPtr<CompositorMutations> mutations)
{
    m_mutations = std::move(mutations);
}

} // namespace blink
