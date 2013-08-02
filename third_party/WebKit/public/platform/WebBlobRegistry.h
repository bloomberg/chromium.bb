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

#ifndef WebBlobRegistry_h
#define WebBlobRegistry_h

#include "WebCommon.h"

namespace WebKit {

class WebBlobData;
class WebString;
class WebThreadSafeData;
class WebURL;

class WebBlobRegistry {
public:
    WEBKIT_EXPORT static WebBlobRegistry* create();

    virtual ~WebBlobRegistry() { }

    // Registers a blob URL referring to the specified blob data.
    virtual void registerBlobURL(const WebURL&, WebBlobData&) = 0;

    // Registers a blob URL referring to the blob data identified by the
    // specified srcURL.
    virtual void registerBlobURL(const WebURL&, const WebURL& srcURL) = 0;

    // Unregisters a blob referred by the URL.
    virtual void unregisterBlobURL(const WebURL&) = 0;

    // Registers a stream URL referring to a stream with the specified media
    // type.
    virtual void registerStreamURL(const WebURL&, const WebString&) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Registers a stream URL referring to the stream identified by the
    // specified srcURL.
    virtual void registerStreamURL(const WebURL&, const WebURL& srcURL) { WEBKIT_ASSERT_NOT_REACHED(); };

    // Add data to the stream referred by the URL.
    virtual void addDataToStream(const WebURL&, WebThreadSafeData&) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Tell the registry that this stream won't receive any more data.
    virtual void finalizeStream(const WebURL&) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Unregisters a stream referred by the URL.
    virtual void unregisterStreamURL(const WebURL&) { WEBKIT_ASSERT_NOT_REACHED(); }
};

} // namespace WebKit

#endif // WebBlobRegistry_h
