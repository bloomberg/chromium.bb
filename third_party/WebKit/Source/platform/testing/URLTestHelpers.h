/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef URLTestHelpers_h
#define URLTestHelpers_h

#include "platform/weborigin/KURL.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLResponse.h"

namespace blink {
namespace URLTestHelpers {

inline blink::KURL toKURL(const std::string& url) {
  WTF::String wtfString(url.c_str());
  return blink::KURL(blink::ParsedURLString, wtfString);
}

// Avoid directly using these methods, instead please use ScopedMockedURL (or
// define a variant of ScopedMockedURL classes if necessary) in new code.
//
// Helper functions for mock URLs. These functions set up the desired URL and
// mimeType, with a 200 OK return status.
// webTestDataPath() or platformTestDataPath() in UnitTestHelpers can be used to
// get the appropriate |basePath| and |filePath| for test data directories.
//  - For the mock URL, fullURL == baseURL + fileName.
//  - For the file path, filePath == basePath + ("/" +) fileName.

// Registers from a base URL and a base file path, and returns a calculated full
// URL.
WebURL registerMockedURLLoadFromBase(
    const WebString& baseURL,
    const WebString& basePath,
    const WebString& fileName,
    const WebString& mimeType = WebString::fromUTF8("text/html"));

// Registers from a full URL and a full file path.
void registerMockedURLLoad(
    const WebURL& fullURL,
    const WebString& filePath,
    const WebString& mimeType = WebString::fromUTF8("text/html"));

// Registers with a custom response.
void registerMockedURLLoadWithCustomResponse(const WebURL& fullURL,
                                             const WebString& filePath,
                                             WebURLResponse);

// Registers a mock URL that returns a 404 error.
void registerMockedErrorURLLoad(const WebURL& fullURL);

}  // namespace URLTestHelpers
}  // namespace blink

#endif
