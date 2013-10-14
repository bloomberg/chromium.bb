/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DatabaseBackendContext_h
#define DatabaseBackendContext_h

#include "modules/webdatabase/DatabaseContext.h"

namespace WebCore {

class ScriptExecutionContext;
class SecurityOrigin;

// FIXME: There is no real distinction between frontend vs backend contexts in Blink.
// These classes exist as artifacts of when Blink diverged from WebKit and catching work
// in progress to run the database logic in a seperate process as part of WebKit2.
// We always instantiate DatabaseBackendContext and never DatabaseContext.

class DatabaseBackendContext : public DatabaseContext {
public:
    DatabaseContext* frontend();
    SecurityOrigin* securityOrigin() const;
    bool isContextThread() const;

private:
    explicit DatabaseBackendContext(ScriptExecutionContext* scriptContext)
        : DatabaseContext(scriptContext) { }

    friend class DatabaseManager;
};

} // namespace WebCore

#endif // DatabaseBackendContext_h
