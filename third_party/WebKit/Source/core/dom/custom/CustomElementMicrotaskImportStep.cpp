/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/dom/custom/CustomElementMicrotaskImportStep.h"

#include "core/dom/custom/CustomElementMicrotaskDispatcher.h"
#include "core/dom/custom/CustomElementMicrotaskQueue.h"
#include <stdio.h>

namespace WebCore {

PassOwnPtr<CustomElementMicrotaskImportStep> CustomElementMicrotaskImportStep::create(PassRefPtr<CustomElementMicrotaskQueue> queue)
{
    return adoptPtr(new CustomElementMicrotaskImportStep(queue));
}

CustomElementMicrotaskImportStep::CustomElementMicrotaskImportStep(PassRefPtr<CustomElementMicrotaskQueue> queue)
    : m_importFinished(false)
    , m_queue(queue)
{
}

CustomElementMicrotaskImportStep::~CustomElementMicrotaskImportStep()
{
}

void CustomElementMicrotaskImportStep::importDidFinish()
{
    // imports should only "finish" once
    ASSERT(!m_importFinished);
    m_importFinished = true;
    CustomElementMicrotaskDispatcher::instance().importDidFinish(this);
}

CustomElementMicrotaskStep::Result CustomElementMicrotaskImportStep::process()
{
    Result result = m_queue->dispatch();
    if (!m_importFinished)
        result = Result(result | ShouldStop);
    return result;
}

#if !defined(NDEBUG)
void CustomElementMicrotaskImportStep::show(unsigned indent)
{
    fprintf(stderr, "indent: %d\n", indent);
    fprintf(stderr, "%*sImport\n", indent, "");
    m_queue->show(indent + 1);
}
#endif

} // namespace WebCore
