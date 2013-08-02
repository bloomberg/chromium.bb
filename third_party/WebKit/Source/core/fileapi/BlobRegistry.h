/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef BlobRegistry_h
#define BlobRegistry_h

#include "wtf/Forward.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"

namespace WebCore {

class BlobData;
class KURL;
class RawData;
class SecurityOrigin;

// A bridging class for calling WebKit::WebBlobRegistry methods on the main
// thread. Each method gets the WebBlobRegistry implementation instance and
// calls the corresponding method (the same name) on it using callOnMainThread()
// if necessary.
class BlobRegistry {
public:
    // Methods for controlling Blobs.
    static void registerBlobURL(const KURL&, PassOwnPtr<BlobData>);
    static void registerBlobURL(SecurityOrigin*, const KURL&, const KURL& srcURL);
    static void unregisterBlobURL(const KURL&);

    // Methods for controlling Streams.
    static void registerStreamURL(const KURL&, const String&);
    static void registerStreamURL(SecurityOrigin*, const KURL&, const KURL& srcURL);
    static void addDataToStream(const KURL&, PassRefPtr<RawData>);
    static void finalizeStream(const KURL&);
    static void unregisterStreamURL(const KURL&);
};

} // namespace WebCore

#endif // BlobRegistry_h
