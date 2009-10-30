/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#ifndef WebSharedWorkerRepository_h
#define WebSharedWorkerRepository_h

#include "WebCommon.h"
#include "WebVector.h"

namespace WebKit {
    class WebString;
    class WebSharedWorker;
    class WebURL;

    class WebSharedWorkerRepository {
    public:
        // Unique identifier for the parent document of a worker (unique within a given process).
        typedef uintptr_t DocumentID;

        // Connects the passed SharedWorker object with the specified worker thread.
        // Caller is responsible for freeing the returned object. Returns null if a SharedWorker with that name already exists but with a different URL.
        virtual WebSharedWorker* lookup(const WebURL&, const WebString&, DocumentID) = 0;

        // Invoked when a document has been detached. DocumentID can be re-used after documentDetached() is invoked.
        virtual void documentDetached(DocumentID) = 0;

        // Returns true if the passed document is associated with any SharedWorkers.
        virtual bool hasSharedWorkers(DocumentID) = 0;
    };

} // namespace WebKit

#endif // WebSharedWorkerRepository_h
