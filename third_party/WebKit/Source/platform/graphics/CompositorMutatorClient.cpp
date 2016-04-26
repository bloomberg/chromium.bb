// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CompositorMutatorClient.h"

#include "base/bind.h"
#include "base/callback.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/CompositorMutation.h"
#include "platform/graphics/CompositorMutationsTarget.h"

namespace blink {

CompositorMutatorClient::CompositorMutatorClient(CompositorMutationsTarget* mutationsTarget)
    : m_mutationsTarget(mutationsTarget)
    , m_mutations(nullptr)
{
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"), "CompositorMutatorClient::CompositorMutatorClient");
}

CompositorMutatorClient::~CompositorMutatorClient()
{
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"), "CompositorMutatorClient::~CompositorMutatorClient");
}

base::Closure CompositorMutatorClient::TakeMutations()
{
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"), "CompositorMutatorClient::TakeMutations");
    if (!m_mutations)
        return base::Closure();

    return base::Bind(&CompositorMutationsTarget::applyMutations,
        base::Unretained(m_mutationsTarget),
        base::Owned(m_mutations.release().leakPtr()));
}

void CompositorMutatorClient::setMutationsForTesting(PassOwnPtr<CompositorMutations> mutations)
{
    m_mutations = std::move(mutations);
}

} // namespace blink
