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
#include "core/dom/custom/CustomElementPendingImport.h"

#include "core/html/HTMLImportChild.h"
#include "core/html/HTMLLinkElement.h"

namespace WebCore {

PassOwnPtr<CustomElementPendingImport> CustomElementPendingImport::create(HTMLImportChild* import)
{
    return adoptPtr(new CustomElementPendingImport(import));
}

CustomElementPendingImport::CustomElementPendingImport(HTMLImportChild* import)
    : m_import(import)
{
}

CustomElementPendingImport::~CustomElementPendingImport()
{
    // Remaining tasks in m_baseElementQueue will be discarded.
    // Such a case only happens when the frame is closed
    // while loading the import.
}

bool CustomElementPendingImport::process(ElementQueue baseQueueId)
{
    m_baseElementQueue.dispatch(baseQueueId);
    return false;
}

CustomElementBaseElementQueue* CustomElementPendingImport::parentBaseElementQueue() const
{
    CustomElementPendingImport* parentPendingImport = m_import->parent()->pendingImport();
    if (!parentPendingImport)
        return 0;
    return &parentPendingImport->baseElementQueue();
}

} // namespace WebCore
